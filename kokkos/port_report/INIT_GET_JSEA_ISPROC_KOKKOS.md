# INIT_GET_JSEA_ISPROC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Maps a global sea point `ISEA` to its local index `JSEA` and its owning rank `ISPROC`: the
round-robin split `JSEA = 1 + (ISEA-1)/NAPROC` of a structured or non-PDLIB run, or the PDLIB
domain decomposition's lookup tables when `LPDLIB` is true. Integer arithmetic only.

## What was ported

| | |
|---|---|
| WW3 routine | `INIT_GET_JSEA_ISPROC`, `model/src/w3parall.F90:1152-1252` at NOAA-EMC/WW3 `03adda91`. `w3parall.F90` is compiled by every switch; the `W3_PDLIB` branch by the 23 switch files with `PDLIB` (`git grep -lw PDLIB -- '*switch*'`: `ww3_tp2.6`, `ww3_tp2.17`, `ww3_tp2.19`, `ww3_tp2.21`, `ww3_ufs1.1/input_unstr`, `model/bin/switch_NWS_rwps`, among others). Called by `W3INIT` (`w3initmd.F90:5178`, `:5222`, `:5549`, `:5612`), `w3iorsmd.F90:751`, `w3iosfmd.F90:652`, `w3iotrmd.F90:781`, `w3updtmd.F90:1959`, `:1995`, `w3wavemd.F90:3758`, `wmgridmd.F90:3290`, `:3365`, `wminiomd.F90:365` |
| Kokkos code | [`../src/ww_kokkos/parall_init_get_jsea_isproc.hpp`](../src/ww_kokkos/parall_init_get_jsea_isproc.hpp), `ww::parall::init_get_jsea_isproc` |
| Variants | without `W3_PDLIB`, and `W3_PDLIB` with `LPDLIB` false or true, as the runtime argument `lpdlib` (false in a build without `W3_PDLIB`). `W3_S` is tracing only |
| Caller inputs | `NAPROC`, `IAPROC`, and the PDLIB tables `MAPSF(:,1)`, `ISEA_TO_JSEA` (filled in `w3profsmd_pdlib.F90:289-304`), `IPGL_TO_PROC` (`PDLIB/yowrankModule.F90:218-223`), all 0-based pointers. Their values come from MPI set-up and are not computed here |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` objects of `w3parall.F90` (WW3's CMake Release flags, gfortran 16.2) under
`switch_lab_shrd` (no `PDLIB`) and under `regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP`
(`DIST MPI PDLIB SCOTCH`), each recompiled and checked identical to the CMake object, against a
throwaway C++ unit with the openmp-release flags, 2026-10-16.

- (v) Round-robin branch: both objects and the C++ compute `lea -1`, `cltd; idiv NAPROC`,
  `lea +1` for `JSEA`, `imul` quotient by `NAPROC` and `sub` from `ISEA` for `ISPROC`; gfortran
  folds `(JSEA-1)*NAPROC` to `quotient*NAPROC` and g++ does the same.
- (v) PDLIB branch: `MAPSF(ISEA,1)` is loaded before the `IAPROC <= NAPROC` test, `JSEA` is
  `ISEA_TO_JSEA(ISEA)` or `-1`, `ISPROC = IPGL_TO_PROC(IP_glob)`; the same loads and compare in
  the C++ object.
- (v) No floating point, no libm call, no reduction, no MIN/MAX.
- ⚠ `ISEA_TO_JSEA` is allocated only in `w3profsmd_pdlib.F90:289`; with `LPDLIB` true and the
  table not yet built, the Fortran reads an unallocated array. The C++ reads whatever pointer the
  caller passes.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. `w3parall.F90` recompiled under `switch_lab_shrd`, the `SETUP` (`DIST MPI PDLIB`) switch and `SETUP` minus `W3_PDLIB` (all identical in instructions to the C++ unit for the matching flags; the first two identical to their CMake objects), and the PDLIB tables' lower bounds checked as 1 (`MAPSF(MSEA,3)` column 1, `ISEA_TO_JSEA(NSEA)`, `IPGL_TO_PROC(np_global)`). Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
