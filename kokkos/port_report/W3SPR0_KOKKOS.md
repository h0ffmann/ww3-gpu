# W3SPR0_KOKKOS

**Status:** ported, phase 1 (translated). Wired: WW3 at the `WW3/` pin (`357f575c`) calls it at `w3srcemd.F90:1086, :2201` behind `WW_KOKKOS_SPR0` (default off); not regtested: compiled, never run inside WW3 (the owner, 2026-10-16).

Mean wave parameters of one spectral point for the case without input and dissipation (switch
`ST0`): the mean energy, the mean frequency and the mean wavenumber, with the high-frequency tail
`FTE`, `FTF`, `FTWN`, and the largest action density. `W3SRCE` calls it per sea point before the
integration (`w3srcemd.F90:1034`) and in the loop of dynamic time steps (`w3srcemd.F90:1863`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3SPR0`, `model/src/w3src0md.F90:81-231` at NOAA-EMC/WW3 `03adda91`; the switch `ST0` is in 355 regtest switch files of 47 directories under `regtests/` (`git grep -lw ST0 -- 'regtests/*switch*'`; among them `ww3_tp1.*`, `ww3_tp2.*`, `mww3_test_0*`, `ww3_tic*`, `ww3_ts1`, `ww3_ts4`), in no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`). Its only `#ifdef` blocks are `W3_S`/`W3_T` output: no variant changes its arithmetic |
| Kokkos code | [`../src/ww_kokkos/srce_st0_spr0.hpp`](../src/ww_kokkos/srce_st0_spr0.hpp), `ww::srce::st0::w3spr0_point`; the automatic array `EB(NK)` (`:172`, a `malloc` in the object) is a caller-provided work array |
| C shim | none yet; it passes `SIG(1:NK)`, `DDEN(1:NK)`, `FTE`, `FTF`, `FTWN` of the current grid |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3src0md.F90` under `switch_st0` (`switch_lab_shrd` with `ST0 FLX2
LN0`, WW3's CMake Release flags, gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno`
and checked identical to the CMake object; compared with a C++ unit under the `openmp-release`
flags plus `-ffp-contract=off`, operation by operation, 2026-10-16.

- (v) The `ITH` loop (`:189-192`) is vectorised by gfortran when `NTH >= 8` (and `NTH /= HUGE`):
  `EB(IK)` stays an in-order sum (lane by lane `addss`), `AMAX` becomes four `maxps` lanes seeded
  with the running `AMAX`, reduced as `max(max(m3,m1), max(m2,m0))`, then the `NTH mod 4` tail in
  order; below 8 it is one scalar loop. The header reproduces both paths and every operand order
  (`(d > s) ? d : s`, `d` the destination); the C++ object has the same `vmaxss` operand order at
  each site. The lanes only matter with a NaN or a signed zero in `A`.
- (v) The `IK` loop (`:197-202`) is vectorised (4 lanes, a 2-lane epilogue, an alias-versioned
  scalar copy), but every sum is in order and each element is `(EB*DDEN)/CG`, `/SIG`,
  `/SQRT(WN)` with IEEE `divps`/`sqrtps`; the C++ is the scalar loop, the same bits.
- (v) Tail and final processing (`:207-215`): `EBAND = EB(NK)/DDEN(NK)`, `X + EBAND*FT`, then
  `(TPIINV*EMEAN)/MAX(1.E-7,FMEAN)` and `(EMEAN/MAX(1.E-7,WNMEAN))**2` as one multiply. Both `MAX`
  are `maxss X,1e-7`: a NaN `X` gives `1e-7`. `TPIINV` is `0x3e22f983`, `1.E-7` is `0x33d6bf95` in
  both objects' `.rodata`.
- (v) No FMA, no libm call in the Fortran (`sqrtss`/`sqrtps` inline). The C++ uses `vsqrtss` and
  calls glibc `sqrtf` only for a negative `WN` (for `errno`) ⚠, whose NaN is not compared here.
- ⚠ NaN payloads: some Fortran operations have their operands the other way round from the C++
  (`EB/SQRT(WN) + WNMEAN` in the 4-lane body; all three sums in the scalar copy taken when
  `NK = 1` or the alias check fails; `DDEN*EB` in the vector bodies; `FTx*EBAND`); the result
  differs only when both operands are NaN.
- ⚠ `NK <= 0` makes the object read `EB(0)` outside its allocation (`:207`); the header requires
  `nk >= 1`, as every WW3 grid has.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`), never run. The PTX has no `fma`; the SASS `FFMA` come
  from the `div.rn`/`sqrt.rn` sequences. nvcc turned the two final `MAX` into `max.f32`, the same
  result for a constant that is neither NaN nor zero; the `AMAX` sites stay `setp.gt` and select.
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3src0md.F90` from
  `build_st0`'s `flags.make` (objdump identical to the CMake object) and the C++ unit under the
  `openmp-release` flags (warning-free, no FMA); read both GIMPLE dumps and the assembly site by
  site: the `maxps`/`movhlps`/`shufps` reduction of `AMAX`, every `maxss` destination, the in-order
  `addss` chains, `.rodata` constants, `sqrtss`/`sqrtps` with no libm call. The only `#ifdef`s are
  `W3_S`/`W3_T`. Fixed the NaN-payload bullet above, which placed the reversed operands in the
  2-lane epilogue; they are in the 4-lane body and the scalar copy.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
