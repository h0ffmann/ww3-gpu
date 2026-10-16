# BISINIT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Common initialisation of the iterative solvers of `W3PROFSMD`: checks the work-array size
(`IPAR(1) = -2`), sets the left and right preconditioning flags from `IPAR(2)`, clears
`IPAR(7:13)`, `FPAR(3:10)` and the first `WKSIZE` elements of the work array, and checks the
tolerances `FPAR(1)`, `FPAR(2)` (`IPAR(1) = -4`, or the defaults `1.0D-6`, `1.0D-16` when
`IPAR(1)` is not 0).

## What was ported

| | |
|---|---|
| WW3 routine | `BISINIT`, `model/src/w3profsmd.F90:2630-2699` at NOAA-EMC/WW3 `03adda91`, a module subroutine, no `#ifdef` inside it |
| Callers | only `BCGSTAB` (`:2333`, `call bisinit(ipar,fpar,8*n,1,lp,rp,w)`), reached through `RUNRC` (`:1239`) from `W3XYPFSNIMP` (`:988`), which `W3XYPUG` calls when `FSNIMP` (`:257-258`), set from the `&UNST` namelist's `IMPFSN` (`w3gridmd.F90:3725`) |
| Which switches compile it | every switch with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:116-125`), among them `switch_NCEP_glwu`, `_gwm`, `_st2`, `_st4`, `_st4sbs`, `switch_NWS_rwps`. The only regtest input that sets `IMPFSN = T` is `ww3_ts4/input_ug`, entries `work_ug` and `work_ug_MPI` of `matrix.base` |
| Kokkos code | [`../src/ww_kokkos/profs_bisinit.hpp`](../src/ww_kokkos/profs_bisinit.hpp), `ww::profs::bisinit(int* ipar, double* fpar, int wksize, int dsc, bool& lp, bool& rp, double* wk)`; `ipar` is `IPAR(16)`, `fpar` `FPAR(16)`, `wk` `WK(*)`, all 0-based |
| C shim | none yet; `LP`, `RP` are default `LOGICAL` (4 bytes) in the Fortran and `bool` here, so a shim converts them |
| Plan | issue #42, PR goal/42-port-batch-easy |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd`), recompiled from `build_lab`'s `flags.make` and checked identical in
disassembly to the CMake object, 2026-10-16; compared with a throwaway C++ unit built with the
`openmp-release` flags, g++ 16.2, warning-free.

- (v) Same branch layout in `__w3profsmd_MOD_bisinit` and the C++ wrapper: the `IPAR(4) < WKSIZE`
  early return storing -2, the `IPAR(2)` three-way test (`jg`/`je`, then `sete` for `== 1`), the
  `IPAR(3) == 0` store of `DSC`, `IPAR(7:13)` cleared as one 16-byte and one 8-byte plus 4-byte
  store, the tolerance test as `comisd` against 0 and 1.0 with the `ucomisd`/`setnp` pair for the
  two `== 0` tests (a NaN `FPAR(1)` or `FPAR(2)` fails every comparison and keeps the tolerances),
  the -4 store.
- (v) The default tolerances are the same doubles in both GIMPLE dumps (one 16-byte constant
  `{1.0e-6, 1.0e-16}`), the `FPAR(11) < 0` test the same `comisd`.
- (v) Store widths differ only where the stored bytes are the same: gfortran clears `FPAR(3:10)`
  with four 16-byte stores and `WK` with a 2-wide vector loop, g++ uses two 32-byte stores and a
  `memset` of `WKSIZE*8` bytes. Every stored value is `+0.0`, all bits zero, so the memory after
  the call is the same; the VEX encoding comes from `-march=x86-64-v3`.
- (v) `REAL*8` stays `double`; no libm or libmvec call, no FMA, no reduction, no MIN/MAX.
- ⚠ Not compiled for CUDA.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3profsmd.F90` from `build_lab`'s
  `flags.make` (disassembly identical to the CMake object) and compared `__w3profsmd_MOD_bisinit`
  with the C++ unit instruction by instruction: same branches and early returns, same
  `comisd`/`ucomisd` tests (NaN path included), same `{1.0e-6, 1.0e-16}` constant in both
  GIMPLE dumps, no `FPAR` store on the -4 path; the caller and regtest cites checked at
  `03adda91`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
