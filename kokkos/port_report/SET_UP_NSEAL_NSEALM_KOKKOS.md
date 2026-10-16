# SET_UP_NSEAL_NSEALM_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Sets the number of sea points this rank holds, `NSEAL`, and its maximum over ranks, `NSEALM`:
`NSEA` for both in a shared-memory build, the round-robin split
`NSEAL = 1 + (NSEA-IAPROC)/NAPROC`, `NSEALM = 1 + (NSEA-1)/NAPROC` in a distributed build, or the
PDLIB decomposition's counts on an unstructured grid. Integer arithmetic only.

## What was ported

| | |
|---|---|
| WW3 routine | `SET_UP_NSEAL_NSEALM`, `model/src/w3parall.F90:1030-1139` at NOAA-EMC/WW3 `03adda91`. `w3parall.F90` is compiled by every switch: `W3_SHRD` by 464 switch files, `W3_DIST` by 298, `W3_PDLIB` by 23 (`git grep -lw <SWITCH> -- '*switch*'`; PDLIB in `ww3_tp2.6`, `ww3_tp2.17`, `model/bin/switch_NWS_rwps`, `switch_USACE_1`, among others). Called by `W3INIT` (`w3initmd.F90:803`) and `W3DIMW` (`w3wdatmd.F90:487`) |
| Kokkos code | [`../src/ww_kokkos/parall_set_up_nseal_nsealm.hpp`](../src/ww_kokkos/parall_set_up_nseal_nsealm.hpp), `ww::parall::set_up_nseal_nsealm` |
| Variants | `W3_SHRD` (`dist` false), `W3_DIST` without `W3_PDLIB`, and `W3_DIST` with `W3_PDLIB` (`pdlib` true), each with `LPDLIB` false or true and `GTYPE` equal to `UNGTYPE` or not, as runtime arguments. `W3_S` is tracing only. `PDLIB` requires `MPI`, which excludes `SHRD` (`model/src/cmake/switches.json`), so `pdlib` without `dist` is not a build |
| Caller inputs | `NSEA`, `LPDLIB`, `GTYPE`, `IAPROC`, `NAPROC`, and `PDLIB_NSEAL`, `PDLIB_NSEALM` (counted in `w3profsmd_pdlib.F90:248-279`). Their values come from MPI set-up and are not computed here |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` objects of `w3parall.F90` (WW3's CMake Release flags, gfortran 16.2) under
`switch_lab_shrd` and under `regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP`
(`DIST MPI PDLIB SCOTCH`), each recompiled and checked identical to the CMake object, against a
throwaway C++ unit with the openmp-release flags (g++ 16.2, no warning), 2026-10-16.

- (v) `W3_SHRD`: the Fortran object and the C++ are the same instructions: load `NSEA`, store it
  to both outputs.
- (v) `W3_DIST` with `W3_PDLIB`: both test `LPDLIB`, then `GTYPE == 3`; the PDLIB branch stores
  `PDLIB_NSEAL` then `PDLIB_NSEALM`; the round-robin branch compares `IAPROC <= NAPROC`, computes
  `NSEA-IAPROC`, `cltd; idiv NAPROC`, `+1` or `0`, stores `NSEALout`, then `NSEA-1`, `idiv`, `+1`
  for `NSEALMout`. g++ loads `NSEA`, `IAPROC`, `NAPROC` before the `LPDLIB` test where gfortran
  loads them after; the values and the stores are the same.
- (v) `W3_DIST` without `W3_PDLIB`: no CMake build of such a switch file was made;
  `w3parall.F90` was compiled with the `SETUP` build's flags minus `W3_PDLIB` and `W3_SCOTCH`, and
  its object (test `LPDLIB`, round robin when false, no store when true) has the same
  instructions as the C++ unit with `dist` true and `pdlib` false.
- (v) No floating point, no libm call, no reduction, no MIN/MAX.
- ⚠ With `W3_DIST`, no `W3_PDLIB` and `LPDLIB` true (`w3parall.F90:1110-1134`), the Fortran
  assigns neither `INTENT(OUT)` value; the C++ leaves them as passed. `W3INIT` resets `LPDLIB` to
  false in such a build before its call (`w3initmd.F90:642`), while `ww3_grid` can set it true
  without PDLIB (`w3gridmd.F90:3736`).
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no bit defect. The `SHRD`, `DIST`+`PDLIB` and `DIST`-only objects of `w3parall.F90` compared instruction by instruction with a fresh C++ unit; the same. Fixed here: 23 switch files carry `PDLIB`, not 22; the `DIST`-only ⚠ became (v) on the compiled object.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
