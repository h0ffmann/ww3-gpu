# DIFFERENTIATE_XYDIR_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The driver of the wave setup module (`w3wavset`, switch `SETUP`) that takes the x and y
derivatives of a nodal field on the unstructured (PDLIB) grid. It does no arithmetic: it calls
`DIFFERENTIATE_XYDIR_MAPSTA` (`:433`). Its only caller is `TRIG_COMPUTE_LH_STRESS`
(`w3wavset.F90:561`, `:566`, `:573`), on the radiation stresses SXX, SXY and SYY.

## What was ported

| | |
|---|---|
| WW3 routine | `DIFFERENTIATE_XYDIR`, `model/src/w3wavset.F90:363-435` at NOAA-EMC/WW3 `03adda91`. `w3wavset.F90` is built only with switch `SETUP` (`model/src/cmake/switches.json:806-807`); no regtest switch and no NOAA operational switch carries `SETUP` (`git grep -lw SETUP -- '*switch*'` finds only `switches.json`). Its only `#ifdef` block is `W3_S` (`STRACE`), output only |
| Kokkos code | [`../src/ww_kokkos/wavset_differentiate_xydir.hpp`](../src/ww_kokkos/wavset_differentiate_xydir.hpp), `ww::wavset::differentiate_xydir`, an `inline` host function template |
| Callee | `DIFFERENTIATE_XYDIR_MAPSTA` (`:225-350`) is taken as a callable `(var, dvdx, dvdy)`: host code that binds the module state (`npa`, `NE`, `INE`, `iplg`, `PDLIB_IEN`, `PDLIB_TRIA`, `MAPSTA`), the `WEI` work array and the MPI exchange `PDLIB_exchange1Dreal` (`:348-349`), and calls `ww::wavset::differentiate_xydir_mapsta` ([`DIFFERENTIATE_XYDIR_MAPSTA_KOKKOS.md`](DIFFERENTIATE_XYDIR_MAPSTA_KOKKOS.md)). It is not called directly: that would pass the state through this driver, and g++ inlines the callee into it, so the object would no longer be a single tail call (checked 2026-10-16, see Arithmetic). `DIFFERENTIATE_XYDIR_NATIVE` is commented out at `:434` and not translated |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3wavset.F90`, WW3's CMake Release flags, gfortran 16.2,
`regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP` (DIST MPI PDLIB SCOTCH), 2026-10-16. The
recompiled object is identical to the CMake object (v).

- (v) `REAL(rkind)` is `float`: `rkind = 4` (`PDLIB/yowdatapool.F90:48`).
- (v) The Fortran object of `__w3wavset_MOD_differentiate_xydir` is a single `jmp` to
  `__w3wavset_MOD_differentiate_xydir_mapsta` (GIMPLE: `[tail call]`, `:433`); the C++ unit,
  with the callee bound to an external function, is a single `jmp` to it too. No floating-point
  operation in either.
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3wavset.F90` from `build_setup`'s
  `flags.make` (object identical to the CMake one); the Fortran routine and the C++ unit are each one
  tail `jmp` to the callee with the arguments in the same order, no floating-point operation; the
  only `#ifdef` is `W3_S`. Fixed off-by-one line citations: the call is `:433`, the commented-out
  `DIFFERENTIATE_XYDIR_NATIVE` is `:434`.
- (v) A direct call was tried on 2026-10-16, after `wavset_differentiate_xydir_mapsta.hpp` existed: a
  driver taking the callee's 13 arguments and calling it, instantiated in a throwaway C++ unit
  with the openmp-release flags, compiles to the whole inlined body of the callee, not a `jmp`.
  The Fortran driver is one tail call with three arguments, so the callable is kept.
- ⚠ CUDA not compiled: the function is host-only, since its callee does an MPI exchange.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest switch compiles `w3wavset.F90`.
