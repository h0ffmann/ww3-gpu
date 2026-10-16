# USOL_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Solves `U x = y` with `U` unit upper triangular, stored in compressed sparse row form (`au`,
`jau`, `iau`, diagonal implied 1, not stored), by backward elimination from row `n` up to row 1.
A SPARSKIT routine kept in `w3profsmd.F90`.

## What was ported

| | |
|---|---|
| WW3 routine | `usol`, `model/src/w3profsmd.F90:3492-3534` at NOAA-EMC/WW3 `03adda91`, 43 lines; no `#ifdef` inside it |
| Callers | none: `git -C WW3-noaa grep -nw usol -- model/src` finds only the `PUBLIC` list (`w3profsmd.F90:63`), the module's index comment (`:2849`) and the routine itself. Translated for completeness; no WW3 run can exercise it |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:117-125`), so by nearly every switch file; compiled is not called |
| Kokkos code | [`../src/ww_kokkos/profs_usol.hpp`](../src/ww_kokkos/profs_usol.hpp), `ww::profs::usol`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based holding the Fortran 1-based indices |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) Same double-precision sequence in both objects: `x(n) = y(n)` copied, then per row `t`
  loaded from `y(k)` into a register, the inner loop `t = t - (x(jau(j)) * au(j))` as one `mulsd`
  then one `subsd` per term, in increasing `j`, and `t` stored to `x(k)` once (SSE in the Fortran
  object, VEX in the C++ one from `-march=x86-64-v3`, the same IEEE operations).
- (v) No FMA, no reciprocal, no libm or libmvec call; neither object vectorised the routine
  (`-fopt-info-vec-optimized` reports nothing at `:3492-3534` nor in the C++ function).
- ⚠ As in the Fortran, `x(n) = y(n)` is executed before the loop with no check on `n`: `n < 1`
  reads and writes out of bounds in both. Reproduced, not guarded.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no arithmetic defect. The Fortran module recompiled from `build_lab`'s flags is identical to the CMake object; its `usol` assembly and the C++ unit's (openmp-release flags) were compared operation by operation: same `y(n)` copy, same per-row `mulsd` of `x(jau(j))` by `au(j)` then `subsd` from `t` in increasing `j`, same loop bounds, no vectorisation, no libm call. Fixed: the header's body cite read `:3524-3531` for the lines at `w3profsmd.F90:3522-3529` (comment only; the object is unchanged).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing in WW3 calls `usol`, so the WW3 regtests cannot exercise it.
