# W3SDS6_KOKKOS

**Status:** ported, phase 1 (translated). Wired: WW3 at the `WW3/` pin (`357f575c`) calls it at `w3srcemd.F90:1636` behind `WW_KOKKOS_SDS6` (default off); not regtested: compiled, never run inside WW3 (the owner, 2026-10-16).

Observation-based whitecapping dissipation of the ST6 package (switch `ST6`), after Babanin et
al. (2010, JPO 40(4) 667-683) as implemented by Rogers et al. (2012, JTECH 29(9) 1329-1346;
both as the Fortran header cites them, `:592-593`): `Sds = (T1 + T2) E`, with `T1` the inherent breaking term and
`T2` the cumulative term, an integral of the normalised excess spectral density over all lower
frequencies. `W3SRCE` calls it per sea point; the point post-processors call it too.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SDS6`, `model/src/w3src6md.F90:566-770` at NOAA-EMC/WW3 `03adda91`; compiled by every switch with `ST6` (`git grep -lw ST6 -- '*switch*'`): `ww3_ts1`, `ww3_ts2`, `ww3_ts3`, `ww3_tp2.15`, `mww3_test_05`, and `model/bin/switch_NRL3`, `switch_swin`, `switch_UoM_*`; by no `switch_NCEP_*` or `switch_NWS_*` |
| Kokkos code | [`../src/ww_kokkos/srce_st6_sds6.hpp`](../src/ww_kokkos/srce_st6_sds6.hpp), `ww::srce::st6::w3sds6_point`, and the host builder `w3sds6_ikn` |
| Structural changes | the module `SAVE` index vector `IKN` (`:104`; `THREADPRIVATE` at `:107` under `OMPG` only, since the guard `#ifdef W3_OMPG \|\| W3_OMPH` at `:106` tests `W3_OMPG` alone: `gfortran -E -cpp -DW3_OMPH` drops the directive with an "extra tokens" warning), built with `IRANGE(1,NSPEC,NTH)` by whichever of `W3SIN6` (`:449-456`) and `W3SDS6` (`:696-704`) is called first and never rebuilt, so once per process (per thread under `OMPG`) and shared with `W3SIN6`, not once per grid, is a host table `ikn` built by `w3sds6_ikn` with `ww::srce::st6::irange` (`srce_st6_util.hpp`); its `EXTCDE(2)` on `SIZE(IKN) /= NK` (`:700-703`) is the return value 2, checked only by the call that builds `IKN`. The automatic arrays `FREQ`, `DFII`, `EDENS`, `ETDENS`, `EXDENS`, `NEXDENS`, `T1`, `T2`, `T12`, `ADF` (`:674-684`) are caller work, `10*NK` reals |
| Variants | the only `#ifdef` blocks are `W3_S` (tracing) and `W3_T6` (`STME21` and test `WRITE`s, `:756-764`), output only, not translated. `__w3src6md_MOD_w3sds6` is the same instruction for instruction in `build_st6f4` (`ST6 FLX4`) and `build_st6f5` (`ST6 FLX5`); no other switch reaches its arithmetic |
| C shim | none yet; it passes `IKN`, `SIG2(1:NSPEC)`, `DSII(1:NK)`, `DTH`, `XFR`, `SDS6A1`, `SDS6A2`, `SDS6P1`, `SDS6P2`, `SDS6ET` of the current grid |
| Plan | issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3src6md.F90` (WW3's CMake Release flags, gfortran 16.2) from the
reference builds `build_st6f4` and `build_st6f5`, each recompiled with
`-fdump-tree-optimized-lineno` and checked identical to the CMake object. Compared with the `-O3`
GIMPLE and assembly of a throwaway C++ unit (`openmp-release` flags, `-ffp-contract=off`,
warning-free), operation by operation, 2026-10-16.

- (v) `SDS6P1` and `SDS6P2` are `INTEGER` (`w3gdatmd.F90:921`): `NEXDENS**SDS6P1` (`:734`) and
  `NEXDENS**SDS6P2` (`:738`) are `__builtin_powif`, two calls to libgcc's `__powisf2` in the
  object, not `powf`. The C++ calls `__builtin_powif` on the host (two `__powisf2` calls in its
  object) and libgcc's `__powisf2` loop on a device.
- (v) `ETDENS` (`:715`): `TPI*BNT` with `BNT = 0.035**2` folded by gfortran to `0x3bfc364c`, a hex
  literal with a `static_assert` of how it is built; then `c/((ANAR*CG)*(WN*(WN*WN)))`, `WN**3`
  as two `powmult`.
- (v) `EDENS` (`:716`): `RESHAPE` through `_gfortran_reshape_r4`, then the `SUM` over directions
  from `0.0` in index order (a vectorised in-order reduction, lanes added one by one); then
  `(((sum*TPI)*SIG2(IKN))*DTH)/CG` (commuted operands only). `FREQ = SIG2(IKN)/TPI` (`:706`) and
  `DFII = DSII/TPI` (`:741`) are divisions, not products by `TPIINV`.
- (v) `MAX(0.0,EDENS-ETDENS)` (`:717`): `maxps`/`maxss` with the difference in the destination at
  all three sites (vector body, pair, scalar tail), `maxss(diff, 0)`.
- (v) `MAXVAL(EDENS)` (`:723`): the first element with `x >= -Inf` starts the limit, then
  `x > limit ? x : limit`; all NaN gives NaN; times `1E-5` (`0x3727c5ac`). `ALL(EDENS > EDENSMAX)`
  (`:724`) stops at the first false.
- (v) `T1 = ((SDS6A1*ANAR)*FREQ)*NEXDENS**SDS6P1`, `ADF = ANAR*NEXDENS**SDS6P2`,
  `XFAC = (1-1/XFR)/(XFR-1/XFR)`. `ANAR` is 1.0 (`:707`); its products are kept in the C++ and
  g++ folds them, the same bits for every input but a signalling NaN.
- (v) `:740-745` as written: `DFII` recomputed whole for every `IK`, `DFII(IK)*XFAC` for
  `1 < IK < NK`, `SUM(ADF(1:IK)*DFII(1:IK))` from `0.0` in index order (a vectorised in-order
  reduction), `T2 = SDS6A2*sum`. O(NK^2) divisions per point, kept.
- (v) `T12 = -1.0*(MAX(0.0,T1)+MAX(0.0,T2))` (`:748`) is a sign flip (`xorps`) of `m2 + m1`. The
  operand order of `MAX(0.0,T1)` differs between paths: the 4-wide and 2-wide vector loops put
  `T1` in the destination of `maxps` (`T1 > 0 ? T1 : 0`), the scalar tail, taken for the last
  element when `NK` is odd, has `maxss -0x4(%rsi,%rax,4),%xmm1` with `0` in the destination
  (`0 > T1 ? 0 : T1`), so there a NaN `T1` survives and a `-0` stays `-0`. `MAX(0.0,T2)` has
  `T2` in the destination everywhere. The C++ selects the scalar-tail form for that one element.
- (v) `D(IKN+ITH-1) = T12` (`:749-751`) and `S = D*A` (`:753`) as element-wise copies and products.
- (v) No libm or libmvec call in either object; no FMA in the C++ object.
- ⚠ The operand order of commuted products and sums (e.g. `SIG2*(sum*TPI)`, `D*A`) was not
  matched one by one; it changes the result only when both operands are NaN (which payload
  survives).
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, the `#cuda` shell), never run. The `FFMA` in the SASS
  belong to nvcc's IEEE division sequences (`MUFU.RCP` with Newton steps), not checked one by
  one. The `maxss` ternaries may compile to `FMNMX` (107 in the unit's SASS), which returns the
  non-NaN operand where `maxss` returns its source; equal for every finite input but the
  `+0`/`-0` tie.
- ⚠ Because `IKN` is built once per process, a `ww3_multi` run whose ST6 grids differ in `NK` or
  `NTH` reads the first grid's `IKN` on the others in the Fortran (no size check after the first
  build); the port reproduces that only if the shim passes the first-built table, not one per
  grid. (v) `mww3_test_05`, the only `ww3_multi` regtest with `ST6`, has `NK = 25`, `NTH = 24` on
  every grid (`ww3_grid_*.nml`), so there the tables agree. Under `OMPH` without `OMPG`, `IKN`
  is one shared array and its first-call build races between threads; not examined.
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3src6md.F90` from
  `build_st6f4` and `build_st6f5` flags (both identical to the CMake objects; `W3SDS6` the same
  instruction for instruction in both) and the header in a throwaway unit (`openmp-release`
  flags, warning-free); compared GIMPLE and assembly per line: `:715` folded `0x3bfc364c` and
  `WN*(WN*WN)`, `:716` in-order lane sum and operand chain, `:717` `maxss(diff,0)` at all three
  sites, `:723` MAXVAL (`maxss` with the new element in the destination), `:724-729`, `:734`
  and `:738` (two `__powisf2` calls in each object, which resolve to the same libgcc symbol in
  one WW3 executable), `:740-745` (`DFII` recomputed per `IK`, `XFAC` for `1 < IK < NK`,
  in-order sum), `:748` (`T1` slot `-0x360(%rbp)` is the `:734` store, so the scalar tail
  `maxss mem,%xmm1` is `0 > T1 ? 0 : T1`, taken exactly for the last element when `NK` is odd,
  `NK <= 3` included; the C++ object versions the loop on odd `NK` and does that element with
  `vcmpnltss`/`vandps`, the same function). The device branch of `powi`, compiled for the host
  with g++ `-O2`, is instruction for instruction libgcc 16.2's `__powisf2`, multiply order
  `y = y*x` included. Fixed: the header and this report said `IKN` is built once per grid; it is
  built once per process (above). CUDA not recompiled by this review.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
