# W3S2XY_SMCRG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Area-weighted regridding of a scalar field on the Spherical Multiple-Cell (SMC) grid to a regular
output grid, for `ww3_ounf` (`SMC%TYPE = 2`). Each SMC sea point `ISEA` in `SMCIDX` adds
`S(ISEA)*WTS(ISEA)` to every output cell it spans, and `WTS(ISEA)` to that cell's coverage `COV`;
points holding `UNDEF` are skipped. A cell with `MAPSMC = 0` becomes `UNDEF`, one with coverage
below 0.5 becomes `NOVAL`, and one with coverage below 1 is rescaled by `1/COV`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3S2XY_SMCRG`, `model/src/w3smcomd.F90:359-418` at NOAA-EMC/WW3 `03adda91`, author C. Bunney. Called only by `W3S2XY_SMC` when `SMCOTYPE == 2` and the field is not directional (`w3smcomd.F90:844-850`), itself called by `ww3_ounf` (`ww3_ounf.F90:1277` and the other fields). No `#ifdef` inside the routine. Module `W3SMCOMD` is compiled under switch `SMC` (`switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, regtests `ww3_tp2.10`, `ww3_tp2.14`, `ww3_tp2.16`, `mww3_test_09`; `git grep -lw SMC -- '*switch*'`), by no NCEP or NWS switch |
| Kokkos code | [`../src/ww_kokkos/smco_w3s2xy_smcrg.hpp`](../src/ww_kokkos/smco_w3s2xy_smcrg.hpp), `ww::smco::w3s2xy_smcrg` (the whole routine, serial) and `ww::smco::w3s2xy_smcrg_mask_point` (the masking of one cell, `:401-412`). Module state `NXO`, `NYO`, `NSMC`, `SMCIDX`, `XIDX`, `YIDX`, `XSPAN`, `YSPAN`, `WTS`, `COV`, `MAPSMC`, `NOVAL` (`w3smcomd.F90:83-120`) and `UNDEF` (`constants.F90:85`, a variable) are arguments; `COV` (allocated `(NXO,NYO)` at `ww3_ounf.F90:603`) is written |
| C shim | none yet; it passes `S` contiguous (the Fortran dummy is assumed-shape) |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3smcomd.F90` under `model/bin/switch_SMCMlt` (WW3's CMake
Release flags, gfortran 16.2), recompiled and checked identical to the CMake object, 2026-10-16;
the C++ unit compiled warning-free with the openmp-release flags (g++ 16.2).

- (v) The accumulation is a scatter sum in `ISMC` order (`:374-396`), not vectorised in the
  object (`-fopt-info-vec-optimized` reports nothing on `:359-418`). The port keeps it serial, in
  the Fortran loop order; the order of the additions into one cell is the Fortran's only on one
  thread, so the routine is not split into a parallel kernel.
- (v) `XY + S*WTS`: both objects multiply `S` by `WTS` first, then add with the product as first
  operand; no FMA.
- ⚠ `COV + WTS`: gfortran emits `WTS + COV` (`addss` with `WTS` as destination), g++ emits
  `COV + WTS`, whatever order the C++ source writes. The sum is the same; only when both operands
  are NaN with different payloads could the stored NaN differ. Not reproducible without inline
  assembly; not translated further.
- (v) `XY*(1.0/COV)` (`:411`): the Fortran parentheses are a `PAREN_EXPR`; both objects do
  `divss` of 1.0 by `COV`, then `mulss` with the reciprocal as first operand. g++ does not fold it
  into `XY/COV` under these flags, so no `__builtin_assoc_barrier` is needed.
- (v) `S(ISEA) == UNDEF` is `ucomiss` in both objects (operands swapped, equality is symmetric);
  `COV < 0.5` and `COV < 1.0` are `comiss` against the `REAL` constants 0.5 and 1.0 in both, in
  the Fortran's branch order. `UNDEF` and `NOVAL` are loaded once, as in the Fortran object.
- (v) The zeroing of `COV(:,:)` and `XY(:,:)` is a `memset` in both objects; `IX > NXO .OR. IY >
  NYO` is evaluated without short circuit in the Fortran object and has no side effect.
- (v) No libm call, no MIN/MAX, no conversion of a real to an integer.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. The Fortran object was rebuilt under `switch_SMCMlt` and
  found identical to the CMake object; its GIMPLE and assembly for `:359-418` were compared
  operation by operation with the C++ unit's: the `memset` zeroing, `ucomiss` against `UNDEF` with
  the same `jp`/`je` NaN path, `S*WTS` then `+ XY`, the `COV` sum, `comiss` against 0.5 and 1.0,
  `divss` 1/`COV` then `mulss` by `XY`. The one operand-order difference (`WTS + COV` against
  `COV + WTS`) stays as written above; writing `wts + cov` in the C++ source leaves the g++ object
  unchanged. `WTS` comes from integer cell counts (`:330-331`), so a NaN there could only be the
  default NaN of `0/0` when `CELFAC` is 0, which carries one payload, and the stored bits would
  match. Nothing was changed in the header.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. `ww3_tp2.16` sets `SMC%TYPE = 2`
(`regtests/ww3_tp2.16/input/ww3_ounf.nml:29`) and is in `matrix.base` (`regtests/bin/matrix.base:1887`),
so its `ww3_ounf` step reaches this routine for every non-directional field once a shim exists.
