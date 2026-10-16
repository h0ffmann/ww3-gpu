# BRKDN_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Breakdown test of the stabilised biconjugate gradient solver `BCGSTAB` (`w3profsmd.F90:2049-2370`):
true when a scalar of the iteration (`alpha`, `omega`, `FPAR(7)`) is zero (`IPAR(1) = -3`) or
abnormal (`IPAR(1) = -9`), which ends the iteration (`:2203`, `:2263`, `:2269`, `:2314`).

## What was ported

| | |
|---|---|
| WW3 routine | `BRKDN`, `model/src/w3profsmd.F90:2594-2627` at NOAA-EMC/WW3 `03adda91`, a private `LOGICAL` module function, no `#ifdef` inside it |
| Callers | only `BCGSTAB`, reached through `RUNRC` (`:1239`) from `W3XYPFSNIMP` (`:988`), which `W3XYPUG` calls when `FSNIMP` (`:257-258`), set from the `&UNST` namelist's `IMPFSN` (`w3gridmd.F90:3725`) |
| Which switches compile it | every switch with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:116-125`), among them `switch_NCEP_glwu`, `_gwm`, `_st2`, `_st4`, `_st4sbs`, `switch_NWS_rwps`. The only regtest input that sets `IMPFSN = T` is `ww3_ts4/input_ug` (`ww3_grid.inp:44`, `namelists_Mediterranean.nml:1`), entries `work_ug` and `work_ug_MPI` of `matrix.base` |
| Kokkos code | [`../src/ww_kokkos/profs_brkdn.hpp`](../src/ww_kokkos/profs_brkdn.hpp), `ww::profs::brkdn(const double& alpha, int* ipar)`; `ipar` is `IPAR(16)`, 0-based, only `ipar[0]` is written, and only when the result is true |
| C shim | none yet |
| Plan | issue #42, PR goal/42-port-batch-easy |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd`), recompiled and checked byte-identical in disassembly to the CMake object,
2026-10-16; compared with a throwaway C++ unit built with the `openmp-release` flags, g++ 16.2.

- (v) Same instruction sequence and branch layout in both objects, symbol
  `__w3profsmd_MOD_brkdn` against the C++ wrapper: `comisd` against 0, `1.0/alpha` by `divsd`,
  `comisd` of `beta` against 0, `ucomisd` with the parity branch for `alpha == 0`, stores of
  -9 and -3 to `IPAR(1)`. The only difference is the VEX encoding (`vdivsd`, `vcomisd`) from
  `-march=x86-64-v3`.
- (v) The division is kept by both compilers: `1/alpha` is not `> 0` for `alpha = +Inf`, so
  the test is not foldable. A NaN `alpha` fails all three comparisons and takes the last branch
  (`-9`), in both objects.
- (v) `REAL*8` stays `double`; the `LOGICAL(4)` result becomes `bool` (0 or 1 in `eax` in both).
- (v) No libm or libmvec call, no FMA, no reduction, no MIN/MAX.
- ⚠ Not compiled for CUDA.
- (v) Fresh review 2026-10-16: no defect. `w3profsmd.F90` recompiled from `build_lab`'s `flags.make`,
  disassembly identical to the CMake object; `__w3profsmd_MOD_brkdn` and the C++ wrapper compared
  instruction by instruction (same branches, same `divsd` of the 1.0 constant, same `-9`/`-3` stores,
  same NaN path through `jp`), GIMPLE read at `:2606-2621`; the routine has no `#ifdef`, so `PR1` and
  `PR2` compile the same code. Fixed only the path of the `switches.json` cite.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
