# UDSOL_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Solves `U x = y` with `U` a non-unit upper triangular matrix in modified sparse row form (`au`,
`jau`) whose diagonal `au(1:n)` is stored inverted: backward elimination, each row ending with
`x(k) = au(k)*t`. A SPARSKIT routine kept in `w3profsmd.F90`.

## What was ported

| | |
|---|---|
| WW3 routine | `udsol`, `model/src/w3profsmd.F90:3536-3579` at NOAA-EMC/WW3 `03adda91`, 44 lines; no `#ifdef` inside it |
| Callers | none: `git -C WW3-noaa grep -nw udsol -- model/src` finds only the `PUBLIC` list (`w3profsmd.F90:64`), a comment (`:2850`) and the routine itself. Translated for completeness; it can never be exercised |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:117-125`), so by nearly every switch file; nothing calls the routine under any of them |
| Kokkos code | [`../src/ww_kokkos/profs_udsol.hpp`](../src/ww_kokkos/profs_udsol.hpp), `ww::profs::udsol`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based holding the Fortran 1-based indices |
| C shim | none (no caller) |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object (`build_lab`), against a throwaway C++ unit with the openmp-release flags and
`-ffp-contract=off`, 2026-10-16.

- (v) Same double-precision operation sequence in both objects: `y(n)*au(n)` stored to `x(n)`;
  per row `t` kept in a register, inner loop `t - au(j)*x(jau(j))` (load `x`, multiply by `au(j)`,
  subtract from `t`), then `au(k)*t` stored to `x(k)` (SSE `mulsd/subsd` in the Fortran object,
  VEX `vmulsd/vsubsd` in the C++ one from `-march=x86-64-v3`, the same IEEE operations; the
  multiply operands are commuted between source and object, which does not change the bits).
- (v) No FMA, no reciprocal, no libm or libmvec call; neither object vectorised the routine
  (`-fopt-info-vec-optimized` reports nothing at `:3536-3579`, nothing in the C++ unit's kernel).
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. The routine has no `#ifdef` and is present in the
  preprocessed `switch_lab_shrd` source; the module was recompiled from `build_lab`'s `flags.make`
  and found identical to the CMake object, and `__w3profsmd_MOD_udsol` was read against the C++
  unit instruction by instruction (same loads, `y(n)*au(n)`, inner `t - x(jau(j))*au(j)` in
  ascending `j`, final `t*au(k)`, no vectorisation, no FMA, no call). Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing in WW3 calls `udsol`, so the WW3 regtests cannot exercise it.
