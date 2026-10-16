# GET_JSEA_IBELONG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Maps a global sea point `ISEA` to its local index `JSEA` and a flag `IBELONG` (1 when this rank
holds the point): the round-robin split `JSEA = 1 + (ISEA-1)/NAPROC` when `LPDLIB` is false or the
grid is structured, or the PDLIB tables `MAPSF(:,1)`, `IPGL_NPA` and `JX_TO_JSEA` on an
unstructured grid (`JSEA = -1` for a point outside this rank's augmented domain). Integer
arithmetic only. Its siblings are [`INIT_GET_JSEA_ISPROC`](INIT_GET_JSEA_ISPROC_KOKKOS.md) and
[`INIT_GET_ISEA`](INIT_GET_ISEA_KOKKOS.md).

## What was ported

| | |
|---|---|
| WW3 routine | `GET_JSEA_IBELONG`, `model/src/w3parall.F90:1265-1384` at NOAA-EMC/WW3 `03adda91`. `w3parall.F90` is compiled by every switch; the `W3_PDLIB` branch by the 23 switch files with `PDLIB` (`git grep -lw PDLIB -- '*switch*'`: `ww3_tp2.6`, `ww3_tp2.17`, `ww3_ufs1.1/input_unstr`, `model/bin/switch_NWS_rwps`, among others). Called only by `W3ULEV` (`w3updtmd.F90:2393`, `:2417`, `:2454`), on dry and re-wetted points; `pdlib_field_vec.F90:449`, `:661` and `w3updtmd.F90:1143` import it without calling it |
| Kokkos code | [`../src/ww_kokkos/parall_get_jsea_ibelong.hpp`](../src/ww_kokkos/parall_get_jsea_ibelong.hpp), `ww::parall::get_jsea_ibelong` |
| Variants | without `W3_PDLIB` (`pdlib` false) and with it (`pdlib` true), with `LPDLIB` false, or true and `GTYPE` structured or unstructured, and `IAPROC` above `NAPROC` or not, as runtime arguments. The routine has no `W3_DIST`/`W3_SHRD` branch: under `SHRD`, `NAPROC` and `IAPROC` are 1. `W3_S` is tracing only |
| Caller inputs | `IAPROC`, `NAPROC` (W3ODATMD), `LPDLIB` (CONSTANTS), `GTYPE` (W3GDATMD), and under PDLIB the first column of `MAPSF(1:MSEA,3)` (W3GDATMD), `IPGL_NPA(1:NP_GLOBAL)` (`PDLIB/yowrankModule.F90:227-233`) and `JX_TO_JSEA(1:NPA)` (`w3profsmd_pdlib.F90:289-305`), all 0-based pointers. Their values come from the MPI and PDLIB set-up and are not computed here |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` objects of `w3parall.F90` (WW3's CMake Release flags, gfortran 16.2) under
`switch_lab_shrd` (no `PDLIB`) and under `regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP`
(`DIST MPI PDLIB SCOTCH`), each recompiled and checked identical to the CMake object, against a
throwaway C++ unit with the openmp-release flags (warning-free), 2026-10-16.

- (v) Round-robin branch, both builds: `lea -1(ISEA)`, `cltd; idiv NAPROC`, `lea +1` for
  `JSEA`, `imul` quotient by `NAPROC`, `sub` from `ISEA`, `cmp IAPROC; sete` for `IBELONG`; the
  C++ object has the same instructions in the same order. Integer division truncates toward zero
  in both languages.
- (v) Without `PDLIB` (lab object): `LPDLIB` true jumps straight to the return with no store; the
  C++ with `pdlib` false does the same.
- (v) `PDLIB` (setup object): `IAPROC`, `NAPROC` loaded first, then `LPDLIB` tested, then
  `GTYPE == 3`, then `IAPROC <= NAPROC`; the unstructured path loads `MAPSF(ISEA,1)` through its
  descriptor, `IPGL_NPA(IX)`, tests it against 0 and stores `IBELONG = 0, JSEA = -1` or
  `IBELONG = 1, JSEA = JX_TO_JSEA(JX)`. The C++ object has the same branch structure, the same
  stores and reads the same elements through 0-based pointers.
- ⚠ WW3 defect: without `W3_PDLIB`, `LPDLIB` true leaves the `INTENT(OUT)` `JSEA` and `IBELONG`
  unassigned (`w3parall.F90:1353-1380`); `W3ULEV` then reads `IBELONG` (`w3updtmd.F90:2394`).
  `LPDLIB` is reset to false for the model run by `w3initmd.F90:642-645`. The C++ leaves both as
  passed.
- ⚠ `MAPSF`, `IPGL_NPA` and `JX_TO_JSEA` are read with no bounds check in the Fortran and the
  C++. `IX = MAPSF(ISEA,1)` is a sea node, so a non-zero `IPGL_NPA(IX)` meets a non-zero
  `JX_TO_JSEA` (`w3profsmd_pdlib.F90:296-305`) as long as the caller passes a valid `ISEA`.
- ⚠ `NAPROC` 0 traps (`idiv`) in both.
- (v) No floating point, no libm call, no reduction, no MIN/MAX.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. The `w3parall.F90` objects of `switch_lab_shrd` and of
  `switch_PDLIB` plus `SETUP` were rebuilt, identical to the CMake objects, and their
  `GET_JSEA_IBELONG` read instruction by instruction against the C++ unit (round-robin `idiv`,
  `imul`, `sub`, `sete`; the `LPDLIB`, `GTYPE == 3`, `IAPROC <= NAPROC` branches; the
  `MAPSF`/`IPGL_NPA`/`JX_TO_JSEA` loads and the stores); the only `#ifdef`s are `W3_S` and
  `W3_PDLIB`, both covered; callers and line cites checked at `03adda91`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
