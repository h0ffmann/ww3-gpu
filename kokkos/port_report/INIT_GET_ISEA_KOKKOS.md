# INIT_GET_ISEA_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Maps a local sea index `JSEA` to its global sea point `ISEA`: the identity under `W3_SHRD`, the
round-robin split `ISEA = IAPROC + (JSEA-1)*NAPROC` under `W3_DIST`, or the PDLIB node map
`IPLG(JSEA)` when `LPDLIB` is true on an unstructured grid. Integer arithmetic only. The inverse
map is [`INIT_GET_JSEA_ISPROC`](INIT_GET_JSEA_ISPROC_KOKKOS.md).

## What was ported

| | |
|---|---|
| WW3 routine | `INIT_GET_ISEA`, `model/src/w3parall.F90:1396-1504` at NOAA-EMC/WW3 `03adda91`. `w3parall.F90` is compiled by every switch: `SHRD` by 464 switch files, `DIST` by 298, the `W3_PDLIB` branch by the 23 with `PDLIB` (`git grep -lw <SWITCH> -- '*switch*'`; e.g. `ww3_tp2.6`, `ww3_tp2.17`, `ww3_ufs1.1/input_unstr`, `model/bin/switch_NWS_rwps`). Called from `w3wavemd.F90` (`:1577`, `:1701`, `:1803`, `:2128`, `:2247`, `:3228`, `:3528`), `w3iogomd.F90` (`:1529` to `:4470`, 11 calls), `w3iorsmd.F90:681`, `:844`, `:862`, `w3iosfmd.F90:248`, `w3initmd.F90:1335`, `w3updtmd.F90:1192`, `:1246`, `w3snl5md.F90:371`, `w3oacpmd.F90:452`, `wminiomd.F90:1620`, `:2262`, `pdlib_field_vec.F90:715`, `:881`, and the PDLIB solvers of `w3profsmd_pdlib.F90` and `w3wavset.F90` |
| Kokkos code | [`../src/ww_kokkos/parall_init_get_isea.hpp`](../src/ww_kokkos/parall_init_get_isea.hpp), `ww::parall::init_get_isea` |
| Variants | `W3_SHRD` (`dist` false); `W3_DIST` without `W3_PDLIB` (`dist` true, `pdlib` false); `W3_DIST` with `W3_PDLIB` (`pdlib` true), with `LPDLIB` false, or true and `GTYPE` structured or unstructured, as runtime arguments. `W3_S` is tracing only. `W3_PDLIB` without `W3_DIST` is not a build WW3 makes (PDLIB needs MPI) and is not translated |
| Caller inputs | `IAPROC`, `NAPROC` (W3ODATMD), `LPDLIB` (CONSTANTS), `GTYPE` (W3GDATMD), `IPLG(1:NPA)` (yowNodepool, final allocation in `PDLIB/yowpdlibmain.F90:998-1012`), a 0-based pointer. Their values come from the MPI and PDLIB set-up and are not computed here |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` objects of `w3parall.F90` (WW3's CMake Release flags, gfortran 16.2) under
`switch_lab_shrd` (`SHRD`) and under `regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP`
(`DIST MPI PDLIB SCOTCH`), each recompiled and checked identical to the CMake object, against a
throwaway C++ unit with the openmp-release flags (warning-free), 2026-10-16.

- (v) `SHRD`: both objects are one load of `JSEA` and one store to `ISEA`.
- (v) `DIST`+`PDLIB`: both objects test `LPDLIB`, then `GTYPE == 3`; the round-robin path is
  `sub 1`, `imul NAPROC`, `add IAPROC`; the unstructured path loads `IPLG(JSEA)` (descriptor
  offset plus `JSEA` in the Fortran, `iplg[jsea-1]` in the C++, the same element). Same branch
  structure and the same integer operations in the C++ object. Integer `add` is exact, so the
  operand order cannot change a result.
- (v) `DIST` without `PDLIB`: no CMake build of such a switch file was made; `w3parall.F90` was
  compiled with the `SETUP` build's flags minus `W3_PDLIB` and `W3_SCOTCH`, and its object (test
  `LPDLIB`, round robin when false, no store when true) has the same instructions as the C++ unit
  with `dist` true and `pdlib` false.
- ⚠ WW3 defect: under `W3_DIST` without `W3_PDLIB`, `LPDLIB` true leaves the `INTENT(OUT)`
  `ISEA` unassigned (`w3parall.F90:1486-1499`). `LPDLIB` can be set true in such a build by
  `w3gridmd.F90:3736` (implicit fluctuation-splitting schemes), though `w3initmd.F90:642-645`
  resets it to false for the model run. The C++ leaves `isea` as passed.
- ⚠ `IPLG` is read with no bounds check in the Fortran and the C++; a `JSEA` beyond `NPA` reads
  past the array in both.
- (v) No floating point, no libm call, no reduction, no MIN/MAX.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no bit defect. The `SHRD`, `DIST`+`PDLIB` and `DIST`-only objects of `w3parall.F90` compared instruction by instruction with a fresh C++ unit; the same. Fixed in the header and here: `IPLG` is `IPLG(1:NPA)` from `PDLIB/yowpdlibmain.F90:998-1012`, not `IPLG(1:NP)` of `:327`; the `DIST`-only ⚠ became (v) on the compiled object.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
