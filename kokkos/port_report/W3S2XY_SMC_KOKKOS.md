# W3S2XY_SMC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Entry point that puts a field held on the Spherical Multiple-Cell (SMC) sea points onto the
output grid of `ww3_ounf`, by `SMCOTYPE`: 1, the flat sea-point array
`PACK(S, SMCMASK)`, turned from radians to degrees nautical when `DIR`; 2, area-weighted
regridding (`W3S2XY_SMCRG`, or `W3S2XY_SMCRG_DIR` for a direction); 3, nearest neighbour with a
gradient correction (`W3S2XY_SMCNN_INT`); 4, nearest neighbour (`W3S2XY_SMCNN`). Any other value
writes a message and stops.

## What was ported

| | |
|---|---|
| WW3 routine | `W3S2XY_SMC`, `model/src/w3smcomd.F90:816-863` at NOAA-EMC/WW3 `03adda91`, author C. Bunney. Called by `ww3_ounf` only: `S2GRID` (`ww3_ounf.F90:3925`, with `FLDIRN`) and directly at `ww3_ounf.F90:1277`, `:1278`, `:1292`, `:1414`, `:1863`, `:1864` (no `DIR`). No `#ifdef` inside the routine. Module `W3SMCOMD` is compiled under switch `SMC` (`switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, regtests `ww3_tp2.10`, `ww3_tp2.14`, `ww3_tp2.16`, `mww3_test_09`; `git grep -lw SMC -- '*switch*'`), by no NCEP or NWS switch |
| Kokkos code | [`../src/ww_kokkos/smco_w3s2xy_smc.hpp`](../src/ww_kokkos/smco_w3s2xy_smc.hpp), `ww::smco::w3s2xy_smc`, an `inline` host function template: type 3 calls `w3s2xy_smcnn_int`, which calls `SMCGradn` through the callable it is given. Types 2 to 4 call the translated callees of `smco_w3s2xy_smcrg.hpp`, `smco_w3s2xy_smcrg_dir.hpp`, `smco_w3s2xy_smcnn_int.hpp`, `smco_w3s2xy_smcnn.hpp` with their declared signatures |
| Structural change | The `WRITE(*,*) "Uknonwn SMC type!", SMCOTYPE` and `STOP` of `:857-860` become the return value `kUnknownType` (1), returned before `XY` is touched, as in the Fortran; the shim writes the same line to unit `*` and stops. The OPTIONAL `DIR` is a `const bool*`, `nullptr` when absent. Module state (`SMCOTYPE`, `NXO`, `NYO`, `SMCMASK`, `NOVAL`, `NSMC`, `SMCIDX`, `XIDX`, `YIDX`, `XSPAN`, `YSPAN`, `WTS`, `COV`, `MAPSMC`, `NNIDX`, `XDIST`, `YDIST`; `w3smcomd.F90:82-119`) and `UNDEF` (`constants.F90:85`, a variable) are arguments; the callees' work arrays (`AUX1`, `AUX2`, `CVQ`, `GrdX`, `GrdY`) are caller-provided |
| C shim | none yet; it passes `S` contiguous (the Fortran dummy is assumed-shape), `SMCMASK` as `LOGICAL(4)`, converts `DIR` to `bool`, binds the Fortran `SMCGradn` (`w3psmcmd.F90:2341`) as the callable, and on 1 writes the message and stops |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3smcomd.F90` under `model/bin/switch_SMCMlt` (WW3's CMake
Release flags, gfortran 16.2), recompiled and checked identical to the CMake object of
`build_smc`, 2026-10-16; the C++ unit compiled warning-free with the openmp-release flags
(g++ 16.2). Both GIMPLEs and objects compared operation by operation. The arithmetic of types 2
to 4 is the callees', compared in their own reports; here gfortran calls them, g++ inlines them.

- (v) Dispatch: `SMCOTYPE` tested against 1, 2, 3, 4 in that order in both objects; `DIRN` is
  `DIR` when present, else false (`:827-831`).
- (v) Type 1, `XY(:,1) = PACK(S, SMCMASK)` (`:835`) is a call of `_gfortran_pack`, which
  dispatches a `REAL(4)` array to `pack_r4` (dtype `0x230`, read in the disassembly of the pinned
  `libgfortran.so.5.0.0`). `pack_r4` tests the low byte of each `LOGICAL(4)` mask element
  (`cmpb $0x0`) and copies with `movss` (no conversion, NaN payloads kept); without bounds
  checking it writes `COUNT(SMCMASK)` elements and checks no size. The port walks `S` in order,
  tests the mask's low byte, and copies with `vmovss`. `ww3_ounf.F90:591-592` sets
  `NXO = COUNT(SMCMASK)`, so the pack fills `XY(1:NXO,1)` exactly when `SIZE(S) = NSEA`.
- (v) Type 1 with `DIR` (`:838-842`): `XY /= UNDEF` is `ucomiss` with the parity (NaN) branch in
  both; then `mulss` with `XY` as first operand and the folded `RADE = 0x1.ca5dc0p+5f`
  (`5.72957763671875e+1` in the GIMPLE) as second, in both; `630 - that`; `MOD(., 360.)` is
  `__builtin_fmodf` expanded as an x87 `fprem` loop in both. The C++ object additionally calls
  `fmodf` on a NaN result only to set `errno` and discards its value. No FMA.
- (v) No reduction, no `MIN`/`MAX`, no integer conversion of a real in this routine.
- ⚠ CUDA not compiled. The routine is host-only anyway (it calls `SMCGradn`).
- (v) Fresh review 2026-10-16: no defect. `w3smcomd.F90` recompiled under `switch_SMCMlt` and found identical to the CMake object of `build_smc`; the C++ unit recompiled; compared the GIMPLE and objects of the dispatch, the `_gfortran_pack` call (destination is `XY(:,1)` itself, no temporary, so an over-long pack spills into `XY(:,2:)` in both), the low-byte mask test, the `DIR` loop (`UNDEF` compare, `XY * 0x1.ca5dc0p+5f`, `630 -`, `fmodf` as `fprem`), and the callees' argument order. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. `ww3_tp2.10` sets SMC output type 1
(`regtests/ww3_tp2.10/input/ww3_ounf.inp:39`) with fields `WND HS T0M1`, so it reaches the `PACK`
branch; whether one of its fields reaches the `DIR` branch was not checked ⚠. `ww3_tp2.16` and
`mww3_test_09` reach type 2 without `DIR` (`ww3_ounf.nml:13`, `:29`). No regtest sets type 3 or 4.
