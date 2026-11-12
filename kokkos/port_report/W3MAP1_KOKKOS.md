# W3MAP1_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12). Batch 4, unit U3.

The propagation maps of the `PR1` (first-order upwind) scheme: `FACVX` and `FACVY`, the reciprocal
of the number of wet points among the two that bound a cell face in X and in Y (0 when none, with
the closure of a simple or tripole grid), and the spectral index maps `IS0` and `IS2` (previous
and next direction bin, cyclic in direction).

## What was ported

| | |
|---|---|
| WW3 routine | `W3MAP1`, `model/src/w3pro1md.F90:107-286` at `WW3@96892c95` (h0ffmann/WW3), byte-identical to NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 HEAD -- model/src/w3pro1md.F90` is empty, v), as `ww3_ts3`'s `switch_ST4_PR1_FLD1` compiles it |
| Kokkos code | [`../src/ww_kokkos/prop_map1.hpp`](../src/ww_kokkos/prop_map1.hpp): `ww::prop::w3map1<ExecSpace>`, run as one kernel iteration like [`W3MAP3`](W3MAP3_KOKKOS.md), then a fence |
| C shim | none yet |
| Caller | `W3WAVE`, `model/src/w3wavemd.F90:1510` at the pin, when `FLMAP` is set and `GTYPE` is not `SMCTYPE` |

Not compiled and not ported: `W3_S` (`STRACE`). `MAPSTA` is the caller's `MAPSTA(NY*NX)`, the
column-major storage of `MAPSTA(NY,NX)`. `ICLOSE_NONE`, `ICLOSE_SMPL`, `ICLOSE_TRPL` are -1, 2
and 8 (`w3gsrumd.F90:314-319`).

## Arithmetic

The reference is the `-O3` object of `w3pro1md.F90` (WW3's CMake Release flags, gfortran 16.2.0,
`-ffp-contract=off`, `switch_ST4_PR1_FLD1`), recompiled with `-fdump-tree-optimized-lineno`; its
disassembly equals the CMake object's (`objdump -d`, v, 2026-11-12).

- (v) Two floating-point operations, both exact: `FACV? + 1.` (`addss`; at most four times for a
  cell, a small integer in `REAL`) and `1./FACV?` (`divss`, only where `FACV? .NE. 0.`, tested with
  `ucomiss`, the C++ `!=`). Neither loop is vectorised in `W3MAP1`. The C++ object has the same
  counts of `addss` (14), `divss` (2) and `ucomiss` (2).
- (v) The order of the additions is not a rounding order: every counter is an exact small integer.
- (v) Serial, in source order, as `W3MAP3`; it runs only when `FLMAP` is set.
- (v) The C++ unit with the `openmp-release` flags and with `-O0 -g -fsanitize=address,undefined`
  compiles with no warning (Serial and OpenMP); `vfmadd`: 0.
- (v) A throwaway differential program linked `libww3.a` of the CMake build, set up a grid through
  `W3DIMX`, `W3DIMS`, `W3DIMI`, `W3DIMA` and called `W3MAP1` on 400 random `MAPSTA` fields
  (3 to 11 points a side, `MAPSTA` in -1..3, `ICLOSE` -1, 2 and 8); the C++ gave the same bits in
  `FACVX`, `FACVY` (compared as hexadecimal), `IS0` and `IS2` in all 400. ⚠ Not kept (ADR-0010): no
  command in the repository reproduces it.
- ⚠ `FACVX`/`FACVY` are `POINTER` arrays (`w3adatmd.F90:510`, `:639`, allocated `NY*NX` at `:1417-1418`);
  the object reads their stride from the descriptor, which is 1 for an array allocated there. The
  C++ takes a contiguous View, and zeroes its whole extent for `FACVX = 0.`: the same as the Fortran
  only when the caller sizes the View `NY*NX`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested: postponed to the validation phase (ADR-0016). Not wired: no shim and no caller hunk.

## WW3 defects found

None.
