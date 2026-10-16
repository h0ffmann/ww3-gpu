# W3S2XY_SMCNN_INT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Regridding of a field on the Spherical Multiple-Cell (SMC) grid to a regular output grid by
nearest neighbour with a gradient correction, for `ww3_ounf` (`SMC%TYPE = 3`). Each output cell
takes `S(ISEA) + GrdX(ISEA)*XDIST(IX,IY) + GrdY(ISEA)*YDIST(IX,IY)`, with `ISEA = NNIDX(IX,IY)`
and the gradients from `SMCGradn`; land gets `UNDEF`, and a directional field is turned from
radians to degrees nautical, `MOD(630 - RADE*x, 360)`. Unlike `W3S2XY_SMCNN`, a sea point
holding `UNDEF` is not replaced by `NOVAL` (`w3smcomd.F90:784-794`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3S2XY_SMCNN_INT`, `model/src/w3smcomd.F90:762-798` at NOAA-EMC/WW3 `03adda91`, author C. Bunney. Called only by `W3S2XY_SMC` when `SMCOTYPE == 3` (`w3smcomd.F90:851-853`), itself called by `ww3_ounf` (`ww3_ounf.F90:1277` and the other fields). No `#ifdef` inside the routine. Module `W3SMCOMD` is compiled under switch `SMC` (`switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, regtests `ww3_tp2.10`, `ww3_tp2.14`, `ww3_tp2.16`, `mww3_test_09`; `git grep -lw SMC -- '*switch*'`), by no NCEP or NWS switch |
| Kokkos code | [`../src/ww_kokkos/smco_w3s2xy_smcnn_int.hpp`](../src/ww_kokkos/smco_w3s2xy_smcnn_int.hpp): `ww::smco::w3s2xy_smcnn_int_cvq` (the copy `CVQ(1:NSEA) = S`, `:778`), `ww::smco::w3s2xy_smcnn_int_point` (one cell, `:784-794`) and `ww::smco::w3s2xy_smcnn_int` (host: copy, `SMCGradn`, then the grid, `IX` outer, `IY` inner as `:782-796`). Module state `NXO`, `NYO`, `NNIDX`, `XDIST`, `YDIST` (`w3smcomd.F90:83-114`, all `(NXO,NYO)` as `READ_SMCINT` allocates them, `:597`) and `UNDEF` (`constants.F90:85`, a variable) are arguments. The automatic arrays `CVQ(-9:NSEA)`, `GrdX(NSEA)`, `GrdY(NSEA)` (heap-allocated by gfortran per call) are caller-provided work |
| Not ported | `SMCGradn` (`w3psmcmd.F90:2341-2539`), the callee: the host driver takes it as a callable, which the shim binds to the Fortran routine. A kernel can only run `w3s2xy_smcnn_int_point` once the gradients exist |
| C shim | none yet; it passes `S` contiguous (the Fortran dummy is assumed-shape) |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3smcomd.F90` under `model/bin/switch_SMCMlt` (WW3's CMake
Release flags, gfortran 16.2), recompiled and checked identical to the CMake object, 2026-10-16;
the C++ unit compiled with the openmp-release flags (g++ 16.2), warning-free.

- (v) `:789` is `GrdX*XDIST`, `+ S`, `GrdY*YDIST`, `+` in both GIMPLEs and both objects
  (`mulss`, `addss`, `mulss`, `addss`); no FMA, no vectorisation of the cell loop in either.
- (v) `RADE` is folded to `5.72957763671875e+1` (`0x42652ee0`), `630.` and `360.` are
  `0x441d8000` and `0x43b40000`, the same three words in `.rodata.cst4` of both objects; the
  constants are those of `smco_w3s2xy_smcnn.hpp` (`static_assert` there).
- (v) `MOD` of `REAL`s is an inline x87 `fprem` loop in both objects, no libm call in the Fortran;
  the C++ object calls `fmodf` on a NaN result only for `errno` and keeps the `fprem` bits.
- (v) The copy `CVQ(1:NSEA) = S` copies `SIZE(S)` words to `CVQ+40` bytes (`memcpy`, or a strided
  loop) in the Fortran, and the same `SIZE(S)` words to `cvq+10` in the C++ (vectorised copy; a
  copy has no rounding). `SMCGradn` receives the base of `CVQ`, i.e. `CVQ(-9)`, in both
  (`smcgradn (_88, ...)` in the GIMPLE).
- ⚠ WW3 defect, reproduced, not fixed: `SMCGradn` declares `CVQ(NSEA)` explicit-shape
  (`w3psmcmd.F90:2353`), so the `CVQ(-9:NSEA)` actual (`w3smcomd.F90:774,779`) is
  sequence-associated from `CVQ(-9)`: `SMCGradn` reads the ten never-written `CVQ(-9:0)`
  (uninitialised heap memory) as its cells 1 to 10, and `S(1:NSEA-10)` as cells 11 to `NSEA`.
  Every cell index `k` that `SMCGradn`'s face loops read therefore holds `S(k-10)`, so the
  gradients use the grid's topology with the field shifted by ten sea points. The C++ passes
  the same pointer and leaves `cvq[0..9]` untouched, so its output depends on what the caller's
  buffer holds there, as the Fortran's depends on `malloc`; bits match only if both hold the same.
- (v) No reduction, no MIN/MAX, no integer conversion of a real.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. Re-read `:762-798` (no `#ifdef`) and the
  translator's `-O3` GIMPLE and objects of `w3smcomd.F90` (`switch_SMCMlt`) and of the C++ unit:
  same `mulss`/`addss` order at `:789`, same folded `RADE`, `630.`, `360.`, same `fprem` loop
  (the C++ `fmodf` call on NaN only for `errno`, its result discarded), `XY` index
  `(IX-1)+NXO*(IY-1)`, and `smcgradn (_88, ...)` with `_88` the `malloc` base of `CVQ(-9:NSEA)`,
  the copy at `_88+40` bytes; the C++ passes `cvq` and copies to `cvq+10`. The WW3 defect above
  is confirmed against `w3psmcmd.F90:2353,2366-2367`. One sentence of this bullet list made
  more precise; the header unchanged.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest reaches it: the regtests' `ww3_ounf` inputs set
`SMC%TYPE` 1 or 2 (`regtests/ww3_tp2.16/input/ww3_ounf.nml:29`, `ww3_tp2.10/input/ww3_ounf.inp:39`),
never 3, which also needs the `smcint.ww3` file read by `READ_SMCINT` (`w3smcomd.F90:589-601`);
no such file is in the WW3 repository.
