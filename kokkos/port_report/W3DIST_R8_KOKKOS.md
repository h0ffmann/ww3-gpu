# W3DIST_R8_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12). Translated in the header
of its caller [`W3GFCL_R8`](W3GFCL_R8_KOKKOS.md), batch 4 unit U1.

`W3DIST_R8`: compute the distance between two points (`w3gsrumd.F90:194`), the `REAL(8)` body of the generic `W3DIST` (`:472`), with `DIST_WITH_SINE` (`:5827`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3DIST_R8`, `model/src/w3gsrumd.F90:5829-5877` at `WW3@96892c95` (h0ffmann/WW3), byte-identical to NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 HEAD -- model/src/w3gsrumd.F90` is empty, v) |
| Callers at the pin | `W3GFCL_R8` (`:1617`) and other routines of `W3GSRUMD` through the generic name |
| Kokkos code | [`../src/ww_kokkos/gsru_w3gfcl_r8.hpp`](../src/ww_kokkos/gsru_w3gfcl_r8.hpp): `ww::gsru::w3dist_r8`, host-only C++ |
| C shim | none yet |
| Plan | [batch 4 plan](../../docs/plans/2026-11-01-001-port-batch-4-plan.md), unit U1 |

## Arithmetic

The reference and the method are the parent's: the `-O3` object of `w3gsrumd.F90` (WW3's CMake
Release flags, gfortran 16.2, `switches/switch_lab_shrd`), recompiled and checked identical to the
CMake object, read as GIMPLE and assembly beside a throwaway C++ unit under the `openmp-release`
flags, 2026-11-12, by the translator and again by a fresh reviewer. [`W3GFCL_R8_KOKKOS.md`](W3GFCL_R8_KOKKOS.md)
has the full list.

(v) `ARGD = sqrt(((cos(YT*D2R)*cos(YS*D2R))*SLAM)*SLAM + SPHI*SPHI)`, `DIST = asin(ARGD)*(2*R2D)`; `sin`, `cos`, `asin` are libm calls, `SQRT` is `sqrtsd`.
⚠ `DX**2 + DY**2` (`:5868`): the Fortran object adds `DX**2` first; the copy inlined in `w3gfcl_r8` does the same, the out-of-line copy adds `DY**2` first, so the NaN payload can differ when both terms are NaN. Swapping the C++ terms swaps both copies: GCC picks the order (fresh review, objdump of both, 2026-11-12).
- (v) No FMA (`vfmadd`: 0 in the Fortran object and in the C++ unit).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested: WW3's regtests are postponed to the validation phase (ADR-0016). Not wired: no shim
and no caller hunk.
