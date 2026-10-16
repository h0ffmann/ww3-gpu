# PROP_FREQ_SHIFT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The frequency-shift velocity `CAS` and the band widths `DMM` (method 1) of one node of the
unstructured grid, for the PDLIB implicit scheme's spectral matrix; `CALCARRAY_JACOBI_SPECTRAL_1`
calls it when `FreqShiftMethod = 1`.

## What was ported

| | |
|---|---|
| WW3 routine | `PROP_FREQ_SHIFT`, `model/src/w3parall.F90:604-739` at NOAA-EMC/WW3 `03adda91`, as `WW3/regtests/ww3_tp2.17/input/switch_PDLIB` compiles it |
| Kokkos code | [`../src/ww_kokkos/prop_freq_shift.hpp`](../src/ww_kokkos/prop_freq_shift.hpp), `prop_freq_shift(a, ip, isea, cas, dmm, dtg, work)`, on [`prop_parall.hpp`](../src/ww_kokkos/prop_parall.hpp)'s arrays and `dsdd_fill` |
| C shim | none yet; `CALCARRAY_JACOBI_SPECTRAL_1`'s `freq_shift(ip, isea, cas, dmm, dtg)` forwards to it with the thread's work buffer |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch A, issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3parall.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_PDLIB`), rebuilt by the translator identical to the CMake build's, 2026-10-16.

- (v) Every operation in the Fortran's order (`:696-712`), `FKD` without `FACK` (unlike method 2),
  true divisions (`(V*x)/V` not simplified), all `REAL`; no FMA and no libm call in either object.
- (v) `CFLK` is not kept as an array: the Fortran reads only its rows `1..NK` (`:730-735`), after
  which nothing reads it, so the C++ writes row `IK+1` straight into `CAS` when `IK+1 <= NK`. The
  same values land at the same indices; a storage change only (fresh reviewer).
- (v) `DMM(0:NK+2)` filled in the Fortran's order (`1..NK+1`, then `NK+2 = 0`, then `0 = DMM(1)`);
  `DEPTH` and `DSDD` through the `dsdd_fill` reviewed with `prop_parall.hpp`, whose `MAX` operand
  orders were read from the object there; `LPDLIB` both ways.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, no `fma` in the PTX, `div.rn.f32`), never run;
  `max.f32` on −0 not compared with `maxss`. Two NaNs of different payload may give a different
  NaN through commuted operands.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
