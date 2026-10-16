# WAVNU_LOCAL_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Wavenumber and group velocity at one point from `WAVNU3` with the depth floored at `DMIN`.

## What was ported

| | |
|---|---|
| WW3 routine | `WAVNU_LOCAL`, `model/src/w3dispmd.F90:456-547` at NOAA-EMC/WW3 `03adda91`; no `#ifdef` inside. Called at `w3profsmd_pdlib.F90:765,3689,5702,5828,6162,6258`, every call under `#ifdef NOCGTABLE`, which no switch file and no CMake file defines: unreachable in WW3 as built (v) |
| Kokkos code | [`../src/ww_kokkos/disp_wavnu3.hpp`](../src/ww_kokkos/disp_wavnu3.hpp), `wavnu_local(sig, dw, dmin, wnl, cgl)`; `DMIN` is per grid, so the caller passes it |
| C shim | none |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch B, issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3dispmd.F90`, read by the translator and a fresh reviewer,
2026-10-16.

- (v) `MAX(DW, DMIN)` is `maxss` with `DW` into `DMIN`, `DMIN > DW ? DMIN : DW`, as the header
  writes it; then the `WAVNU3` call. No FMA.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
