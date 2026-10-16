# W3S2XY_SMCRG_DIR_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Area-weighted regridding of a directional field (radians) from the Spherical Multiple-Cell (SMC)
grid to a regular output grid, for `ww3_ounf` with `SMC%TYPE = 2`. Each SMC sea cell adds
`COS(S)*WTS` and `SIN(S)*WTS` to the output cells it spans and `WTS` to their coverage `COV`; an
output cell is then `UNDEF` on land, `NOVAL` when `COV < 0.5`, and otherwise
`MOD(630 - RADE*ATAN2(sum sin, sum cos), 360)`, degrees nautical. Averaging the components rather
than the angles handles the 359 to 0 degree wrap.

## What was ported

| | |
|---|---|
| WW3 routine | `W3S2XY_SMCRG_DIR`, `model/src/w3smcomd.F90:438-510` at NOAA-EMC/WW3 `03adda91`, author C. Bunney. Called only by `W3S2XY_SMC` when `SMCOTYPE == 2` and `DIRN` (`w3smcomd.F90:844-847`), itself called by `S2GRID` in `ww3_ounf` with `FLDIRN = .TRUE.` for `DIR`, `DP`, `PDIR` and `PTHP0` (`ww3_ounf.F90:1352`, `:1364`, `:1513`, `:1529`). No `#ifdef` inside the routine. Module `W3SMCOMD` is compiled under switch `SMC` (`switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, regtests `ww3_tp2.10`, `ww3_tp2.14`, `ww3_tp2.16`, `mww3_test_09`; `git grep -lw SMC -- '*switch*'`), by no NCEP or NWS switch |
| Kokkos code | [`../src/ww_kokkos/smco_w3s2xy_smcrg_dir.hpp`](../src/ww_kokkos/smco_w3s2xy_smcrg_dir.hpp), `ww::smco::w3s2xy_smcrg_dir`, the whole grid, serial (the scatter into `AUX1`, `AUX2`, `COV` keeps the Fortran's `ISMC`, `I`, `J` order per output cell). `RADE`, 630 and 360 are the constants of `smco_w3s2xy_smcnn.hpp` |
| Structural change | `ALLOCATE(AUX1(NXO,NYO), AUX2(NXO,NYO))` (`:452`, freed on return without a `DEALLOCATE`) becomes two caller-provided work arrays. Module state `NXO`, `NYO`, `NSMC`, `SMCIDX`, `XIDX`, `YIDX`, `XSPAN`, `YSPAN`, `WTS`, `MAPSMC`, `NOVAL` (`w3smcomd.F90:83-113`), `UNDEF` (`constants.F90:85`, a variable) and the written `COV` are arguments. `COV(:,:) = 0` zeroes the whole of `COV`; the port zeroes `NXO*NYO`, its allocation at `ww3_ounf.F90:603` |
| C shim | none yet; it passes `S` contiguous (the Fortran dummy is assumed-shape) |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3smcomd.F90` under `model/bin/switch_SMCMlt` (WW3's CMake
Release flags, gfortran 16.2), recompiled and checked identical to the CMake object of
`build_smc`, 2026-10-16; the C++ unit compiled warning-free with the openmp-release flags
(g++ 16.2). Both GIMPLEs and objects compared operation by operation.

- (v) `COS(S)` and `SIN(S)` become one `cexpi`, a call of `sincosf`, in both objects; no libmvec
  variant. `ATAN2` is a call of `atan2f` in both.
- (v) `MOD` of `REAL`s is `__builtin_fmodf`, expanded inline as an x87 `fprem` loop in both
  objects. The C++ object calls `fmodf` on a NaN result only to set `errno` and discards its
  value. `RADE` is the folded `0x1.ca5dc0p+5f` in both `.rodata`; 630, 360, 0.5 and 1.0 have the
  same bits. Order `x*RADE`, `630 - that`, `fmod(., 360)`; no FMA anywhere.
- (v) Accumulation: `AUX1 = AUX1 + COSS*WTS`, `AUX2 = AUX2 + SINS*WTS`, `COV = COV + WTS`, scalar
  `mulss`/`addss` in both, no vectorised reduction; the sum into each output cell follows `ISMC`
  then `I` then `J` as in the Fortran. In each `addss` the product (or `WTS`) is the first
  operand in both objects.
- ⚠ In the two products gfortran puts `WTS` as the first operand of `mulss` and g++ puts
  `COSS`/`SINS`, whatever order the source writes. The product's bits differ only when both
  operands are NaN (the result then carries the first operand's payload). `COSS` is NaN only for
  `S` infinite or NaN and `WTS = MIN(1., ...)` (`w3smcomd.F90:331`), so not in practice; not made
  bit-identical.
- (v) `S(ISEA) == UNDEF` is `ucomiss` with the NaN test in both. `COV < 0.5` is `comiss` against
  0.5 in both. gfortran merges the `COV < 1.0` and `ELSE` branches, which hold the same statements
  (`:496-505`); g++ keeps the test and two copies of the same code, so the stored bits are the same.
- (v) gfortran computes `sincosf` after testing that `XSPAN-1` and `YSPAN-1` are non-negative; the
  port calls it after the `UNDEF` test as the source does. The call has no side effect the Fortran
  observes, so the stored values do not change.
- (v) No `MIN`/`MAX`, no integer conversion of a real; only an upper-bound check on `IX`, `IY` as
  the Fortran (`:472`).
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3smcomd.F90` (identical to the
  `build_smc` object) and the C++ unit; compared GIMPLE and assembly: one `sincosf`, `atan2f`,
  the inline `fprem` loop with the same value kept on a NaN (the C++ `fmodf` call only sets
  `errno`), `RADE` and constants, `addss` operand order, `comiss`/`ucomiss` tests, `COV` and
  `MAPSMC` read with stride `NXO`. Writing `w * coss` does not move `WTS` to the first `mulss`
  operand (checked), so the NaN-payload ⚠ stays. Fixed in this report: the regtest section said
  no regtest reaches the routine; `ww3_tp2.14` `input_oasacm6` does.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. One regtest reaches it: `ww3_tp2.14` with `input_oasacm6`
(`matrix.base:2147`, switch `switch_OASACM6` with `SMC`, an SMC grid of Brest) sets
`SMC%TYPE = 2` and asks `DIR DP` (`regtests/ww3_tp2.14/input_oasacm6/ww3_ounf.nml:12`, `:27`).
It is coupled through OASIS (`-C OASIS`); ⚠ whether the lab's matrix can run it was not checked.
`ww3_tp2.16` and `mww3_test_09` set `SMC%TYPE = 2` (`regtests/ww3_tp2.16/input/ww3_ounf.nml:29`,
`regtests/mww3_test_09/input/ww3_ounf.nml:29`) but output only `HS T01` and `HS` (`:13`), no
directional field; `ww3_tp2.10` uses type 1 (`regtests/ww3_tp2.10/input/ww3_ounf.inp:39`).
