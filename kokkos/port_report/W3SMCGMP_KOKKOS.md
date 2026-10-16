# W3SMCGMP_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The cell of Spherical Multiple-Cell (SMC) grid `IMOD` that holds each of `NC` longitude-latitude
points: longitudes below `XI0 = X0 - 0.5*SX` are wrapped by `+360`, each point becomes the base
cell index `IX1 = FLOOR((XLow - XI0)/DX1)`, `JY1 = FLOOR((YLat - YJ0)/DY1)` with
`DX1 = SX/MRFct`, and the cells `1..NSEA` are scanned in order until every point is matched to a
cell whose range `[I1, I1+I3) x [J2, J2+J4)` from `IJKCel` contains it; `IDCl = 0` means no cell.
Its callers, both under `W3_SMC`: `W3IOBC` (`w3iobcmd.F90:515`), for the boundary points of an
SMC grid read from a nest file, and `WMSMCEQL` (`wmgridmd.F90:5636`), for the boundary points
`ww3_multi` exchanges between grids.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SMCGMP`, `model/src/w3psmcmd.F90:3808-3930` at NOAA-EMC/WW3 `03adda91` (123 lines); compiled by every switch with `SMC`: `model/bin/switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, `regtests/ww3_tp2.10`, `ww3_tp2.16`, `mww3_test_09`, `ww3_tp2.14/input_oasacm6` (`git grep -lw SMC -- '*switch*'`); which of their runs reach `W3IOBC`'s SMC branch or `WMSMCEQL` was not checked ⚠ |
| Kokkos code | [`../src/ww_kokkos/psmc_w3smcgmp.hpp`](../src/ww_kokkos/psmc_w3smcgmp.hpp): `w3smcgmp`, the whole routine as one `KOKKOS_INLINE_FUNCTION` (no allocation; it runs on one thread, see below) |
| Interface | `IMOD` selects `GRIDS(IMOD)`; the function takes that grid's `SX`, `SY`, `X0`, `Y0`, `MRFct`, `NSEA` and `IJKCel` (column 1 onward; it is a POINTER component, `w3gdatmd.F90:660`, so the shim must pass a contiguous copy if it is not contiguous) in its place. The automatic arrays `XLow`, `IX1`, `JY1` (`:3874-3875`) are caller-provided work arrays of `NC` |
| `#ifdef` | none that changes the arithmetic: `W3_S` is a trace call only; the routine has no OpenMP directive, so `W3_OMPG`/`W3_OMPH` do not change it |
| C shim | none yet |
| Plan | [`BATCH3_SHIMS_KOKKOS_PLAN_202610.md`](../../docs/BATCH3_SHIMS_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3psmcmd.F90` built with the flags of WW3's CMake Release build
under `switch_SMCMlt` (gfortran 16.2.0, pinned shell), identical to the CMake object; compared
with a throwaway C++ unit compiled with the openmp-release flags (g++ 16.2.0, warning-free),
2026-10-16.

- (v) Setup: `SX*0.5` then `X0 - that` (`mulss`, `subss`), one `cvtsi2ss` of `MRFct` shared by
  the two `divss` `SX/r` and `SY/r`, in both objects.
- (v) `WHERE(XLow < XI0) XLow = XLow + 360.0` after a copy of `XLon`: a scalar compare and
  `addss 360.0` in both; a NaN fails the test and is kept.
- (v) `FLOOR`: gfortran inlines it as `q = (x - XI0)/DX1` (a true division, no reciprocal; the
  parentheses are a `PAREN_EXPR`), `i = INT(q)`, then `q >= REAL(i) ? i : i-1`. The Fortran object
  vectorises the two `FLOOR` loops (`subps`, `divps`, `cvttps2dq`, `cvtdq2ps`, `cmpleps`, blend,
  with a scalar tail); the C++ keeps them scalar (`vsubss`, `vdivss`, `vcvttss2si`). Each lane is
  the same IEEE operation on the same operands, so the integers are the same. For a NaN or
  out-of-range quotient `cvttps2dq`/`cvttss2si` give `INT_MIN`; `ww::st4::fint` returns the same,
  and the `i-1` wraps to `INT_MAX` through an unsigned subtraction as the object's `paddd` does
  (`INT_MIN` for `q >= 2^31`, `INT_MAX` for NaN or `q < -2^31`), in both.
- (v) Search loop: the test order (`IDCl == 0`, then `IX1 >= I1`, `IX1 < I1+I3`, `JY1 >= J2`,
  `JY1 < J2+J4`), the `EXIT` after the first match and the `ij < NSEM .AND. NFund < NC` guard are
  the same in both GIMPLE dumps; `I1+I3` is a 32-bit add that wraps in the object, written as an
  unsigned add in the C++.
- (v) No FMA in either object; the Fortran is compiled without `-march`, the C++ for x86-64-v3,
  and neither contracts.
- ⚠ Each cell is given to at most one point per scan (the `EXIT LPNBIS` of `:3919`), and a matched
  cell is not visited again: when two points of the list fall in the same cell, the second keeps
  `IDCl = 0` and is reported as outside the grid. This is WW3's behaviour and the C++ reproduces
  it; whether it is intended was not checked.
- ⚠ The routine is sequential by construction (the scan stops when `NFund` reaches `NC`, and the
  result for one point depends on the points before it); it is not split into a per-point
  kernel.
- ⚠ CUDA not compiled for this header.
- (v) Fresh review 2026-10-16: no defect. `w3psmcmd.F90` recompiled from `build_smc`'s flags (identical to the CMake object; the `W3_OMPG` object gives the same code for this routine), its GIMPLE and assembly read against a fresh C++ unit: setup (`mulss`, `subss`, one `cvtsi2ss`, two `divss`), the `WHERE` test (NaN kept), `FLOOR` in the vector body (`cmpleps`, `paddd -1`, blend) and in the scalar tail (`ucomiss` then `sbb`, which takes `i-1` for a NaN) against the C++ `fint` branches (`INT_MAX` for NaN or `q < -2^31`, `INT_MIN` for `q >= 2^31`), the search loop's test order and `EXIT`, and the `IJKCel(4,-9:MCel)` layout (`w3gdatmd.F90:1770`). Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
