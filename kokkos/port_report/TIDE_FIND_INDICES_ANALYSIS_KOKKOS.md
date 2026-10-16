# TIDE_FIND_INDICES_ANALYSIS_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Chooses the tidal constituents of a harmonic analysis among the 65 that `W3TIDEMD` knows, from a
list of names or from the presets `FAST` (44 constituents) and `VFAST` (20), and sets the module's
`TIDE_MF`, `TIDECON_NAME` and `TIDE_FREQC` (frequencies in cycles per hour) before building the
index tables with `TIDE_SET_INDICES`.

## What was ported

| | |
|---|---|
| WW3 routine | `TIDE_FIND_INDICES_ANALYSIS`, `model/src/w3tidemd.F90:343-488` at NOAA-EMC/WW3 `03adda91` (146 lines). Callers: `ww3_prnc` for tidal input (`ww3_prnc.F90:1424`, `ITYPE 6`) and `W3FLDTIDE2` on `READ` (`w3fldsmd.F90:873`, inside `#ifdef W3_TIDE` from `:844`), which `ww3_shel` (`ww3_shel.F90:1964-1965`, `W3_TIDE`) and `ww3_prtide` (`:356`) call. The routine has no `#ifdef` but `W3_S` (trace only). Regtest switches with `TIDE`: `ww3_tp2.18` (`switch_TIDE`, `switch_TIDE_MPI`); also `switch_Ifremer1`, `switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OAS*`, `switch_ite_pdlib` (`git grep -lw TIDE -- '*switch*'`); no `switch_NCEP_*`/`switch_NWS_*` |
| Kokkos code | [`../src/ww_kokkos/tide_tide_find_indices_analysis.hpp`](../src/ww_kokkos/tide_tide_find_indices_analysis.hpp), `ww::tide::tide_find_indices_analysis`, a host function template (CHARACTER work and an `ALLOCATE`, once per tidal setup). It reuses `fstr_eq`/`flen_trim` of `tide_tide_find_indices_prediction.hpp` and calls `ww::tide::tide_set_indices` (`:485`) |
| C shim | none yet. It gives `TIDE_FREQC` and `TIDECON_NAME` as arrays of 65 (the Fortran allocates `TIDE_MF` at `:479`, and `TIDE_MF` <= 65 on every defined path), `ALLOCATED(TIDECON_NAME)`, `IAPROC.EQ.NAPOUT`, the state `TIDE_SET_INDICES` reads, the work array `INDS(65)`, and two callables for the WRITEs to `NDSO` at `:463-465` (`(A,I4,2A,E12.2)`) and `:468-469` (`(A,I4,A,A)`); it raises the `ALLOCATE` error when the function returns 1 |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3tidemd.F90` (WW3's CMake Release flags, gfortran
16.2) from `build_mixa` (`TIDE` on) and `build_lab` (`TIDE` off), each recompiled and found
identical to the CMake object; the GIMPLE of this routine is the same in both (line tags
stripped). Compared with a C++ unit built with the `openmp-release` flags (no warning), 2026-10-16.

- (v) No floating-point arithmetic: `TIDE_FREQC(J) = TIDE_FREQC_ALL(INDS(J))` is a copy. The 260
  bytes of the C++ `freqc_all` (default-REAL literals as `float` literals, one decimal-to-binary32
  rounding each) were found byte for byte in the Fortran object (its `A.266`); the 65 names
  equal the 65 strings of `A.264` in the GIMPLE, and the `FAST`/`VFAST` tables equal `A.267`/`A.269`.
- (v) `TRIM` is dropped by gfortran: `:440` is `_gfortran_compare_string(100, LIST(1), 4, "FAST")`,
  then `:446` `(100, LIST(1), 5, "VFAST")`, in that order; `:459` is `compare_string(5,
  TIDECON_NAME_ALL(J), 100, LIST(TIDE_MF))`; `:454` is `_gfortran_string_len_trim(100, LIST(TIDE_MF+1))`.
  The C++ uses the blank-padded equality and `len_trim` of the sibling header.
- (v) Same control flow: `INDS` zeroed first (`:438`); `TIDE_MF` incremented before the search and
  read by the not-found WRITE; no `EXIT` in the 65-name search, so every match increments `NTIDES`;
  `TIDE_MF = NTIDES` at `:473`; the copy loop over `1..TIDE_MF` then `TIDE_SET_INDICES` as a tail
  call. The 65 names are unique, so each list entry matches at most once.
- ⚠ `:454` has no bound: with 70 non-blank names in `LIST(70)` the loop reads `LIST(71)` past the
  caller's array, here as in the Fortran.
- ⚠ `INDS` has 65 entries (`:406`): more than 65 matched names (possible only with repeated names
  in a list of 66 to 70) write `INDS(66..)` past the stack array in the Fortran and past `inds`
  here. Not checked whether an input file can give such a list.
- ⚠ `:478` deallocates `TIDE_FREQC` when allocated but not `TIDECON_NAME`, and nothing in
  `model/src` deallocates `TIDECON_NAME`: a second call stops with "Attempting to allocate already
  allocated variable 'tidecon_name'" (the object's `_gfortran_runtime_error_at` at `:479`).
  `ww3_shel` calls `W3FLDTIDE2` twice when both tidal water levels and tidal currents are read
  (`ww3_shel.F90:1964-1965`), so that configuration would stop there; read in the code, not run.
  `ww3_tp2.18` reads tidal currents only (`ww3_shel.nml:18`). The C++ returns 1 at that point,
  after `TIDE_MF` and the WRITEs, before the copy.
- `W3_S`: the `STRACE` call names `'TIDE_FIND_INDICES_PREDICTION'` (`:412`), a trace label only;
  not translated.
- CUDA: not compiled; the function is host only (`std::memcpy`, callables that write).
- (v) Fresh review 2026-10-16: no arithmetic or control-flow defect. `w3tidemd.F90` recompiled from
  `build_mixa` and `build_lab` (identical to the CMake objects); the routine's GIMPLE read against the
  header; the 260 bytes of `A.266` extracted from the object equal the C++ `freqc_all` section, `A.267`
  and `A.269` decode to the two preset tables, and the 325 name bytes equal the source's literals.
  Fixed: the header cited the name table at `:422-429` and the frequencies at `:431-447`; they are at
  `:415-422` and `:424-436`. The C++ unit was compiled again, warning-free.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
