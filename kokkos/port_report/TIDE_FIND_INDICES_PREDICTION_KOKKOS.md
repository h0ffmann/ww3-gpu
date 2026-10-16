# TIDE_FIND_INDICES_PREDICTION_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Finds, for each tidal constituent named in the prediction request of `ww3_prtide`, its position in
the analysed constituents `TIDECON_NAME`, and counts the names (`TIDE_PRMF`). A first name `FAST`
or `VFAST` selects every analysed constituent in order.

## What was ported

| | |
|---|---|
| WW3 routine | `TIDE_FIND_INDICES_PREDICTION`, `model/src/w3tidemd.F90:244-339` at NOAA-EMC/WW3 `03adda91` (96 lines). Only caller: `ww3_prtide` (`ww3_prtide.F90:436`), with `TIDECON_PRNAMES(70)` filled by `STRSPLIT` (`:316-317`). `ww3_prtide` is built only when the switch has `TIDE` (`model/src/CMakeLists.txt:199-201`); the routine itself has no `#ifdef` but `W3_S` (trace only). Regtest switches with `TIDE`: `ww3_tp2.18` (`switch_TIDE`, `switch_TIDE_MPI`); also `switch_Ifremer1`, `switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OAS*`, `switch_ite_pdlib` (`git grep -lw TIDE -- '*switch*'`); no `switch_NCEP_*`/`switch_NWS_*` |
| Kokkos code | [`../src/ww_kokkos/tide_tide_find_indices_prediction.hpp`](../src/ww_kokkos/tide_tide_find_indices_prediction.hpp), `ww::tide::tide_find_indices_prediction`, a host function template (CHARACTER work, once per `ww3_prtide` run); `LIST` as 100-byte records, `TIDECON_NAME` as 5-byte records, indices written 1-based |
| C shim | none yet; it passes `TIDE_MF`, `TIDECON_NAME`, `IAPROC.EQ.NAPOUT`, and two callables that do the WRITEs to `NDSO` at `:331-332` (`(A,A,E12.2)` with `TIDE_FREQC(J)`) and `:335-336` |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3tidemd.F90` (WW3's CMake Release flags, gfortran
16.2) from `build_mixa` (`TIDE` on) and `build_lab` (`TIDE` off), each recompiled and found
identical to the CMake object; the GIMPLE of this routine is the same in both (line tags
stripped). Compared with a C++ unit built with the `openmp-release` flags (no warning), 2026-10-16.

- (v) No floating point: the routine only compares strings and stores integers; `TIDE_FREQC(J)` is
  read only for the WRITE, which the shim does.
- (v) `TRIM` is dropped by gfortran: `:316` is `_gfortran_compare_string(100, LIST(1), 5, "VFAST")`
  then `(100, LIST(1), 4, "FAST")`, and `:328` is `compare_string(5, TIDECON_NAME(J), 100,
  LIST(TIDE_PRMF))`, blank-padded comparisons. The C++ `fstr_eq` is that comparison's equality
  (common prefix equal, the longer operand's rest all `' '`). `:324` is `_gfortran_string_len_trim`
  over 100 bytes; `flen_trim` counts only `' '` as trailing, as libgfortran does.
- (v) Same control flow: `TIDE_MF` read once per loop, no `EXIT` in the search so the last match
  wins (`:327-333`), `FOUND` reset per name, the not-found test `FOUND.EQ.0` before `IAPROC.EQ.NAPOUT`.
  Both objects vectorise the `INDS(J)=J` fill (`:317-319`); integer results are the same.
- ⚠ `:324` has no bound: with 70 non-blank names in `TIDECON_PRNAMES(70)` the loop reads
  `LIST(71)` (`len_trim` of 100 bytes past the caller's array) and, if non-blank, writes `INDS(71)`.
  The C++ reproduces the unbounded scan; the caller's arrays have the same 70 entries. `STRSPLIT`
  (`w3servmd.F90:1882-1938`) has no bound either and overflows first with more than 70 names.
- ⚠ `TIDE_MF > 70` in the `FAST`/`VFAST` branch would write past `INDS(70)` (`:317-319`), in the
  Fortran and the C++ alike. Not checked whether an input can set it.
- CUDA: not compiled; the function is host only (`std::memcmp`, callables that write).
- (v) Fresh review 2026-10-16: the module recompiled from `build_mixa` and `build_lab` flags (identical
  to both CMake objects); its GIMPLE read call by call against the header: `TIDE_PRMF=0` stored
  before the `VFAST`/`FAST` tests, `compare_string(100, LIST(1), 5|4, ...)`,
  `string_len_trim(100, LIST(TIDE_PRMF+1))`, `compare_string(5, TIDECON_NAME(J), 100, LIST(TIDE_PRMF))`
  with no `EXIT`, `TIDE_PRMF` stored before each WRITE as the C++ reference does, and a new C++ unit
  compiled warning-free with the two callables at the two WRITE sites; no defect, nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
