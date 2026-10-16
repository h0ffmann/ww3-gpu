# PREPARATION_FD_SCHEME_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Builds, once at initialisation, the graph of the finite-difference wave-setup solve on a regular
grid: the neighbour table `GRIDS(IMOD)%NEIGH(NSEA,4)`, the edge list `GRIDS(IMOD)%EDGES(NBEDGE,2)`
with its count `NBEDGE`, and the CSR pattern `PDLIB_IA`, `PDLIB_JA`, `PDLIB_I_DIAG`, `PDLIB_NNZ` of
yowNodepool. Its only caller is `W3INIT` (`w3initmd.F90:753`), under switch `SETUP`, and with
`PDLIB` only when `GTYPE /= UNGTYPE` (`:749-756`).

## What was ported

| | |
|---|---|
| WW3 routine | `PREPARATION_FD_SCHEME`, `model/src/w3wavset.F90:1777-1932` at NOAA-EMC/WW3 `03adda91`; compiled by no regtest switch and no NOAA operational switch (`git grep -lw SETUP -- '*switch*'` finds only `model/src/cmake/switches.json`). Its only `#ifdef` block is `W3_S` tracing; no MPI call |
| Kokkos code | [`../src/ww_kokkos/wavset_preparation_fd_scheme.hpp`](../src/ww_kokkos/wavset_preparation_fd_scheme.hpp), host functions `ww::wavset::preparation_fd_scheme_count` (`:1853-1892`) and `ww::wavset::preparation_fd_scheme_fill` (`:1894-1931`) |
| Structural change | The six `ALLOCATE`s become caller-provided arrays. `NBEDGE` and `PDLIB_NNZ` are known only after the counting loop, so the routine is cut at the `ALLOCATE` of `EDGES` (`:1893`): `_count` is the size query and also writes `NEIGH` and the work array `STAT_SeaLand(NX,NY)`, which the caller keeps for `_fill`. `IMOD` selects `GRIDS(IMOD)`; the C++ takes that grid's arrays directly. Arrays are column-major, Fortran base 1, passed 0-based; the stored values stay 1-based sea indices |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3wavset.F90` (WW3's CMake Release flags, gfortran 16.2,
`regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP`, `ref3/build_setup`), recompiled and found
identical to the CMake object, against a throwaway C++ unit at the openmp-release flags, no
warning, 2026-10-16.

- (v) Integer-only routine: no floating point, libm, FMA, `MIN`/`MAX` or reduction. Both GIMPLE
  dumps unroll the four-neighbour loop with `NeighMat` folded to constants and address
  `STAT_SeaLand(IXN,JXN)` as `(IXN-1) + MAX(NX,0)*(JXN-1)` in 64 bits, with no bounds check; the
  C++ computes the same index. The counts `nbEdge` and `PDLIB_NNZ` are incremented in the same order.
- (v) The loads of `IY` at `:1874` and `:1897` are dead and the Fortran object drops them; the C++
  does not read them.
- ⚠ WW3 defect, reproduced: `JXN=IX+NeighMat(IN,2)` (`:1878`, `:1900`, `:1920`) uses `IX` where `IY`
  is meant, so the "neighbours" are `STAT_SeaLand(IX±1,IX)` and `STAT_SeaLand(IX,IX±1)`, not the
  four grid neighbours of `(IX,IY)`.
- ⚠ Undefined behaviour in WW3, reproduced: with no bounds check, `IXN` or `JXN` reaches 0 for a
  sea point at `IX=1` and `JXN` exceeds `NY` when `IX >= NY`, so `STAT_SeaLand` is read before its
  start or past its end. The value read is whatever memory holds; neither the Fortran nor the C++
  result is defined there, and no bit parity can be claimed for such a grid.
- ⚠ WW3 defect, reproduced: the CSR loop (`:1917-1930`) never sets `IX`, so every row of
  `PDLIB_JA` takes the neighbours of `IX = MAPSF(NSEA,1)` left by the previous loop. The C++ keeps
  `ix` from its edge loop for the same effect. `PDLIB_NNZ` counts each row's own neighbours
  (`:1886`), so `J` can exceed `PDLIB_NNZ` and write `PDLIB_JA` past its end, or stop short.
- (v) No loop is vectorised in either object; the Fortran's only vector code fills array descriptors at the `ALLOCATE`s (`:1853`, `:1893`, `:1912-1914`).
- ⚠ CUDA not compiled: the functions are host-only (`inline`), run once at initialisation.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest switch compiles `SETUP`, so no regtest can exercise it.
