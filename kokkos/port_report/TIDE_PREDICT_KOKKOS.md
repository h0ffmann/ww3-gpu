# TIDE_PREDICT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Reconstructs a tidal series from the harmonic analysis of `W3TIDEMD` and measures its misfit: for
each series `IDEF` and time index `I`, the mean `TIDE_AMPC(1)` (plus a linear trend
`TIDE_PHG(1)·t/8760 h` when `ITREND = 1`) and, for each constituent, `f·A·cos((V+u)·2π − g·π/180)`,
with the nodal factor `f`, nodal phase `u` and astronomical argument `V` returned by `VUF`. The
residual is `DATAIN − PREDICTED`; `RMSR = sqrt(Σr²/(N − 2·TIDE_MF))` and `SDEV = sqrt(Σr²/N)`.
`TIDE_PREDICT_ONLY` (`:2571-2617`) is the same sum without the residual statistics.

## What was ported

| | |
|---|---|
| WW3 routine | `TIDE_PREDICT`, `model/src/w3tidemd.F90:2513-2568` at NOAA-EMC/WW3 `03adda91` (56 lines). No caller: `git grep -i tide_predict` in `WW3-noaa/model/src` finds only its definition and the module's lists of routines (`:45`, `:65`), so it is translated for completeness and no regtest can exercise it. No `#ifdef` in the routine; `w3tidemd.F90` is in every build (`cmake/src_list.cmake:58`). Switches with `TIDE`: `ww3_tp2.18` (`switch_TIDE`, `switch_TIDE_MPI`), `switch_Ifremer1`, `switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OAS*`, `switch_ite_pdlib`; no `switch_NCEP_*`/`switch_NWS_*` |
| Kokkos code | [`../src/ww_kokkos/tide_tide_predict.hpp`](../src/ww_kokkos/tide_tide_predict.hpp), `ww::tide::tide_predict`, a host function template; module state passed in, `CHARACTER*5` names as consecutive 5-byte records |
| Not translated | `VUF` (`:1706-1746`): taken as a callable `vuf(konx, vx, ux, fx, itime)` that the caller supplies. It executes `STOP` when the name is unknown, which is why the routine is host only |
| C shim | none yet |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3tidemd.F90` (WW3's CMake Release flags, gfortran
16.2) from `build_mixa` (`TIDE` on) and `build_lab` (`TIDE` off), each recompiled and found
identical to the CMake object; the GIMPLE of `tide_predict` is the same in both, so the routine
has one variant. Compared operation by operation with the GIMPLE and assembly of a C++ unit built
with the `openmp-release` flags (no warning), `VUF` an external call, 2026-10-16.

- (v) `TWPI` and `FAC` (`:87-88`) are float π widened and doubled, `0x1.921fb6p+2`, and
  `0x1.1df46aaaaaaabp-6`; hex literals in the header with a `static_assert` of how they are built,
  the same constants as in the object.
- (v) Trend (`:2534`) in double, rounded to `REAL`; `ARG` and `ADD` (`:2541-2542`, `:2550-2551`)
  in double with the float `cosf` of libm (two calls, no libmvec variant), rounded to `REAL`;
  `SUM1 = SUM1 + ADD` in float. Same operations and operand order as the object.
- (v) `RESID(I,IDEF)**2` (`:2560`) is a float product `r·r`, widened, then added to the double
  `SDEV(IDEF)`, in `I` order; `SSQ` is `SDEV` rounded to `REAL`; both square roots are `sqrtss`
  of `SSQ/REAL(N−M)` and `SSQ/REAL(N)` in float, widened (`:2563-2565`). `M = 2·TIDE_MF` is read
  once before the loops (`:2528`).
- (v) No reduction reordered, no vectorisation (`-fopt-info-vec` reports nothing for the routine),
  no MIN/MAX, no integer conversion of a real.
- ⚠ WW3 defect, reproduced: `SDEV=0.D0` (`:2531`) clears the whole array at the start of every
  `IDEF`, so on return only `SDEV(NDEF)` holds a deviation and `SDEV(1:NDEF-1)` are 0. The object
  does the same (`memset` of `NDEF` doubles inside the `IDEF` loop).
- ⚠ When `N <= 2·TIDE_MF`, `RMSR` divides by zero or a negative number: `+Inf` or NaN, in the
  Fortran and the C++ alike.
- ⚠ `XLAT` is unused. `TIDE_MF`, `TIDE_NIN`, `TIDE_NINF(k)` are not checked against `MC = 70` and
  10, as in `TIDE_PREDICT_ONLY`.
- CUDA: not compiled; the function is host only (the `VUF` callable stops the run).
- (v) Fresh review 2026-10-16: no defect. `w3tidemd.F90` recompiled from `build_mixa` and
  `build_lab` flags (both identical to the CMake objects, `tide_predict` GIMPLE identical between
  them), and a fresh C++ unit (no warning) compared with it operation by operation: index arithmetic
  of `TIDE_AMPC`/`TIDE_PHG` (70), `TIDE_PHGI`/`TIDE_AMPCI` (10,10), `TIDE_KONIN` and `TIDECON_NAME`
  (5-byte records), the folded `TWPI`/`FAC` bits, the `/8.76e+3` trend, `cosf` and `sqrtf`, the
  float `SUM1` and `r·r`, the double `SDEV` sum in `I` order, and the whole-array `memset` per
  `IDEF`. `TIDE_NIN.NE.0` tested as `> 0` in the object is equivalent to the C++ loop. Nothing
  changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
