# W3SIC1_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Damping of waves by sea ice with a uniform, user-set rate (switch `IC1`), a source term `W3SRCE`
calls per sea point (`w3srcemd.F90:1367`, `:2150`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3SIC1`, `model/src/w3sic1md.F90:93-369` at NOAA-EMC/WW3 `03adda91`; compiled by nine regtest switches (`ww3_tic1.1`, `ww3_tic1.4`, `ww3_tic2.1`, `ww3_tic2.2`, `ww3_ts1/input_icesct_IC1`, `mww3_test_08`; `git grep -lw IC1 -- '*switch*'`), by no NOAA operational switch. Its only `#ifdef` blocks are `W3_S`/`W3_T` output |
| Kokkos code | [`../src/ww_kokkos/srce_ic1.hpp`](../src/ww_kokkos/srce_ic1.hpp), `w3sic1_point`; returns 2 where the Fortran stops in `EXTCDE(2)` (`:299-302`), after `D = 0` (`:289`) as in the Fortran |
| C shim | none yet; it passes `ICEP1` for the point (the only use of `IX`, `IY`) and stops the run on 2 |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch B, issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3sic1md.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd` with `IC0` replaced by `IC1`), read by the translator and a fresh reviewer,
2026-10-16.

- (v) `D1D = -((2*CG)*WN_I)`: the object computes `CG+CG`, `mulss`, then a sign flip, all exact, so
  the same bits; then `D*A`; the `ICECOEF1 == 0` and `INFLAGS2(-7)` branches both ways.
- (v) No FMA, no reciprocal, no libm or libmvec call, no reduction in either object.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`; only `FMUL`/`FADD` in the SASS), never run.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
