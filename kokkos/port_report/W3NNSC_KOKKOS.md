# W3NNSC_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12). Translated in the header
of its caller [`W3GFCL_R8`](W3GFCL_R8_KOKKOS.md), batch 4 unit U1.

`W3NNSC`: create the nearest-neighbour-search object (`w3gsrumd.F90:200`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3NNSC`, `model/src/w3gsrumd.F90:6819-6879` at `WW3@96892c95` (h0ffmann/WW3), byte-identical to NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 HEAD -- model/src/w3gsrumd.F90` is empty, v) |
| Callers at the pin | `W3GFCL_R8` (`:1563`) and `GSU_CREATE` (`:7482`, `:7484`, `:7829`) |
| Kokkos code | [`../src/ww_kokkos/gsru_w3gfcl_r8.hpp`](../src/ww_kokkos/gsru_w3gfcl_r8.hpp): `ww::gsru::w3nnsc`, and `ww::gsru::w3nnsc_hook`, which fills the `W3NNSC` hook of [`gsru_gsu_create.hpp`](../src/ww_kokkos/gsru_gsu_create.hpp), host-only C++ |
| C shim | none yet |
| Plan | [batch 4 plan](../../docs/plans/2026-11-01-001-port-batch-4-plan.md), unit U1 |

## Arithmetic

The reference and the method are the parent's: the `-O3` object of `w3gsrumd.F90` (WW3's CMake
Release flags, gfortran 16.2, `switches/switch_lab_shrd`), recompiled and checked identical to the
CMake object, read as GIMPLE and assembly beside a throwaway C++ unit under the `openmp-release`
flags, 2026-11-12, by the translator and again by a fresh reviewer. [`W3GFCL_R8_KOKKOS.md`](W3GFCL_R8_KOKKOS.md)
has the full list.

(v) Integer index arithmetic only; the ring order of the `(2*NLVL+1)**2` neighbour deltas matches the object.
The C++ returns `kExitNnscBounds` where the Fortran writes out of bounds (`NLVL < 0`, or an `NNBR` overflow); see the header.
- (v) No FMA (`vfmadd`: 0 in the Fortran object and in the C++ unit).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested: WW3's regtests are postponed to the validation phase (ADR-0016). Not wired: no shim
and no caller hunk.
