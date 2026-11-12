# W3FDWT_R8_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12). Translated in the header
of its caller [`W3CGDM_R4`](W3CGDM_R4_KOKKOS.md), batch 4 unit U1.

`W3FDWT_R8`: compute finite-difference weights (`w3gsrumd.F90:199`), the `REAL(8)` body of the generic `W3FDWT` (`:504`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3FDWT_R8`, `model/src/w3gsrumd.F90:6706-6747` at `WW3@96892c95` (h0ffmann/WW3), byte-identical to NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 HEAD -- model/src/w3gsrumd.F90` is empty, v) |
| Callers at the pin | `GET_FDW3` (`:9597`) |
| Kokkos code | [`../src/ww_kokkos/gsru_w3cgdm_r4.hpp`](../src/ww_kokkos/gsru_w3cgdm_r4.hpp): `ww::gsru::w3fdwt_r8`, host-only C++ |
| C shim | none yet |
| Plan | [batch 4 plan](../../docs/plans/2026-11-01-001-port-batch-4-plan.md), unit U1 |

## Arithmetic

The reference and the method are the parent's: the `-O3` object of `w3gsrumd.F90` (WW3's CMake
Release flags, gfortran 16.2, `switches/switch_lab_shrd`), recompiled and checked identical to the
CMake object, read as GIMPLE and assembly beside a throwaway C++ unit under the `openmp-release`
flags, 2026-11-12, by the translator and again by a fresh reviewer. [`W3CGDM_R4_KOKKOS.md`](W3CGDM_R4_KOKKOS.md)
has the full list.

(v) The recurrences of `:6735-6742` in the source's order; `:6735` is vectorised over `K` elementwise in the Fortran object, which gives the same bits as the scalar C++.
- (v) No FMA (`vfmadd`: 0 in the Fortran object and in the C++ unit).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested: WW3's regtests are postponed to the validation phase (ADR-0016). Not wired: no shim
and no caller hunk.
