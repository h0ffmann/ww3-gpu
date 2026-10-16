# LDSOL_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Solves `L x = y` for a lower triangular `L` stored in modified sparse row form (`al`, `jal`),
with the diagonal already inverted: `x(1) = y(1)*al(1)`, then for each row `k` the sum
`t = y(k) - sum al(j)*x(jal(j))` over the row's off-diagonal entries and `x(k) = al(k)*t`. A
SPARSKIT routine kept in `w3profsmd.F90`.

## What was ported

| | |
|---|---|
| WW3 routine | `ldsol`, `model/src/w3profsmd.F90:3302-3344` at NOAA-EMC/WW3 `03adda91`, 43 lines; no `#ifdef` inside it |
| Callers | none: `git -C WW3-noaa grep -nw ldsol` finds only the definition, its comments and the `PRIVATE ::` list at `w3profsmd.F90:61-63`, so nothing outside the module can call it either. Translated for completeness; it can never be exercised |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:117-125`), so by nearly every switch file; with no caller, no regtest runs it |
| Kokkos code | [`../src/ww_kokkos/profs_ldsol.hpp`](../src/ww_kokkos/profs_ldsol.hpp), `ww::profs::ldsol`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based holding the Fortran 1-based indices |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) Same GIMPLE shape and the same double-precision operation sequence in both objects:
  `y(1)*al(1)` stored to `x(1)`; per row, `t` loaded from `y(k)` and kept in a register, the inner
  loop `t - al(j)*x(jal(j))` (one `mulsd`, one `subsd`), then `al(k)*t` stored to `x(k)`. SSE in
  the Fortran object, VEX in the C++ one from `-march=x86-64-v3`, the same IEEE operations; both
  products are commutative, so operand order cannot change bits.
- (v) No FMA, no reciprocal, no libm or libmvec call; neither object vectorised the routine
  (`-fopt-info-vec-optimized` reports nothing at `:3302-3344`).
- (v) `x(1)` is written before the `n` test in the object (`w3profsmd.F90:3333`), so the C++
  writes `x[0]` unconditionally too; with `n < 1` both write one element past an empty array ⚠
  (WW3's behaviour, kept).
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3profsmd.F90` from `build_lab`'s
  `flags.make` in the pinned shell (disassembly identical to the CMake object) and compared
  `__w3profsmd_MOD_ldsol` with the C++ unit's GIMPLE and assembly instruction by instruction: the
  same three products and one subtraction per inner step, in the same order, with the same first
  source operand in each `mulsd`/`vmulsd` (so a NaN payload propagates from the same operand), no
  vectorisation and no library call on either side. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing in WW3 calls `ldsol`, so the WW3 regtests cannot exercise it.
