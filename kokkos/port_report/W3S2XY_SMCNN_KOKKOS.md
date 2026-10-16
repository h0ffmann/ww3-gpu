# W3S2XY_SMCNN_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Regridding of a field on the Spherical Multiple-Cell (SMC) grid to a regular output grid by
nearest neighbour, without interpolation, for `ww3_ounf` (`SMC%TYPE = 4`). Each output cell takes
the value of the SMC sea point `NNIDX(IX,IY)`; land gets `UNDEF`, a sea point holding `UNDEF`
gets `NOVAL`, and a directional field is turned from radians to degrees nautical,
`MOD(630 - RADE*x, 360)`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3S2XY_SMCNN`, `model/src/w3smcomd.F90:711-743` at NOAA-EMC/WW3 `03adda91`, author C. Bunney. Called only by `W3S2XY_SMC` when `SMCOTYPE == 4` (`w3smcomd.F90:854-856`), itself called by `ww3_ounf` (`ww3_ounf.F90:1277` and the other fields). No `#ifdef` inside the routine. Module `W3SMCOMD` is compiled under switch `SMC` (`switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, regtests `ww3_tp2.10`, `ww3_tp2.14`, `ww3_tp2.16`, `mww3_test_09`; `git grep -lw SMC -- '*switch*'`), by no NCEP or NWS switch |
| Kokkos code | [`../src/ww_kokkos/smco_w3s2xy_smcnn.hpp`](../src/ww_kokkos/smco_w3s2xy_smcnn.hpp), `ww::smco::w3s2xy_smcnn_point` (one cell) and `ww::smco::w3s2xy_smcnn` (the grid, `IX` outer, `IY` inner as `:722-741`). Module state `NXO`, `NYO`, `NNIDX`, `NOVAL` (`w3smcomd.F90:83-113`) and `UNDEF` (`constants.F90:85`, a variable) are arguments |
| C shim | none yet; it passes `S` contiguous (the Fortran dummy is assumed-shape) |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3smcomd.F90` under `model/bin/switch_SMCMlt` (WW3's CMake
Release flags, gfortran 16.2), recompiled and checked identical to the CMake object, 2026-10-16;
the C++ unit compiled with the openmp-release flags (g++ 16.2).

- (v) `RADE` is folded by gfortran into `5.72957763671875e+1` (`0x1.ca5dc0p+5f`); the header writes
  that literal with a `static_assert` that it equals `180./PI` in `REAL`. Both GIMPLEs compute
  `x*RADE`, then `630 - that`, then `fmod(., 360)`, in that order; no FMA.
- (v) `MOD` of `REAL`s is `__builtin_fmodf`, expanded inline as an x87 `fprem` loop in both
  objects; no libm call in the Fortran. The C++ object adds a call of `fmodf` on a NaN result only
  to set `errno` and discards its value, so the stored bits are the `fprem` ones. `fmod` is exact,
  so any correct `fmodf` (CUDA included) gives the same bits.
- (v) `S(ISEA) == UNDEF` is `ucomiss` with the NaN test in both objects; branch order and the four
  stored values (`UNDEF`, `NOVAL`, `S`, the converted direction) match.
- (v) No reduction, no MIN/MAX, no integer conversion of a real.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. The routine has no `#ifdef`; `w3smcomd.F90` recompiled
  under `switch_SMCMlt` and found identical to the CMake object; both GIMPLEs and objects compared
  operation by operation: `.rodata` bits of `RADE`, 630 and 360, `mulss` operand order, `630 - x`,
  the `fprem` loop (the C++ `fmodf` call on a NaN only sets `errno`, its value is discarded), the
  `ucomiss`/`jp` test of `UNDEF`, the four stored values, column-major `NNIDX` and `XY` indexing
  and the 1-based `ISEA`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest reaches it: the regtests' `ww3_ounf` inputs set
`SMC%TYPE` 1 or 2 (`regtests/ww3_tp2.16/input/ww3_ounf.nml:29`, `ww3_tp2.10/input/ww3_ounf.inp:39`),
never 4, which also needs the `NNIDX` file read at `w3smcomd.F90:601`.
