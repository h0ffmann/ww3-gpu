# APPLY_BOUNDARY_CONDITION_VA_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Sets the spectra of the open-boundary nodes of an unstructured grid from the boundary input,
interpolated in time between `BBPI0` and `BBPIN`, under `PDLIB`.

## What was ported

| | |
|---|---|
| WW3 routine | `APPLY_BOUNDARY_CONDITION_VA`, `model/src/w3profsmd_pdlib.F90:5037-5148` at NOAA-EMC/WW3 `03adda91`, as `WW3/regtests/ww3_tp2.17/input/switch_PDLIB` compiles it; 0.01 % of the attributed time of the PDLIB profile (`docs/data/ww3_pdlib_gprof.tsv`) |
| Kokkos code | [`../src/ww_kokkos/pdlib_boundary_va.hpp`](../src/ww_kokkos/pdlib_boundary_va.hpp), `apply_boundary_condition_va` |
| C shim | none yet; the grid-type, `FLBPI` and `IAPROC` tests (`:5114,:5122`) and the two `DSEC21` calls stay on the host |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch A, issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3profsmd_pdlib.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_PDLIB`), read by the translator and a fresh reviewer, 2026-10-16.

- (v) `RD2 = MIN(1.,MAX(0.,RD1/RD2))` (`:5126`) is `divss`, `maxss(x,0)`, `minss(x,1)` with the
  computed value first, then `RD1 = 1 - RD2`; `RD20 > 0.001` (`0x3a83126f`) is a strict compare, and
  a NaN takes the `ELSE`. The boundary value is two `mulss` and an `addss`, times `CG`, then
  `maxss(x,0)`. All `REAL`; no FMA.
- (v) One thread per spectral component walks the boundary points `IBI = 1..NBI` in order, so when
  two of them name one sea point the last write wins, as in the Fortran; every input is read-only.
- (v) `CG(0:NK+1,0:NSEA)`, `VA(NSPEC,0:NSEALM)`, `BBPI0/BBPIN(:,0:NBI)` keep their bases;
  `IK = 1 + (ISP-1)/NTH`. The `ELSE` at `:5118-5121` cannot run, since `:5122` also needs `FLBPI`.
- ⚠ The shim's views and their bases do not exist yet. CUDA compiled (no `fma` in the PTX), never
  run.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
