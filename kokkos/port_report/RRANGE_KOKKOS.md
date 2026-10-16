# RRANGE_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

`RRANGE(X0,X1,DX)`, a private helper of the ST6 source-term module `W3SRC6MD` that builds a
`REAL` vector of evenly spaced integers. `LFACTOR` (`w3src6md.F90:956`) and `TAU_WAVE_ATMOS`
(`:1249`) call it as `IK10Hz = RRANGE(1,NK10Hz,1)`, the wavenumber index of the spectrum extended
to 10 Hz, used as a real exponent in `SIG10Hz = SIG(1)*XFR**(IK10Hz-1.0)` (`:996`, `:1285`).
`IK10Hz` is a module variable, `ALLOCATABLE, SAVE` (`:105`), `!$OMP THREADPRIVATE` (`:107`) only
under `OMPG` (the guard at `:106` tests `W3_OMPG` alone), built by whichever caller runs first on
each thread.

## What was ported

| | |
|---|---|
| WW3 routine | `RRANGE` of `W3SRC6MD`, `model/src/w3src6md.F90:1387-1416` at NOAA-EMC/WW3 `03adda91`; author S. Zieger. Compiled with `ST6`: 61 switch files (`git grep -lw ST6 -- '*switch*'`; `ww3_ts1`, `ww3_ts2`, `ww3_ts3`, `ww3_tp2.15`, `mww3_test_05`, `model/bin/switch_NRL3`, `switch_UoM_*`, `switch_swin`); no NCEP or NWS operational switch. No `#ifdef` inside the function |
| Kokkos code | [`../src/ww_kokkos/srce_st6_util.hpp`](../src/ww_kokkos/srce_st6_util.hpp), `ww::srce::st6::rrange(x0, x1, dx, rx)`; the caller sizes `rx` with `range_size(x0, x1, dx)` (shared with `irange`), and the function fills it and returns that size |
| C shim | none yet. `IK10Hz` is a per-thread `SAVE` table: the host builds it once; the `SIZE(IK10Hz) /= NK10Hz` stop (`:958-961`, `:1251-1254`) becomes a host check |
| Plan | issue #42, branch `goal/42-port-st-flx-swl` |

## Arithmetic

Reference: the `-O3` object of `w3src6md.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd` with `ST6 FLX4`), checked identical in its disassembly to the CMake object;
compared with a throwaway C++ unit compiled with the `openmp-release` flags, 2026-10-16. The
`FLX5` object has the same disassembly of `__w3src6md_MOD_rrange`.

- (v) gfortran does not inline `RRANGE`: `lfactor` and `tau_wave_atmos` `call
  __w3src6md_MOD_rrange` (GIMPLE at `:956`, `:1249`), so the standalone function is the reference.
- (v) `N` (`:1410`) as in `IRANGE`: `cvtsi2ss`, `cvtsi2ss`, `divss`, `cvttss2si`, `+1`; the C++
  through `ww::st4::fint` (NaN or out of range gives `INT_MIN`, as `cvttss2si`).
- (v) `RX(I) = REAL(X0+(I-1)*DX)` (`:1412-1414`): the sum in `INTEGER`, then one `cvtsi2ss`
  (round to nearest). Vectorised by 4 in the object, by 8 and 4 in the C++; elements are
  independent, so the same bits.
- (v) `N <= 0`: zero-size result, nothing written, 0 returned.
- (v) No libm or libmvec call, no reduction, no `MIN`/`MAX`, no floating-point constant.
- ⚠ Exact only while the integers are below 2^24 (`REAL` of `N`'s operands and of each element);
  `NK10Hz` is a few tens. Reproduced, not fixed.
- ⚠ In WW3: `IK10Hz` keeps the `NK10Hz` of the first call on each thread; the size check runs only
  at that allocation, so a later grid with another `NK10Hz` reads the old table without a stop
  (`:955-962`). Reproduced, not fixed; it matters only under `ww3_multi` with several ST6 grids.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, pinned `#cuda` shell): `I2FP`, IEEE division,
  `F2I.TRUNC` behind the range test. Never run.
- (v) Fresh review 2026-10-16: no arithmetic defect. The reviewer recompiled `w3src6md.F90` and `w3swldmd.F90` from the `ST6 FLX4` reference flags (disassembly identical to the CMake objects; `w3src6md` also under `ST6 FLX5`, identical) and a C++ unit including `srce_st6_util.hpp` and `srce_swl_util.hpp` together (warning-free, no clash), and compared their GIMPLE and assembly: the `N` chain, the integer sum then one `cvtsi2ss` per element, the zero-size path, and the two calls at `:956`, `:1249`. Fixed in this report only: `THREADPRIVATE` holds under `OMPG` alone.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
