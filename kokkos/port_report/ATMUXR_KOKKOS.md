# ATMUXR_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Computes `y = transpose(A) x` for a rectangular `REAL*8` matrix in compressed sparse row (CSR)
form: `ia(1:n+1)` are the row pointers, `a(k)` and `ja(k)` the values and column numbers, `m` the
column dimension (the length of `y`). A SPARSKIT routine (Y. Saad) kept in `w3profsmd.F90`. It
differs from `atmux` only in zeroing `y(1:m)` instead of `y(1:n)`.

## What was ported

| | |
|---|---|
| WW3 routine | `atmuxr`, `model/src/w3profsmd.F90:2996-3044` at NOAA-EMC/WW3 `03adda91`, 49 lines; no `#ifdef` inside it |
| Callers | none: `git grep -i atmuxr -- model/src` finds only its definition, the `PUBLIC` list (`:62`) and the header comment (`:2836`). Translated for completeness; WW3 can never exercise it |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json`), so by nearly every switch file, `switch_lab_shrd` included |
| Kokkos code | [`../src/ww_kokkos/profs_atmuxr.hpp`](../src/ww_kokkos/profs_atmuxr.hpp), `ww::profs::atmuxr`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based holding the Fortran 1-based indices |
| C shim | none yet, and none planned while nothing calls it |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) The zeroing loop (`:3029-3031`) is vectorised in both objects (16-byte stores in the
  Fortran, 32-byte in the C++); it only stores `+0.0`, so the width changes no bit.
- (v) The scatter loop (`:3035-3039`) is not vectorised in either object. Both load `x(i)` once
  per row, read `ia(i+1)-1` once per row as the Fortran `DO` defines, and per entry multiply
  `x(i)` by `a(k)` (`mulsd` / `vmulsd`), add `y(ja(k))` (`addsd` / `vaddsd`, product as first
  operand in both) and store `y(ja(k))`, in the Fortran's order of `i` and `k`.
- (v) No FMA, no libm or libmvec call, no reduction reordered.
- ⚠ The multiply's operand order differs: the Fortran object computes `mulsd x,a` with `a(k)` as
  destination, the C++ `vmulsd a,x` with `x(i)` as first source. The product is the same; only
  when both `x(i)` and `a(k)` are NaN can the NaN payload returned differ.
- ⚠ Nothing checks `ja(k)` against `1..m` (the Fortran does not either; an index out of range
  writes outside `y` in both).
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3profsmd.F90` from `build_lab`'s flags (identical to the CMake object), read the routine (no `#ifdef` inside it, so no switch variant) and compared its GIMPLE and assembly with the header's openmp-release object: same zeroing to `+0.0`, same scatter order, `ia(i+1)-1` read once per row, add with the product as first source in both. Writing the product as `a(k)*x(i)` in C++ leaves g++'s operand order unchanged, so the NaN-payload ⚠ above stays. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing in WW3 calls `atmuxr`, so no regtest can reach it.
