# AMUXE_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Computes `y = A x` for a `REAL*8` matrix in Ellpack-Itpack (ELL) form: `a(i,k)`, `k=1..ncol`,
holds the entries of row `i` and `ja(i,k)` their column numbers, both with leading dimension `na`.
A SPARSKIT routine (Y. Saad) kept in `w3profsmd.F90`.

## What was ported

| | |
|---|---|
| WW3 routine | `amuxe`, `model/src/w3profsmd.F90:3046-3093` at NOAA-EMC/WW3 `03adda91`, 48 lines; no `#ifdef` inside it |
| Callers | none: `git -C WW3-noaa grep -nw amuxe -- model/src` finds only the `PUBLIC` list (`:62`), the index comment (`:2837`) and the routine. Translated for completeness; no WW3 run can exercise it |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json`), so by nearly every switch file, `switch_lab_shrd` included |
| Kokkos code | [`../src/ww_kokkos/profs_amuxe.hpp`](../src/ww_kokkos/profs_amuxe.hpp), `ww::profs::amuxe`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based and column-major, `ja` holding the Fortran 1-based column numbers |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) Zeroing loop (`:3081-3083`): both objects store `+0.0` (Fortran `0.0` converted to `REAL*8`),
  vectorised in both.
- (v) Product loop (`:3084-3089`): the Fortran object vectorises the inner `i` loop two doubles wide,
  unrolled twice (`mulpd`, `addpd`, gathered `x(ja(i,j))`, no alias check since dummies do not alias) with a scalar tail; the
  C++ object keeps it scalar (`vmulsd`, `vaddsd`). Each `y(i) = y(i) + a(i,j)*x(ja(i,j))` is one
  rounded product and one rounded sum per element, in the order `j = 1..ncol`, so the bits match
  for every input. No FMA, no libm call, no reduction reordered.
- (v) Leading dimension: gfortran takes `MAX(na,0)` as a 64-bit stride and multiplies it by
  `j-1` in 64 bits; the C++ does the same (`cmovns`, `imul` on 64-bit registers).
- (v) Operand order: at every site of both objects (Fortran vector body, Fortran scalar tail, C++
  scalar loop) the gathered `x(ja(i,j))` is the first operand of the multiply and the product the
  first operand of the add (`mulpd`/`mulsd` with `x` in the destination, `vmulsd` with `x` as first
  source), so even the NaN payload that propagates when both operands are NaN is the same.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect in the header. Recompiled `w3profsmd.F90` from `build_lab`'s
  `flags.make` (object identical to the CMake one) and compared `amuxe`'s assembly with a throwaway
  C++ unit's, site by site; fixed this report's operand-order bullet (it was a ⚠ claiming a
  swapped `x*a` in one tail step; every site of both objects multiplies `x` by `a`).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing in WW3 calls `amuxe`, so the WW3 regtests cannot exercise it.
