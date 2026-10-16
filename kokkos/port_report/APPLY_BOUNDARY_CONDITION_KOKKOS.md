# APPLY_BOUNDARY_CONDITION_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Sets the open-boundary spectra of an unstructured grid, interpolated in time, for the PDLIB
schemes; the body behind `PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK`'s `apply_boundary_condition` hook.

## What was ported

| | |
|---|---|
| WW3 routine | `APPLY_BOUNDARY_CONDITION`, `model/src/w3profsmd_pdlib.F90:5150-5336` at NOAA-EMC/WW3 `03adda91` |
| Kokkos code | [`../src/ww_kokkos/pdlib_boundary.hpp`](../src/ww_kokkos/pdlib_boundary.hpp), `apply_boundary_condition`; returns at once without `FLBPI` |
| C shim | none yet; `DSEC21` is computed on the host, only with `FLBPI` |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch A, issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3profsmd_pdlib.F90` (`switch_PDLIB`), read by the translator and
a fresh reviewer, 2026-10-16.

- (v) Parallel over spectral components, serial over boundary points in order, so the last write
  wins; `maxss`/`minss` with the quotient as destination; four `mulss`/`addss` roundings in order.
- (v) The `ELSE` at `:5254-5256` is dead: `RD10`, `RD20` are read only under the second `IF (FLBPI)`.
- ⚠ CUDA compiled, never run.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
