# LSOLC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Solves `L x = y` for a unit lower triangular `L` stored in compressed sparse column form (`al`,
`jal`, `ial`) by column-oriented forward elimination: copy `y` into `x`, then for each column `k`
subtract `x(k)*al(j)` from every `x(jal(j))` below it. A SPARSKIT routine kept in
`w3profsmd.F90`.

## What was ported

| | |
|---|---|
| WW3 routine | `lsolc`, `model/src/w3profsmd.F90:3346-3389` at NOAA-EMC/WW3 `03adda91`, 44 lines; no `#ifdef` inside it |
| Callers | none: `git -C WW3-noaa grep -n -i lsolc -- model` finds only its definition, the `PRIVATE` list at `:61-65` and comments, so nothing outside the module can call it either. Translated for completeness; it can never be exercised |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:117-125`), so by nearly every switch file, but no regtest can reach a routine nothing calls |
| Kokkos code | [`../src/ww_kokkos/profs_lsolc.hpp`](../src/ww_kokkos/profs_lsolc.hpp), `ww::profs::lsolc`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based holding the Fortran 1-based indices |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) The only floating-point operations are in the elimination loop (`:3379-3384`), and both
  objects emit the same sequence per `j`: load `al(j)`, `mul` by `t` held in a register, load
  `x(jal(j))`, `sub` the product, store (`mulsd`/`subsd` in the Fortran object, `vmulsd`/`vsubsd`
  in the C++ one from `-march=x86-64-v3`, the same IEEE double operations). `t` is loaded once per
  column in both, so a `jal(j)` equal to `k` does not change `t`.
- (v) The copy loop (`:3376-3378`) is vectorised in both objects (16-byte vectors in the Fortran,
  32-byte with an aliasing check in the C++); it moves data only, so the widths cannot change bits.
- (v) No FMA, no reciprocal, no libm or libmvec call; the elimination loop is not vectorised in
  either object.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no arithmetic defect. The `-O3` GIMPLE and disassembly of the Fortran (recompiled from `build_lab` flags, identical to the CMake object) and of the C++ unit were compared operation by operation: same per-`j` load, `al(j)*t`, subtract, store, `t` loaded once per column, no FMA, no call, elimination loop scalar in both; no `#ifdef` in the routine. Fixed: the Fortran line cites of the two loops in the header and this report were 2 to 4 lines off (copy `:3376-3378`, elimination `:3379-3384`); the C++ object is unchanged.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing in WW3 calls `lsolc`, so the WW3 regtests cannot exercise it.
