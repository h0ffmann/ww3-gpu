# SETDEPTH_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Marks the nodes of an unstructured grid whose depth is below `DMIN + 1e-5` m as dry
(`IOBDP = 0`, all others 1) before the explicit fluctuation-splitting advection. Its only caller is
`W3XYPUG` (`w3profsmd.F90:202`), which `W3WAVE` calls for `GTYPE .EQ. UNGTYPE` under `PR1`,
`PR2` or `PR3`, and under `MPI` only when `.NOT. LPDLIB` (`w3wavemd.F90:1964-1979`). The PDLIB
path uses `SETDEPTH_PDLIB` in `w3profsmd_pdlib.F90` instead.

## What was ported

| | |
|---|---|
| WW3 routine | `SETDEPTH`, `model/src/w3profsmd.F90:1601-1678` at NOAA-EMC/WW3 `03adda91`; `w3profsmd.F90` is built with switch `PR1`, `PR2` or `PR3`; the only `#ifdef` is `W3_S` (tracing, no arithmetic), so there is one variant |
| Kokkos code | [`../src/ww_kokkos/profs_setdepth.hpp`](../src/ww_kokkos/profs_setdepth.hpp), `ww::profs::setdepth(dmin, iobdp, iobdp_size, nx, dw)`: the module state `DMIN`, `IOBDP(1:MX)` (`INTEGER*1`, 0-based, extent `MX`), `NX` (W3GDATMD) and `DW(0:NSEA)` (W3ADATMD, passed with its 0 element so `dw[ip]` is `DW(IP)`) |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` as WW3's CMake Release build compiles it
(gfortran 16.2.0, `switch_lab_shrd`, recompiled and checked identical to the CMake object),
its `-fdump-tree-optimized-lineno` GIMPLE and disassembly, against a throwaway C++ unit with the
`openmp-release` flags and `-ffp-contract=off`, 2026-10-16.

- (v) `REAL*8, PARAMETER :: DTHR = 10E-6` (`:1667`) has a default-REAL literal: the GIMPLE adds
  `9.99999974737875163555145263671875e-6`, which is `(double)1e-5f`, not `1e-5`. The C++ builds it
  the same way (`static_cast<double>(10E-6f)`, `static_assert` against `0x1.4f8b58p-17`); its
  `.rodata` holds `0x3ee4f8b580000000`, the bits the Fortran object adds.
- (v) Both objects compute the threshold once as `cvtss2sd(DMIN) + DTHR` in double (gfortran
  hoists it out of the loop; the C++ writes it outside) and compare `cvtss2sd(DW(IP))` against it
  with `comisd`/`jbe`, so a NaN depth leaves `IOBDP = 1` in both.
- (v) `IOBDP = 1` (`:1672`) is a byte loop over `LBOUND..UBOUND` in the Fortran object and a
  `memset` in the C++; integer stores, same result.
- (v) No FMA, no libm call, no reduction, no MIN/MAX in either object.
- ⚠ `DW` is a sea-point array (`DW(0:NSEA)`, `w3adatmd.F90:1004`) and `SETDEPTH` indexes it by
  node `IP = 1..NX` (`:1674`). That is right only when every node is a sea point
  (`NSEA = NX`, `MAPSF` the identity); with land nodes on the mesh (`w3gridmd.F90:4876-4883`
  counts `NSEA` from `TMPSTA > 0`) it reads the wrong depth and, for `IP > NSEA`, past the end of
  `DW`. The C++ reproduces the Fortran indexing; the shim must pass the same `DW`.
- CUDA not compiled (optional this batch).
- (v) Fresh review 2026-10-16: no defect. `w3profsmd.F90` recompiled from `build_lab`'s `flags.make` and checked identical to the CMake object; the only `#ifdef` in `:1601-1678` is `W3_S` (tracing). The GIMPLE and disassembly of both objects were compared site by site: one `cvtss2sd(DMIN) + (double)1e-5f` (`.rodata` `0x3ee4f8b580000000` in the C++), `cvtss2sd(DW(IP))`, `comisd` against it (NaN keeps 1), byte stores of 1 then 0, no FMA, no libm call. Nothing was changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Regtests that reach it are unstructured grids run without PDLIB, for
example `ww3_tp2.6` and `ww3_tp2.7` (`switch`, `switch_ST0`, `switch_ST4`), `ww3_tp1.11`,
`ww3_tp2.17` and `ww3_tp2.21` (`switch_NO_PDLIB`), `ww3_tpt1.1` and `mww3_test_07` ⚠ (found by
grepping each regtest's grid input for `UNST` and its switch for no `PDLIB`, not by running them).
