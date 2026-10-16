# VUF_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Returns the nodal corrections of one tidal constituent at one time: the astronomical argument
`V`, the nodal phase `u` and the nodal factor `f`, looked up by the constituent's name in the
tables `V_ARG`, `U_ARG`, `F_ARG` that `SETVUF` fills (`w3tidemd.F90:1893-2030`). The tidal
height is then `f*A*cos(V+u-g)` in the callers.

## What was ported

| | |
|---|---|
| WW3 routine | `VUF`, `model/src/w3tidemd.F90:1706-1746` at NOAA-EMC/WW3 `03adda91` (41 lines). Called by `flex_tidana_webpage` (`:2215`, `:2250`), `TIDE_PREDICT` (`:2540`, `:2549`) and `TIDE_PREDICT_ONLY` (`:2594`, `:2603`). No `#ifdef` in the routine; `w3tidemd.F90` is in every build. Regtest switches with `TIDE`: `ww3_tp2.18` (`switch_TIDE`, `switch_TIDE_MPI`); also `switch_Ifremer1`, `switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OAS*`, `switch_ite_pdlib` (`git grep -lw TIDE -- '*switch*'`); no `switch_NCEP_*`/`switch_NWS_*` |
| Kokkos code | [`../src/ww_kokkos/tide_vuf.hpp`](../src/ww_kokkos/tide_vuf.hpp), `ww::tide::vuf`, `KOKKOS_INLINE_FUNCTION`; the module state (`NTOTAL_CON`, `TIDECON_ALLNAMES`, `V_ARG`, `U_ARG`, `F_ARG` and their first extent) is passed in; names are 5-byte records |
| C shim | none yet. Where the Fortran writes `ERROR IN VUF: STOP.` and the name on `NDSET` and stops (`:1728-1731`), the C++ returns `kStop` and the shim does the `WRITE` and the `STOP`. The existing `tide_predict.hpp` and `tide_predict_only.hpp` take `VUF` as a callable; a lambda over `ww::tide::vuf` with the module state fits it |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3tidemd.F90` (WW3's CMake Release flags, gfortran
16.2) from `build_mixa` (`TIDE` on) and `build_lab` (`TIDE` off), each recompiled and found
identical to the CMake object; the GIMPLE of `vuf` is the same in both, so the routine has one
variant. Compared with a C++ unit built with the `openmp-release` flags (no warning), 2026-10-16.

- (v) The only floating-point operations are three `REAL` to `REAL(8)` conversions
  (`cvtss2sd` in both objects), which are exact. No libm call, no reduction, no `MIN`/`MAX`.
- (v) Same search: `K` from 1 to `NTOTAL_CON`, read once; the first match returns (`:1726`), as
  in the C++. The Fortran compares names with `__builtin_memcmp_eq(…, 5)` (inlined as a 4-byte and
  a 1-byte `cmp`); the C++ compares the 5 bytes one by one (five 1-byte `cmp`): the same equality,
  written without `std::memcmp` so the function can run in a kernel.
- (v) Same element: `(K, ITIME)` with the stride of the descriptor's second dimension; the C++
  takes one first extent for the three arrays, which are allocated with the same shape at both
  sites (`ww3_prnc.F90:1522`, `ww3_shel.F90:1966`, first extent 170).
- ⚠ `ITIME` is not bounds checked in the Fortran or the C++. `ww3_shel` allocates the tables with
  one time (`ww3_shel.F90:1966`, "to be removed later"), so any caller passing `I > 1` there would
  read past them. Whether a `ww3_shel` path does was not checked.
- ⚠ `NTOTAL_CON` is set in `VUF_SET_PARAMETERS` (`:1463`, 146 names) and in `OPNVUF` (`:1848`),
  which allocates `TIDECON_ALLNAMES(170)` (`:1771`); both the Fortran and the C++ trust `NTOTAL_CON` not to exceed
  the allocated names and the tables' first extent.
- CUDA: not compiled (optional this batch).
- (v) Fresh review 2026-10-16: the module recompiled from `build_mixa` and `build_lab` flags (identical
  to both CMake objects) and a new C++ unit read instruction by instruction: same 5-byte name test,
  same first match, same element `(K, ITIME)`, three exact `cvtss2sd`; no defect in the header. Fixed
  in this report the line where `VUF_SET_PARAMETERS` sets `NTOTAL_CON` (`:1463`, was `:1467`) and
  added `OPNVUF`'s `:1848`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
