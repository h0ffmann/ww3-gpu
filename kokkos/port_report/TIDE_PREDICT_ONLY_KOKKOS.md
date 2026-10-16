# TIDE_PREDICT_ONLY_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Reconstructs a tidal series from the harmonic analysis of `W3TIDEMD`: for each series `IDEF` and
time index `I`, the mean `TIDE_AMPC(1)` (plus a linear trend `TIDE_PHG(1)·t/8760 h` when
`ITREND = 1`) and, for each constituent, `f·A·cos((V+u)·2π − g·π/180)`, with the nodal factor
`f`, nodal phase `u` and astronomical argument `V` returned by `VUF`. Unlike `TIDE_PREDICT`
(`:2513-2568`) it computes no residual or deviation.

## What was ported

| | |
|---|---|
| WW3 routine | `TIDE_PREDICT_ONLY`, `model/src/w3tidemd.F90:2571-2617` at NOAA-EMC/WW3 `03adda91` (47 lines). No caller: `git grep -i tide_predict_only` in `WW3-noaa/model/src` finds only its definition and the module's list of routines (`:66`), so it is translated for completeness and no regtest can exercise it. No `#ifdef` in the routine; `w3tidemd.F90` is in every build (`model/src/cmake/src_list.cmake:58`). Switches with `TIDE`: `ww3_tp2.18` (`switch_TIDE`, `switch_TIDE_MPI`), `switch_Ifremer1`, `switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OAS*`, `switch_ite_pdlib`; no `switch_NCEP_*`/`switch_NWS_*` |
| Kokkos code | [`../src/ww_kokkos/tide_tide_predict_only.hpp`](../src/ww_kokkos/tide_tide_predict_only.hpp), `ww::tide::tide_predict_only`, a host function template; module state passed in, `CHARACTER*5` names as consecutive 5-byte records |
| Not translated | `VUF` (`:1706-1746`): taken as a callable `vuf(konx, vx, ux, fx, itime)` that the caller supplies. It searches `TIDECON_ALLNAMES` and reads `V_ARG`, `U_ARG`, `F_ARG` at time `ITIME`, and executes `STOP` when the name is unknown, which is why the routine is host only |
| C shim | none yet |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3tidemd.F90` (WW3's CMake Release flags, gfortran
16.2) from `build_mixa` (`TIDE` on) and `build_lab` (`TIDE` off), each recompiled and found
identical to the CMake object; the GIMPLE of `tide_predict_only` is the same in both, so the
routine has one variant. Compared operation by operation with the GIMPLE and assembly of a C++
unit built with the `openmp-release` flags (no warning), `VUF` an external call, 2026-10-16.

- (v) `TWPI = 3.1415926535898*2.` (`:87`): the literal is default `REAL`, so the `DOUBLE PRECISION`
  parameter is float π widened and doubled, `6.283185482025146484375` (`0x1.921fb6p+2`), not 2π
  in double. `FAC = TWPI/360.` is `0x1.1df46aaaaaaabp-6`. Both are hex literals in the header
  with a `static_assert` of how they are built; the object's constants match.
- (v) The trend (`:2588`) is computed in double: `(double)AMPC + (double)PHG·HOURS / 8760.`,
  then rounded to `REAL`; `365.*24.` is folded to 8760 exactly.
- (v) `ARG = REAL((VX+UX)·TWPI − (double)PHG·FAC)`; `ADD = REAL(FX·(double)AMPC·(double)COSF(ARG))`:
  the cosine is the float `cosf` of libm (two calls, no libmvec variant), the product in double.
  `SUM1 = SUM1 + ADD` in float, first `addss` operand the same as in the object in both loops.
- (v) No reduction reordered, no vectorisation (`-fopt-info-vec` reports nothing for the routine),
  no MIN/MAX, no integer conversion of a real.
- ⚠ `XLAT` is unused in the Fortran and in the C++.
- ⚠ `TIDE_MF` is not checked against `MC = 70` (`:93`), nor `TIDE_NIN`, `TIDE_NINF(k)` against 10;
  larger values read past `TIDE_AMPC`, `TIDE_PHG`, `TIDE_KONIN`, `TIDE_PHGI`, `TIDE_AMPCI` in the
  Fortran and the C++ alike. Not checked whether an input can set them.
- (v) Fresh review 2026-10-16: no defect. `w3tidemd.F90` recompiled from the `build_lab` and
  `build_mixa` flags (disassembly identical to both CMake objects and to each other); the GIMPLE and
  assembly of `tide_predict_only` compared with a fresh C++ unit operation by operation: the trend
  in double with a true division by 8760, the PAREN_EXPR `(VX+UX)` then `·TWPI − PHG·FAC`, the
  `.rodata` bits of both constants, `cosf` from libm in both loops, `AMPC·FX·COS` in that order,
  float `SUM1 + ADD`, and the 0-based offsets of `PREDICTED`, `TIDE_KONIN`, `TIDE_PHGI`,
  `TIDE_AMPCI`. The C++ reloads `TIDE_NINF(k)` after each `VUF` call where the Fortran loads it
  once per `k`; `VUF` does not write it, so no bit changes. Fixed only the path of the
  `src_list.cmake` cite.
- CUDA: not compiled; the function is host only (the `VUF` callable stops the run).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
