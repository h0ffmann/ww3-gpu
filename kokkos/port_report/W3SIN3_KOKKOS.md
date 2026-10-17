# W3SIN3_KOKKOS

**Status:** ported, phase 1 (translated). Wired: WW3 at the `WW3/` pin (`357f575c`) calls it at `w3srcemd.F90:1155, :1467, :2418` behind `WW_KOKKOS_SIN3` (default off); not regtested: compiled, never run inside WW3 (the owner, 2026-10-16).

The WAM4+ wind input source term of Janssen (1991) with Bidlot's extensions (switch `ST3`): the
diagonal term `D`, `S = D*A`, the resolved and negative wave-supported stresses, and the
high-frequency stress read from the table `TAUHFT`. `W3SRCE` calls it per sea point
(`w3srcemd.F90:1059-1061`, `:1234-1236`, `:2002-2004`), passing `WN2` as `K` and `TAUWAX`, `TAUWAY`
as `TAUWNX`, `TAUWNY`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SIN3`, `model/src/w3src3md.F90:384-717` at NOAA-EMC/WW3 `03adda91` (334 lines); it reads the module table `TAUHFT(0:100,0:200)` and the steps `DELUST`, `DELALP` (`:96-99`) that `TABU_TAUHF` builds. `ST3` is in 47 regtest switch files (`mww3_test_05` 20, `ww3_ts3` 20, `ww3_ts2` 5, `ww3_ts1` 1, `ww3_tnc1` 1; `git -C WW3-noaa grep -lw ST3 -- 'regtests/*switch*'`), none with `STAB3` (one names `STAB0`, the rest no `STAB` switch, which compiles the same code); no `model/bin/switch_NCEP_*` or `switch_NWS_*` has `ST3` |
| Kokkos code | [`../src/ww_kokkos/srce_st3_sin3.hpp`](../src/ww_kokkos/srce_st3_sin3.hpp), `ww::srce::st3::w3sin3_point<kStab3>`; `false` is `STAB0`, `true` the `W3_STAB3` blocks (`:557-560`, `:563-567`, `:642-649`). The automatic array `DSTAB(3,NSPEC)` (`:526`, a `malloc` per call in the object) is caller-provided work, `sin3_work_reals(p)` reals, Fortran layout |
| Table | `tauhft` is `TAUHFT` flat in Fortran column-major order, `TAUHFT(i,j)` at `i + 101*j`, as `TABU_TAUHF`'s port (`srce_st3_tabu_tauhf.hpp`, `TauhfSteps` for `DELUST`, `DELALP`) builds it; this header only reads it |
| C shim | none yet; it passes `SIG` from `SIG(1)`, `SIG2`, `DDEN2`, `ECOS`, `ESIN`, the `W3GDATMD` scalars `DTH`, `BBETA`, `ZZALP`, `SSWELLF(1)`, `SSINTHP`, `AALPHA`, and the grid's `TAUHFT`, `DELUST`, `DELALP` |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` objects of `w3src3md.F90` (WW3's CMake Release flags, gfortran 16.2) under
`ST3 STAB0 FLX0` and `ST3 STAB3 FLX0` (both `switch_lab_shrd` with `ST4` replaced), recompiled in
scratch and byte-identical in their disassembly to the CMake builds' objects; compared with a
throwaway C++ unit instantiating both variants, built with the `openmp-release` flags (g++ 16.2,
`-O3 -march=x86-64-v3 -ffp-contract=off`), warning-free, by GIMPLE and assembly, 2026-10-16.

- (v) Every floating-point statement in the Fortran's order, operands only swapped across a
  commutative multiply or add: `CONST0 = (BBETA*DRAT)/KAPPA**2`, `CONST3 = ((SSWELLF(1)*2)*KAPPA)*DRAT`,
  `CM`, `UCN = UST*CM+ZZALP`, `CONST2 = ((DDEN2/CG)*GRAV)/((SIG/K)*DRAT)`, `ZCN = LOG(K*Z0)`,
  `COSWIND`, `ZBETA = (CONST3*(COSWIND+KAPPA/((ZCN*UST)*CM)))*UCN**2`, the Janssen branch
  `(((CONST*EXP(ZLOG))*ZLOG**4)*UCN**2)*COSWIND**SSINTHP`, `TEMP2 = (CONST2*DSTAB)*A`; the tail
  `CONST0 = ((((DTH*SIG**5)/(GRAV**2*TPI))*TPI)*SIG)/CG(NK)`, `TAU1` as the Fortran's bilinear sum,
  `TAUHF = ((CONST0*TEMP)*UST**2)*TAU1`, the cap at `:699-705`. Powers as the object's trees:
  `UCN**2` `u*u`, `ZLOG**4` `(z*z)*(z*z)`, `SIG**5` `(s*(s*s))*(s*s)`, `MAX(COSWIND,0)**3` `m*(m*m)`.
  Folded constants with the object's bits: `KAPPA**2` 0x3e23d70b, `GRAV**2*TPI` 604.17621,
  `EPS2**2` 9.99999996e-13, each with a `static_assert` of how it is built. No FMA in either object.
- (v) libm: both objects call `sincosf` once for `COS(USDIR)`, `SIN(USDIR)` (the second use at
  `:693-694` is the same value), `logf`, `expf` and `powf` (`COSWIND**SSINTHP`, a REAL exponent);
  `SQRT` is `sqrtss`. No libmvec call: the main loop is not vectorised in either object.
- (v) Reductions: `STRESSSTAB` and `STRESSSTABN` sum over `ITH` inside `IK` in order from 0, kept
  in registers by both compilers; the C++ packs the x and y sums into one 2-lane add, the same two
  roundings. `TEMP` sums in order. The vectorised loops (`DSTAB = 0`, `D = DSTAB(3,:)`,
  `D = 0.5*(DSTAB(1,:)+DSTAB(2,:))`, `S = D*A`) are element-wise.
- (v) `MIN`/`MAX` operand order from the assembly, `maxss d, s` giving `s` on a NaN or a tie in `d`:
  `MAX(COSWIND,0)` (`:677`), `MIN(1,XI-IND)` then `MAX(.,0)` (`:684`), `MAX(UST,1E-5)` (`:686`),
  `MAX(XJ,0)` (`:686`), `MIN(1,XJ-J)` then `MAX(.,0)` (`:688`), `MAX(UST2-EPS1,EPS2**2)` and
  `MIN(TAUW, that)` with the `MAX` result as `d` (`:701`), `MAX(-0.025*AS,0)` (`:558`, STAB3). One
  site differs between the variants: `MAX(USTAR,EPS2)` (`:700`) has `EPS2` as `d` under STAB0 and
  `USTAR` as `d` under STAB3, so a NaN `USTAR` gives NaN in one and `EPS2` in the other; the header
  reproduces each (`max_ustar_eps2<kStab3>`). The C++ GIMPLE gives the same result as each site,
  NaN and ties included.
- (v) `INT()` at `:683`, `:687` through `fint` (`st4_params.hpp`), gfortran's `cvttss2si`: NaN or out
  of range gives `INT_MIN`, which the clamps turn into 1, in both objects.
- (v) Variants: `STAB3` translated and checked against its own object. Under it the stresses of
  each pass start from zero, `D = DSTAB(3,:)` (zeros) is still written inside the loop and then
  overwritten, `LLWS` is the second pass's, and the tail uses `UST = USTAR*(1+USTARsigma)`, as the
  Fortran does. `W3_S`, `W3_T`, `W3_T0`, `W3_T1` are tracing and test output, not ported.
  `Z0VISC` (`:549`) and `TAUX`, `TAUY` (`:568-569`) are never read; both compilers drop them.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, `nix develop picasso/labs/pratico#cuda`), never run. The
  SASS has `FFMA`, expected inside CUDA's division and `logf`, `expf`, `powf`, `sincosf`; CUDA's
  are not shown to return glibc's bits, so the device path is not claimed bit for bit.
- (v) Fresh review 2026-10-16: no defect. `w3src3md.F90` recompiled under `build_st3` (STAB0) and
  `build_st3s` (STAB3), disassembly identical to the CMake objects; both instantiations of the
  throwaway C++ unit compared with their object by GIMPLE and assembly: every `minss`/`maxss` site
  (`:558`, `:677`, `:684`, `:686` twice, `:688`, `:700`, `:701`) with the same destination operand,
  including `:700`'s switch-dependent order; `.rodata` constants (`0x3e23d70b`, `0x44170b47`,
  `0x2b8cbccc`, `0x3ccccccd`); powers; `INT` clamps (NaN and out of range give 1); `TAUHFT(IND,J)` at
  `IND + 101*J` in both objects, as `srce_st3_tabu_tauhf.hpp` lays it out; libm calls `sincosf`,
  `logf`, `expf`, `powf` only, no FMA, no libmvec. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
