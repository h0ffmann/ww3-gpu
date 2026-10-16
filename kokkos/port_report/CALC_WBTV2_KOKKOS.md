# CALC_WBTV2_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The dominant wave breaking probability b_T = 85.1 [(ε_p - 0.055)(1 + H_s/d)]^2.33, capped at 1, of
the wind-sea part of the spectrum (Babanin et al. 2001, their Fig. 12), with ε_p = k_p H_p / 2 the
peak steepness. Components with c - β U10 cos(θ - φ) < 0, β = 1.2, count as wind sea; σ_p is the
E(σ)^4-weighted mean frequency, k_p comes from `WAVNU1`, and H_p integrates E(σ) over
[0.7 σ_p, 1.3 σ_p]. `W3SNL5` (`:283`) calls it to set the phase-mixing interval of `NL5` only when
`QI5PMX < 0`.

## What was ported

| | |
|---|---|
| WW3 routine | `CALC_WBTv2`, `model/src/w3snl5md.F90:560-711` at NOAA-EMC/WW3 `03adda91` (152 lines); private, one caller, `W3SNL5` (`:283`). Compiled by the three `ww3_ts1` regtest switches with `NL5` (`input_nl5_gaussian`, `input_nl5_growth`, `input_nl5_matrix`; `git grep -lw NL5 -- '*switch*'`), by no NOAA operational switch. None of those regtests sets `NL5PMX < 0` (`gaussian` 0, `growth` and `matrix` 100), so no regtest at the pin executes it |
| Kokkos code | [`../src/ww_kokkos/snl5_calc_wbtv2.hpp`](../src/ww_kokkos/snl5_calc_wbtv2.hpp), `ww::snl5::calc_wbtv2()` (`KOKKOS_INLINE_FUNCTION`); calls `ww::prop::wavnu1()` ([`../src/ww_kokkos/prop_wavnu1.hpp`](../src/ww_kokkos/prop_wavnu1.hpp)) and `ww::st4::mvec_powf()` |
| Structural change | the automatic array `ESIG(NK)` (`:623`, a `malloc`/`free` in the object) is a caller-provided work array of `NK` floats |
| Not ported | `W3_S` tracing (`:630`); no other `#ifdef` in the routine |
| C shim | none yet |
| Translated before | none (checked `kokkos/src/ww_kokkos` and `kokkos/port_report`, 2026-10-16) |

## Arithmetic

Reference: the `-O3` object of `w3snl5md.F90` from WW3-noaa's CMake Release build, gfortran 16.2,
`switch_lab_shrd` with `NL1` replaced by `NL5` (`build_nl5`), recompiled with
`-fdump-tree-optimized-lineno` and identical to the CMake object, 2026-10-16. C++: a throwaway unit
calling `calc_wbtv2`, g++ 16.2 `-O3 -march=x86-64-v3 -ffp-contract=off -fopenmp`, warning-free;
GIMPLE and disassembly of both read operation by operation.

- (v) `SIN(UDIR)` and `COS(UDIR)` become one `sincosf` call in both objects.
- (v) `TFORCE` (`:659-660`) is `TC - (U10*(COSU*ECOS+SINU*ESIN))*1.2` in that order in both;
  `FACTOR = DTH*(SIG/CG)`; `ESIG(IK) + A*FACTOR`.
- (v) The three `SUM`s (`:672`, `:677-678`) are vectorised by gfortran with in-order fold-left
  reductions (lanes added to the accumulator one by one, index order), so a scalar loop in index
  order gives the same bits; g++ also vectorises `ET` with an in-order fold. The products keep the
  Fortran's order: `(SIG*ESIG**4)*DSII` and `ESIG**4*DSII`.
- (v) `ESIG**4.` has a `REAL` exponent and is `powf`, not a multiplication: the object calls
  `_ZGVbN4vv_powf` for each group of four from the first element and `powf` for the last `NK mod 4`,
  separately in the numerator and the denominator loops. The C++ calls the same two functions with
  the same grouping through `st4_libmvec.hpp`, twice, as the Fortran does.
- (v) `MIN`/`MAX`: `MAX(0.,ET)`, `MAX(0.,ETP)` and `MAX(0.,WSTP-0.055)` are `maxss` with the
  variable as destination (NaN gives 0), `MAX(1E-10,SUM)` the same with the sum as destination (NaN
  gives 1E-10), `MIN(1.0,TWBT)` is `minss` with `TWBT` as destination (NaN gives 1). The C++ writes
  each as `x > c ? x : c` / `x < c ? x : c`; g++ emits `vmaxss`/`vminss` with the same operand roles,
  or a compare and select with the same result for `1E-10`.
- (v) `SQRT` is `sqrtss` in both (g++ adds an errno call to `sqrtf` on the negative path, same
  result). `WSTP = (KP*0.5)*HSP`. `(1+HS/DPT)*MAX(...)` then `powf(., 2.33)` then `*85.1`.
- (v) Every `REAL` literal has the same bits in both `.rodata` (1.2, 0.055, 85.1, 2.33, 1E-10,
  1E-7, 0.7, 1.3, 0.5, 4.0).
- (v) `WAVNU1` is an out-of-line call to `__w3dispmd_MOD_wavnu1` in the Fortran object; the C++ inlines
  `prop::wavnu1`, whose parity with that routine is its own report's.
- ⚠ With `NK <= 0` the Fortran reads `SIG(NK)` at `:679` (`SIG(0)` for `NK = 0`, allocated as
  `SIG(0:NK+1)`, `w3gdatmd.F90:2066`); the C++ reads `sig[nk-1]` with `sig` at `SIG(1)`, the same
  element. Never reached in a valid grid.
- ⚠ On a device there is no libmvec: `mvec_powf` falls back to `powf` per element, which is not
  bit for bit with the Fortran's grouping. CUDA not compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
