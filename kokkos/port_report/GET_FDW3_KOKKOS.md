# GET_FDW3_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12). Translated in the header
of its caller [`W3CGDM_R4`](W3CGDM_R4_KOKKOS.md), batch 4 unit U1.

`GET_FDW3`: the internal routine of `W3GSRUMD` that tabulates the stencil offsets `K` and weights `C` of order `N` for the grid derivatives.

## What was ported

| | |
|---|---|
| WW3 routine | `GET_FDW3`, `model/src/w3gsrumd.F90:9582-9604` at `WW3@96892c95` (h0ffmann/WW3), byte-identical to NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 HEAD -- model/src/w3gsrumd.F90` is empty, v) |
| Callers at the pin | `W3CGDM_R4` (`:4183`, `:4196`) and the other `W3CGDM`/`W3GRD*` bodies of the module (`:4536` to `:5695`) |
| Kokkos code | [`../src/ww_kokkos/gsru_w3cgdm_r4.hpp`](../src/ww_kokkos/gsru_w3cgdm_r4.hpp): `ww::gsru::get_fdw3`, host-only C++ |
| C shim | none yet |
| Plan | [batch 4 plan](../../docs/plans/2026-11-01-001-port-batch-4-plan.md), unit U1 |

## Arithmetic

The reference and the method are the parent's: the `-O3` object of `w3gsrumd.F90` (WW3's CMake
Release flags, gfortran 16.2, `switches/switch_lab_shrd`), recompiled and checked identical to the
CMake object, read as GIMPLE and assembly beside a throwaway C++ unit under the `openmp-release`
flags, 2026-11-12, by the translator and again by a fresh reviewer. [`W3CGDM_R4_KOKKOS.md`](W3CGDM_R4_KOKKOS.md)
has the full list.

(v) Its weights come from `W3FDWT_R8` in the same order as the object.
⚠ `:9592-9601` leaves the entries of `K` and `C` with `J > L` or `I > L` undefined; the C++ keeps them zero.
- (v) No FMA (`vfmadd`: 0 in the Fortran object and in the C++ unit).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested: WW3's regtests are postponed to the validation phase (ADR-0016). Not wired: no shim
and no caller hunk.
