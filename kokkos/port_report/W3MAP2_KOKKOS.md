# W3MAP2_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12). Batch 4, unit U3.

The propagation maps of the `PR2` (third-order, `UQ` or `UNO`) scheme: `MAPX2` and `MAPY2` (the points of each
one-dimensional pass, in three ranges: both neighbours wet, boundary above, boundary below;
the `MAPBOU` of [`W3UNO2`](W3UNO2_KOKKOS.md)), `MAPAXY` (its `MAPACT`), `MAPXY` (the cells with
four wet corners, for the cross term of `W3XYP2`), their counters `NMX0..2`, `NMY0..2`, `NACT`,
`NMXY`, and the spectral maps `MAPTH2`, `MAPWN2`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3MAP2`, `model/src/w3pro2md.F90:135-492` at `WW3@96892c95` (h0ffmann/WW3), byte-identical to NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 HEAD -- model/src/w3pro2md.F90` is empty, v), as `mww3_test_01`'s `switch_PR2_UNO` compiles it |
| Kokkos code | [`../src/ww_kokkos/prop_map2.hpp`](../src/ww_kokkos/prop_map2.hpp): `ww::prop::w3map2<ExecSpace>`, run as one kernel iteration like [`W3MAP3`](W3MAP3_KOKKOS.md), then a fence and a copy of the eight counters to the host |
| C shim | none yet |
| Caller | `W3WAVE`, `model/src/w3wavemd.F90:1513` at the pin, when `FLMAP` is set and `GTYPE` is not `SMCTYPE` |

Not compiled and not ported: `W3_S` (`STRACE`), `W3_T` (the `MAPTXY`/`MAPTST` test maps and their
prints; `NDST` is not in the interface). Sections 1 to 3 and 5 are `W3MAP3`'s; section 4 is
`MAPXY` (`:390-404`) in place of `MAPCXY`, and there is no `GTYPE` test and no zeroing of a map.
`MAPTH2(1) .NE. 0` returns after the counters (`:408`), as in the Fortran.

## Arithmetic

None in floating point. The reference is the `-O3` object of `w3pro2md.F90` (WW3's CMake Release
flags, gfortran 16.2.0, `switch_PR2_UNO`), recompiled; its disassembly equals the CMake object's
(`objdump -d`, v, 2026-11-12).

- (v) Integer compares, adds and stores; the only SSE instructions of `W3MAP2` are integer ones
  (`movd`, `movdqa`, `paddd`, `pmuludq`, `pshufd` and similar) of the `MAPTH2`/`MAPWN2` fills; the
  C++ object has no floating-point instruction.
- (v) Every index is inside `MAPSTA(NY,NX)`: `IY` is `2..NY-1` or `1..NY-1`, rows `IY` and `IY+1`
  are read, `IX2 = 1+MOD(IX,NX)` is `1..NX`. (`W3MAP3` reads rows 0 and `NY+1`.)
- (v) Serial, in source order: the counters number the entries in loop order.
- (v) The C++ unit with the `openmp-release` flags and with `-O0 -g -fsanitize=address,undefined`
  compiles with no warning (Serial and OpenMP).
- (v) A throwaway differential program linked `libww3.a` of the CMake build, set up a grid through
  `W3DIMX`, `W3DIMS`, `W3DIMI`, `W3DIMA` and called `W3MAP2` on 400 random `MAPSTA` fields (3 to
  11 points a side, `NK` 3..7, `NTH` 4..9, most points `MAPSTA` 1); the C++ gave the same counters,
  `MAPX2`, `MAPY2`, `MAPAXY` and `MAPXY` up to their counters, and the whole of `MAPTH2` and
  `MAPWN2`, in all 400 (388 with every counter above zero). ⚠ Not kept (ADR-0010): no command in
  the repository reproduces it. ⚠ The early return for `MAPTH2(1) .NE. 0` was not exercised, and
  the Fortran leaves the entries of `MAPX2` .. `MAPXY` above the counters undefined, so they were
  not compared.
- ⚠ `MAPXY` is allocated `NSEA` long (`w3adatmd.F90:1426`) and `MAPX2`, `MAPY2`, `MAPAXY` `NY*NX`
  (`:1423-1425`); no counter can exceed `NX*(NY-2)` (one entry for each cell and range), but the C++ does not
  check the extents.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested: postponed to the validation phase (ADR-0016). Not wired: no shim and no caller hunk.

## WW3 defects found

None.
