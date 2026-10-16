# CALC_INTERP_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Setup of the experimental SMC output types 3 and 4 (regridding of a Spherical Multiple-Cell grid
to an arbitrary regular grid). For each output cell `(I,J)` it searches, by brute force over all
sea points, the SMC cell that contains the output cell centre, and stores its index in `NNIDX` and
the east and north distances between the two centres, in metres, in `XDIST` and `YDIST`:
`XDIST = (lon - mlon) * DERA * RADIUS * CLATS(ISEA)`, `YDIST = (lat - mlat) * DERA * RADIUS`.

## What was ported

| | |
|---|---|
| WW3 routine | `CALC_INTERP`, `model/src/w3smcomd.F90:622-696` at NOAA-EMC/WW3 `03adda91`, author C. Bunney. **Called by nothing**: `git grep -i calc_interp` in `WW3-noaa` finds only its definition. The types 3 and 4 read `NNIDX`, `XDIST`, `YDIST` from a `ww3_smcint` file instead (`READ_SMCINT`, `:579-605`). Translated for completeness; it can never be exercised. Module `W3SMCOMD` is compiled under switch `SMC` (`switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, regtests `ww3_tp2.10`, `ww3_tp2.14`, `ww3_tp2.16`, `mww3_test_09`; `git grep -lw SMC -- '*switch*'`), by no NCEP or NWS switch. One `#ifdef`: `W3_RTD` (`:626-629`, `:635-637`, `:662-669`), set with `SMC` only in `switch_UKMO_uk` |
| Kokkos code | [`../src/ww_kokkos/smco_calc_interp.hpp`](../src/ww_kokkos/smco_calc_interp.hpp), `ww::smco::calc_interp`, an `inline` host function (it allocates its automatic arrays `mlon`, `mlat`, `olon`, `olat`, `:633`, as `std::vector`). Module state is passed in: `NSEA`, `NRLv`, `SX`, `SY`, `X0`, `Y0`, `NX`, `NY`, `IJKCel` (pointer to `IJKCel(1,-9)`), `CLATS` (pointer to `CLATS(0)`), and the output grid `SXO`, `SYO`, `DXO`, `DYO` (double), `NXO`, `NYO`; written: `DLAT`, `DLON`, `NNIDX` (with its allocated extents), `XDIST`, `YDIST` |
| Variants | Without `W3_RTD`: the default template argument `nullptr`. With `W3_RTD`: a callable with the dummies of `W3LLTOEQ` (`w3servmd.F90:1415`) plus `PoLat`, `PoLon` (`LAMBDA_POLE` is `INOUT`); `W3LLTOEQ` itself is not part of this port |
| Structural changes | The `ALLOCATE(xdist(nx,ny), ydist(ny,nx))` of `:646` is the caller's (the arrays are passed in, with that shape kept in the index arithmetic). The `PRINT` progress lines (`:665`, `:668`, `:675`) are not reproduced; they carry no arithmetic |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3smcomd.F90` under `model/bin/switch_SMCMlt` (WW3's CMake
Release flags, gfortran 16.2), recompiled and checked identical to the CMake object of
`build_smc`, 2026-10-16. The `W3_RTD` variant: `w3gdatmd.F90` and `w3smcomd.F90` recompiled with
the same flags plus `-DW3_RTD` (no CMake build has `SMC` with `RTD`). The C++ unit compiled with
the openmp-release flags (g++ 16.2), warning-free, both instantiations.

- (v) `cfac = 2**(NRLv-1)` is `1 << n` for `0 <= n <= 31` and `0` otherwise in the object
  (`:640`); `cfac_of` does the same, so `NRLv < 1` gives `DLAT = SY/0.`, as in WW3.
- (v) `DLAT = SY/cfac`, `DLON = SX/cfac` (`:643-644`), both `divss` by the same converted `cfac`.
- (v) `mlon`, `mlat` (`:650-651`): `(X0 - SX*0.5) + (REAL(IJKCel(1)) + REAL(IJKCel(3))*0.5)*dlon`,
  same operations and grouping in both GIMPLEs. The Fortran vectorises this loop by 4 and 2; the
  operations are elementwise, so the bits are the same as the scalar C++ loop.
- (v) `olon`, `olat` (`:657-658`): `REAL(SXO + DBLE(I-1)*DXO)`, the product and sum in double,
  then one conversion to float, in both.
- (v) Longitude wrap (`:679-680`): `X0 - SX/2` (gfortran and g++ both emit `SX*0.5`, exact) and
  `SX*0.5 + (X0 + SX*REAL(NX-1))`, `lon ± 360.` in float, both.
- (v) The containment test (`:683-686`): `hx = (0.5*REAL(IJKCel(3)))*dlon`, compared as
  `mlon - hx <= lon`, `mlon + hx >= lon`, then the same in latitude; four ordered comparisons,
  a NaN fails each in both objects; first match wins (`EXIT`, `break`).
- (v) `XDIST = (((lon - mlon)*DERA)*RADIUS)*CLATS(ISEA)`, `YDIST = ((lat - mlat)*DERA)*RADIUS`
  (`:688-689`), left to right, `DERA*RADIUS` not folded, in both; `DERA = 0x1.1df46ap-6f`,
  `RADIUS = 6.3661975e+6f`, built in the header as `constants.F90:72-80` builds them, with
  `static_assert`s.
- (v) No FMA in either object (`vfmadd`/`vfmsub` count 0), no libm call, no reduction, no MIN/MAX,
  no real-to-integer conversion.
- (v) `W3_RTD`: the Fortran copies `olat`, `olon` to `tmplat`, `tmplon` and calls
  `w3lltoeq(tmplat, tmplon, olat, olon, ang, polat, polon, NXO*NYO)` (`:663-667`); the C++
  instantiation calls the callable with the same arguments in the same order, the rest unchanged.
- ⚠ CUDA not compiled (host function; it allocates).
- ⚠ Defects of WW3 kept as written:
  - `:646` allocates `XDIST(NX,NY)` and `YDIST(NY,NX)` (the SMC base grid), but `:688-689` write
    `XDIST(I,J)`, `YDIST(I,J)` for `I <= NXO`, `J <= NYO` (the output grid); `READ_SMCINT` and
    `W3S2XY_SMCNN_INT` (`:598`, `:789`) use `(NXO,NYO)`. Any `NXO`/`NYO` beyond those extents
    writes out of bounds. The header keeps the leading dimensions `NX` and `NY`; the caller must
    size the arrays for that index arithmetic.
  - `:673` sets `NNIDX(:,:) = -1` but nothing in `CALC_INTERP` allocates `NNIDX`; only
    `READ_SMCINT` does (`:598`), and after it the `ALLOCATE` of `XDIST` at `:646` stops with
    "already allocated". There is no order in which the routine can run to completion. The header
    takes `NNIDX` with its extents from the caller.
  - Under `W3_RTD`, `ang` (`:634`) is written by `W3LLTOEQ` and never read.
- (v) Fresh review 2026-10-16: no arithmetic defect. `w3smcomd.F90` recompiled from `build_smc`'s flags (object identical to the CMake one) and with `-DW3_RTD`; its `-O3` GIMPLE compared with the C++ unit's site by site: `cfac` shift and its `0.0` fallback, `DLAT`/`DLON` divisions, `mlon`/`mlat` grouping (vectorised elementwise in the Fortran), `olon`/`olat` in double then one conversion, the wrap bounds, the four ordered comparisons, `XDIST`/`YDIST` left to right with the same `.rodata` constants, `NNIDX` fill, the `W3LLTOEQ` argument order, no FMA in either object. Fixed two line citations in this report (`W3_RTD` block `:626-629`, `READ_SMCINT` `:579-605`).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing calls the routine, so no regtest can reach it.
