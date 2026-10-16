# TIDE_SET_INDICES_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Builds the integer index tables of the tidal package: for each analysed constituent its position
in the full list of constituents (`TIDE_INDEX2`), and for each shallow-water constituent where its
list of main constituents starts (`TIDE_INDEXJ`) and which earlier constituent each of them is
(`TIDE_INDEXJK`). `SETVUF_FAST` reads them to combine the nodal factors `f`, `u` and the
astronomical argument `V` (`w3tidemd.F90:604-662`).

## What was ported

| | |
|---|---|
| WW3 routine | `TIDE_SET_INDICES`, `model/src/w3tidemd.F90:494-527` at NOAA-EMC/WW3 `03adda91` (34 lines). Called by `TIDE_FIND_INDICES_ANALYSIS` (`:485`), which `ww3_prnc` calls for tidal input (`ww3_prnc.F90:1424`, `ITYPE 6`), and by `W3FLDTIDE2` (`w3fldsmd.F90:881`), which `ww3_shel` (`ww3_shel.F90:1964-1965`, under `W3_TIDE` from `:1963`), `ww3_prnc` (`:1810`) and `ww3_prtide` (`:356`) call. No `#ifdef` in the routine; `w3tidemd.F90` is in every build (`cmake/src_list.cmake:58`). Regtest switches with `TIDE`: `ww3_tp2.18` (`switch_TIDE`, `switch_TIDE_MPI`, `matrix.base:727`, `:818`); also `switch_Ifremer1`, `switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OAS*`, `switch_ite_pdlib` (`git grep -lw TIDE -- '*switch*'`); no `switch_NCEP_*`/`switch_NWS_*` |
| Kokkos code | [`../src/ww_kokkos/tide_tide_set_indices.hpp`](../src/ww_kokkos/tide_tide_set_indices.hpp), `ww::tide::tide_set_indices`, a host function (it builds a table once per tidal setup); the module state is passed in, names as consecutive 5-byte records, every index written stays 1-based |
| C shim | none yet; it passes the module arrays of `W3TIDEMD` and the allocated extents of `TIDE_INDEXJ` (`NTOTAL_CON`) and `TIDE_INDEXJK` (`NKONCO`), `:1467` |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3tidemd.F90` (WW3's CMake Release flags, gfortran
16.2) from `build_mixa` (`TIDE` on) and `build_lab` (`TIDE` off), each recompiled and found
identical to the CMake object; the GIMPLE of `tide_set_indices` is the same in both, so the routine
has one variant. Compared with a C++ unit built with the `openmp-release` flags (no warning),
2026-10-16.

- (v) No floating point: integer loops, stores and `CHARACTER*5` comparisons only. Both objects
  compare names with `__builtin_memcmp_eq(…, 5)`, inlined as a 4-byte and a 1-byte `cmp`; both
  clear `TIDE_INDEXJ` and `TIDE_INDEXJK` with one `memset` each over the whole allocated extent
  (`:506-507`).
- (v) Same loop bounds and order; no `EXIT` in either search, so the last matching name wins
  (`:502`, `:519-521`), as in the C++. `L2=0` at `:517` is a dead store gfortran removes.
- ⚠ An analysed name that matches no entry of `TIDECON_ALLNAMES` leaves `TIDE_INDEX2(L)` at its
  previous value (0 on the first call, the array is in static storage), and `SETVUF_FAST` then
  reads `FA(0)` (`:660`). The C++ reproduces it: it does not write the entry. Whether any input
  reaches it was not checked; `TIDE_FIND_INDICES_ANALYSIS` takes names from `TIDECON_NAME_ALL`.
- ⚠ `TIDE_INDEX2` has `MC = 70` entries (`:93`, `:134`); `TIDE_MF > 70` would write past it, in
  the Fortran and the C++ alike. Not checked whether an input can set it.
- CUDA: not compiled; the function is host only (`std::memcmp`).
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3tidemd.F90` from `build_mixa` and
  `build_lab` flags (both identical to the CMake object, same preprocessed routine and GIMPLE), and
  compared it with the C++ unit's GIMPLE and assembly: same loop bounds read once, same `memcmp_eq`
  of 5 bytes (inlined as a 4-byte and a 1-byte `cmp` in both), same two `memset` over the allocated
  extents, same last-match-wins stores. Fixed one cite: `ww3_shel` calls `W3FLDTIDE2` at `:1964-1965`,
  not `:1924-1928`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
