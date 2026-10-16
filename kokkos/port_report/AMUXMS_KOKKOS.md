# AMUXMS_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Computes `y = A x` for a `REAL*8` matrix in modified sparse row form: `a(1:n)` is the diagonal,
`ja(1:n+1)` the row pointers of the off-diagonal entries, `a(k)`, `ja(k)` beyond them the values
and column numbers. A SPARSKIT routine (Y. Saad) kept in `w3profsmd.F90`.

## What was ported

| | |
|---|---|
| WW3 routine | `amuxms`, `model/src/w3profsmd.F90:2903-2944` at NOAA-EMC/WW3 `03adda91`, 42 lines; no `#ifdef` inside it |
| Callers | none: `git -C WW3-noaa grep -nw amuxms -- model/src` finds only the module's `PRIVATE` list (`:62`), the index comment (`:2834`) and the routine, so nothing outside the module can call it either. Translated for completeness; no WW3 run can exercise it |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json`), so by nearly every switch file, `switch_lab_shrd` included |
| Kokkos code | [`../src/ww_kokkos/profs_amuxms.hpp`](../src/ww_kokkos/profs_amuxms.hpp), `ww::profs::amuxms`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based holding the Fortran 1-based indices |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) First loop (`:2932-2934`): both objects vectorise it (Fortran `mulpd` by 2, without an alias
  check; C++ `vmulpd` by 4 with an alias check and a scalar tail). Each `y(i) = a(i)*x(i)` is one
  rounded product either way, so the bits match for every finite or infinite input.
- (v) Second loop (`:2935-2941`): both objects keep `y(i)` in a register over the `k` loop, add
  `x(ja(k))*a(k)` to it with one `mulsd` and one `addsd` per entry in the Fortran's order, and store
  `y(i)` after the loop. The C++ carries a local accumulator to reproduce this; it does not change
  the result since Fortran dummies do not alias.
- (v) No FMA, no libm or libmvec call, no reduction reordered: the inner sum stays scalar and in
  order in both objects.
- ⚠ The scalar tail of the first loop multiplies `a*x` in the C++ object and `x*a` in the Fortran
  one. IEEE multiplication is commutative, so the result differs only in which NaN payload
  propagates when both operands are NaN.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect beyond the ⚠ above. Recompiled `w3profsmd.F90` with `build_lab`'s flags.make (object identical to the CMake one) and the header in a throwaway unit with the openmp-release flags; compared every multiply and add site of both objects. Operand order is x first at every Fortran site; the C++ has x first in the 4-wide and 2-wide bodies and the scalar fallback, a first in the last scalar tail. Writing `x*a` in the source moves the mismatch into the vector bodies, so g++ cannot be steered by source order and the header was left as translated; the difference stays the both-NaN payload case. Report fixed: `:62` is the `PRIVATE` list.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing in WW3 calls `amuxms`, so the WW3 regtests cannot exercise it.
