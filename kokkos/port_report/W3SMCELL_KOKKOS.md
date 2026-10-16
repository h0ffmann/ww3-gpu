# W3SMCELL_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Centre longitude and latitude, in degrees, of a list of Spherical Multiple-Cell (SMC) cells of
grid `IMOD`: `XLon = XI0 + (I1 + 0.5*I3)*DX1`, `YLat = YJ0 + (J2 + 0.5*J4)*DY1` from the cell's
`IJKCel` column, with `XI0 = X0 - 0.5*SX`, `DX1 = SX/MRFct` (and the same in y); an id outside
`1..NSEA` gets `(0, -90)`, and negative longitudes are wrapped into `[0, 360)`. Its only caller
is `WMSMCEQL` (`wmgridmd.F90:5557`, under `W3_SMC`), which `WMINIT` calls (`wminitmd.F90:3034`,
`:6118`) to match boundary points of grids in `ww3_multi`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SMCELL`, `model/src/w3psmcmd.F90:3665-3784` at NOAA-EMC/WW3 `03adda91` (120 lines); compiled by every switch with `SMC`: `model/bin/switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, `regtests/ww3_tp2.10`, `ww3_tp2.16`, `mww3_test_09`, `ww3_tp2.14/input_oasacm6` (`git grep -lw SMC -- '*switch*'`); called only from `ww3_multi`, so of these only `mww3_test_09` can exercise it ⚠ (not checked that its run reaches `WMSMCEQL`) |
| Kokkos code | [`../src/ww_kokkos/psmc_w3smcell.hpp`](../src/ww_kokkos/psmc_w3smcell.hpp): `w3smcell_point` (one iteration of `DO ij`, `:3757-3775`) and the host driver `w3smcell` (the setup of `:3743-3750`, the loop, the `WHERE` of `:3781`) |
| Interface | `IMOD` selects `GRIDS(IMOD)`; the driver takes that grid's `SX`, `SY`, `X0`, `Y0`, `MRFct`, `NSEA` and `IJKCel` in its place |
| `#ifdef` | `W3_OMPG` makes `DO ij` a parallel DO; the outlined body `w3smcell._omp_fn.0` has the same arithmetic (below). `W3_S` is a trace call only |
| C shim | none yet |
| Plan | [`BATCH3_SHIMS_KOKKOS_PLAN_202610.md`](../../docs/BATCH3_SHIMS_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3psmcmd.F90` built with the flags of WW3's CMake Release build
under `switch_SMCMlt` (gfortran 16.2.0, pinned shell), identical to the CMake object, and the same
file with `-DW3_OMPG -fopenmp`; compared with a throwaway C++ unit compiled with the
openmp-release flags (g++ 16.2.0, warning-free), 2026-10-16.

- (v) Setup: `SX*0.5` then `X0 - that`, one `cvtsi2ss` of `MRFct` shared by the two divisions
  `SX/r` and `SY/r`. The serial object does it with `mulss`, `subss`, `divss`; the `W3_OMPG`
  object packs the x and y pairs into one `mulps`, `subps` and `divps`, which give the same bits
  per lane.
- (v) Cell centre: `cvtsi2ss` of the four `IJKCel` entries, `0.5*I3`, `+ I1`, `* DX1`, `+ XI0`;
  the parentheses of `:3772-3773` are a `PAREN_EXPR` and the C++ keeps the same grouping. The
  `mulss`/`addss` sequence and the role of each operand (the running value is the first source)
  are the same in both objects; no FMA in either.
- (v) Out-of-range ids store `0.0` and `-90.0`; the test is `ijp <= 0 | NSEA < ijp` in both.
- (v) `WHERE(XLon < 0.0)`: a scalar `comiss` and `addss 360.0` loop in both objects; neither
  vectorises it. A NaN fails the test and is kept.
- (v) `W3_OMPG`: each iteration writes only its own `ij`, so the parallel loop gives the serial
  result; `w3smcell_point` can run per `ij` in a parallel loop.
- ⚠ CUDA not compiled for this header.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3psmcmd.F90` under `switch_SMCMlt`
  (identical to the CMake object) and with `-DW3_OMPG -fopenmp`, compiled a C++ unit with the
  openmp-release flags, and compared the optimised GIMPLE and the assembly of the setup, the cell
  loop (operation order, operand roles, `PAREN_EXPR` grouping, the range test) and the `WHERE`
  loop: same operations in the same order. Fixed only the setup bullet above, which described
  the `W3_OMPG` object as scalar.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
