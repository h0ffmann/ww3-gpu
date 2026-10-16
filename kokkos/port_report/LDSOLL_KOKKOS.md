# LDSOLL_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Solves `L x = y` for a lower triangular `L` stored in modified sparse row form (`al`, `jal`, with
the diagonal `al(1:n)` already inverted) by level scheduling: for each level `ii` in turn, each row
`jrow = lev(i)` of that level takes the inner product of its off-diagonal entries with `x`,
`x(jrow) = (y(jrow) - sum al(k)*x(jal(k))) * al(jrow)`. A SPARSKIT routine kept in `w3profsmd.F90`.

## What was ported

| | |
|---|---|
| WW3 routine | `ldsoll`, `model/src/w3profsmd.F90:3439-3490` at NOAA-EMC/WW3 `03adda91`, 52 lines; no `#ifdef` inside it |
| Callers | none: `git -C WW3-noaa grep -n -i -w ldsoll -- model` finds only its definition, the `PRIVATE` list at `:61-65` and the index comment at `:2848`, so nothing outside the module can call it either. Translated for completeness; it can never be exercised |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:116-125`), so by nearly every switch file, but no regtest can reach a routine nothing calls |
| Kokkos code | [`../src/ww_kokkos/profs_ldsoll.hpp`](../src/ww_kokkos/profs_ldsoll.hpp), `ww::profs::ldsoll`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based holding the Fortran 1-based indices |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) The GIMPLE of both has the same three floating-point operations: `al(k)*x(jal(k))` and
  `t - (al(k)*x(jal(k)))` (`:3483`), and `al(jrow)*t` (`:3485`; the C++ writes `t*al(jrow)`, a
  commuted product). The objects emit them as `mulsd`/`subsd`/`mulsd` (Fortran) and
  `vmulsd`/`vsubsd`/`vmulsd` (C++, `-march=x86-64-v3`) with the same first source in each
  (`x(jal(k))` for the inner product, `t` for the diagonal scaling), so even a NaN's payload
  is the same.
- (v) The inner product is a sequential sum in `k` order in both; neither object vectorises any
  loop of `ldsoll`. No FMA, no reciprocal, no libm or libmvec call.
- (v) The level loop is sequential in the Fortran and the row loop within a level, marked
  "PARALLEL loop" in a comment, is serial too (no OpenMP directive); the C++ keeps both serial.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3profsmd.F90` from `build_lab`'s flags (object identical to the CMake one) and the header in a throwaway unit with the openmp-release flags; the assembly of `ldsoll` matches operation by operation (load `x(jal(k))`, multiply by `al(k)`, subtract from `t`, multiply `t` by `al(jrow)`, same first source each time, loop bounds read once per loop, no vectorisation); no `#ifdef` in `:3439-3490`. Fixed only the header comment's loop cite, `:3476` to `:3472`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing in WW3 calls `ldsoll`, so the WW3 regtests cannot exercise it.
