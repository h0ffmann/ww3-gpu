# DIAGONALIZE_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12).

Jacobi eigenvalues and eigenvectors of a real symmetric matrix (Numerical Recipes), with its
internal rotation, up to 50 sweeps. Called only by `INSIS2`.

## What was ported

| | |
|---|---|
| WW3 routine | `DIAGONALIZE`, `model/src/w3servmd.F90:2308-2387` at h0ffmann/WW3 `96892c95` |
| Kokkos code | `ww::is2::diagonalize` in [`../src/ww_kokkos/is2_insis2.hpp`](../src/ww_kokkos/is2_insis2.hpp), host-only; returns `DiagonalizeResult {nrot, too_many_iterations}` |

## Arithmetic

- (v) `DOUBLE PRECISION` throughout, the rotations in the Fortran's order (`ROTATE` on the array
  sections of `:2363-2369`), no `vfmadd`.
- (v) Eigenvalues and eigenvectors equal the Fortran's bit for bit in the `INSIS2` differential
  run ([`INSIS2_KOKKOS.md`](INSIS2_KOKKOS.md)), `NTH` from 5 to 36. ⚠ Not reproducible by a repository command.

## Benchmarks

None.

## How it was regtested

Not regtested: postponed (ADR-0016). Not wired.

## WW3 defects found

- ⚠ After 50 sweeps the Fortran writes 'Too many iterations in DIAGONALIZE' and returns what it
  has; the C++ sets `too_many_iterations` instead of printing.
