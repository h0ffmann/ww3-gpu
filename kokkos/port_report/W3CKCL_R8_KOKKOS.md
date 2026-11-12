# W3CKCL_R8_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12). Translated in the header
of its caller [`W3GFCL_R8`](W3GFCL_R8_KOKKOS.md), batch 4 unit U1.

`W3CKCL_R8`: check if a point lies within a grid cell (`w3gsrumd.F90:189`), the `REAL(8)` body of the generic `W3CKCL` (`:508`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3CKCL_R8`, `model/src/w3gsrumd.F90:3615-3840` at `WW3@96892c95` (h0ffmann/WW3), byte-identical to NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 HEAD -- model/src/w3gsrumd.F90` is empty, v) |
| Callers at the pin | `W3GFCL_R8` (`:1540`) and other routines of `W3GSRUMD` through the generic name |
| Kokkos code | [`../src/ww_kokkos/gsru_w3gfcl_r8.hpp`](../src/ww_kokkos/gsru_w3gfcl_r8.hpp): `ww::gsru::w3ckcl_r8`, host-only C++ |
| C shim | none yet |
| Plan | [batch 4 plan](../../docs/plans/2026-11-01-001-port-batch-4-plan.md), unit U1 |

## Arithmetic

The reference and the method are the parent's: the `-O3` object of `w3gsrumd.F90` (WW3's CMake
Release flags, gfortran 16.2, `switches/switch_lab_shrd`), recompiled and checked identical to the
CMake object, read as GIMPLE and assembly beside a throwaway C++ unit under the `openmp-release`
flags, 2026-11-12, by the translator and again by a fresh reviewer. [`W3GFCL_R8_KOKKOS.md`](W3GFCL_R8_KOKKOS.md)
has the full list.

(v) `MINVAL`/`MAXVAL` are gfortran's NaN-aware inline reductions; `V1X = XXS(J)-XXS(J)` (`:3743`) is computed, not folded (NaN for a non-finite vertex).
(v) The branch-cut shift and its `DEBUG` text match the object (the parent report's differential cases).
- (v) No FMA (`vfmadd`: 0 in the Fortran object and in the C++ unit).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested: WW3's regtests are postponed to the validation phase (ADR-0016). Not wired: no shim
and no caller hunk.
