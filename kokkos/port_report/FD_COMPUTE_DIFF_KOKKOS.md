# FD_COMPUTE_DIFF_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Length of one edge of the finite-difference wave-setup graph and its unit vector, signed by which
of the edge's two sea points asks (switch `SETUP`, regular and curvilinear grids). Its only caller
is `FD_WAVE_SETUP_COMPUTE_SYSTEM` (`w3wavset.F90:2630`, `:2636`), reached from
`WAVE_SETUP_COMPUTATION` (`:3155-3160`) when `GTYPE` is not `UNGTYPE`, itself called from `W3WAVE`
(`w3wavemd.F90:1124`).

## What was ported

| | |
|---|---|
| WW3 routine | `FD_COMPUTE_DIFF`, `model/src/w3wavset.F90:2433-2525` at NOAA-EMC/WW3 `03adda91`; compiled by no regtest switch and no NOAA operational switch (`git grep -lw SETUP -- '*switch*'` finds only `model/src/cmake/switches.json`). Its only `#ifdef` block is `W3_S` tracing; no MPI or PDLIB path |
| Kokkos code | [`../src/ww_kokkos/wavset_fd_compute_diff.hpp`](../src/ww_kokkos/wavset_fd_compute_diff.hpp), `ww::wavset::fd_compute_diff`; `IEDGE`, `ISEA` 1-based; `EDGES`, `MAPSF`, `XGRD`, `YGRD` passed 0-based, column-major, with their leading dimensions |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3wavset.F90` (WW3's CMake Release flags, gfortran 16.2,
`regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP`), recompiled and found identical to the CMake
object, against a throwaway C++ unit at the openmp-release flags, 2026-10-16.

- (v) Same operation sequence in both objects: two `subsd` in `REAL(8)`, `cvtsd2ss` to `REAL`, two
  `mulss`, `addss`, `sqrtss` (inline, no libm call), two `divss`, then `xorps` sign flips on the
  `ISEA /= ISEA1` branch. The Fortran `-deltaX/dist` is `-(deltaX/dist)`: the object negates the
  quotient, and so does the C++ (the two differ only in the sign of the NaN from `0/0`).
- (v) No FMA, no reduction, no `MIN`/`MAX`, no libmvec call.
- ⚠ `XGRD(IX1,IY1)` (`:2515-2516`) subscripts an array allocated `(MY,MX)` (`w3gdatmd.F90:1727`)
  with `IX` first. The C++ reproduces the address `(IX-1) + (IY-1)*NY` as written. On a grid with
  `NX > NY` this reads past the column, and in general it reads another point's coordinates.
- ⚠ CUDA not compiled this batch.
- (v) Fresh review 2026-10-16: no defect. The module recompiled from `ref3/build_setup` flags gives a disassembly identical to the CMake object; its GIMPLE and assembly were compared with a new C++ unit (openmp-release flags, no warning): the same `subsd`, `cvtsd2ss`, `mulss`, `addss`, `sqrtss`, `divss`, `xorps` sequence with the same operands, and the `MAPSF(MSEA,3)` (`MSEA = NSEA` at every `W3DIMX` call), `EDGES(NBEDGE,2)` and `XGRD(MY,MX)` layouts as the header documents. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest switch compiles `SETUP`, so no regtest can exercise it.
