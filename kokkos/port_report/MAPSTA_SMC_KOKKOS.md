# MAPSTA_SMC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Land/sea mask of the regular output grid that `ww3_ounf` regrids a Spherical Multiple-Cell (SMC)
field onto (`SMC%TYPE = 2`). Each SMC sea point whose `MAPSTA` is non-zero adds its regridding
weight `WTS(ISEA)` to the coverage `COV(IX,IY)` of every output cell it spans; a cell with
`COV < 0.5` is land (`MAPSMC = 0`), any other cell is sea (`MAPSMC = 1`).

## What was ported

| | |
|---|---|
| WW3 routine | `MAPSTA_SMC`, `model/src/w3smcomd.F90:518-568` at NOAA-EMC/WW3 `03adda91`, author C. Bunney. Called only by `ww3_ounf` at each output time when `SMCGRD .AND. SMCOTYPE == 2` (`ww3_ounf.F90:1170`, under `W3_SMC`). No `#ifdef` inside the routine. Module `W3SMCOMD` is compiled under switch `SMC` (`switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, regtests `ww3_tp2.10`, `ww3_tp2.14`, `ww3_tp2.16`, `mww3_test_09`; `git grep -lw SMC -- '*switch*'`), by no NCEP or NWS switch |
| Kokkos code | [`../src/ww_kokkos/smco_mapsta_smc.hpp`](../src/ww_kokkos/smco_mapsta_smc.hpp), `ww::smco::mapsta_smc`, the whole grid on one thread: `COV` is a scatter sum in `ISEA` order, so only a serial loop keeps the Fortran's order of additions into a cell. Module state `NSEA`, `MAPSF`, `MAPSTA` (`w3gdatmd`), `NXO`, `NYO`, `XIDX`, `YIDX`, `XSPAN`, `YSPAN`, `WTS`, `COV`, `MAPSMC` (`w3smcomd.F90:83-99`) are arguments; `MAPSF(MSEA,3)` and `MAPSTA(MY,MX)` take their leading dimension |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3smcomd.F90` under `model/bin/switch_SMCMlt` (WW3's CMake
Release flags, gfortran 16.2), recompiled and checked identical to the CMake object, 2026-10-16;
the C++ unit compiled with the openmp-release flags (g++ 16.2), warning-free.

- (v) The one floating-point operation, `COV(IX,IY) + WTS(ISEA)` (`:548`), is `addss` with `COV`
  in the register and `WTS` from memory in the Fortran and `vaddss` with the same operands in the
  C++; no FMA, no vectorisation in either object.
- (v) `COV(IX,IY) .LT. 0.5` (`:558`) is `comiss` against `0.5` then `setbe` in both objects: a
  NaN coverage gives `MAPSMC = 1` in both.
- (v) `COV(:,:) = 0.0` and `MAPSMC(:,:) = 0` are two `memset` calls in both objects, over
  `NXO*NYO` elements (the allocation, `ww3_ounf.F90:603`).
- (v) Branch order kept: `XIDX == -1` skip, then the span loops, the `IX > NXO .OR. IY > NYO`
  skip, the `MAPSTA(IMY,IMX) /= 0` test; column-major indexing of `MAPSF`, `MAPSTA`, `COV`,
  `MAPSMC`. No reduction other than the scatter sum, no MIN/MAX, no conversion of a real.
- ⚠ No lower-bound test on `IX`, `IY`, as in the Fortran: it relies on `SMC_INTERP` setting
  `XIDX`, `YIDX` to -1 or a value in `1..NXO`, `1..NYO` (`w3smcomd.F90:303-311`).
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no difference in arithmetic. The `w3smcomd.F90` object was recompiled
  from `build_smc`'s flags and found identical to the CMake object. Compared with the C++ object:
  one `addss`/`vaddss` with `COV` in the register and `WTS` from memory, `comiss`/`vcomiss` against
  `0.5` (`0x3f000000`) then `setbe`, two `memset` calls, no FMA, no vectorisation. Fixed: the
  `memset` count was `NXO*NYO`. The Fortran object takes it from `COV`'s descriptor, with each
  extent clamped at 0 (`cmovs`), so a negative `NXO` and `NYO` gave a positive count and an
  out-of-bounds write in the C++. The count now clamps each extent, and the object was read again
  (`cmovns`, the same `memset` calls).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. The regtest `ww3_tp2.16` sets `SMC%TYPE = 2`
(`regtests/ww3_tp2.16/input/ww3_ounf.nml:29`), and `ww3_tp2.10` sets type 1 or 2 in its
`ww3_ounf.inp`, so its checkpoint (ADR-0011) can exercise it through `ww3_ounf` once a shim and
the caller's hunk exist.
