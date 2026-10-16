# PTSORT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The ordering step of `W3PART`'s watershed partitioning: a counting sort of the digitised,
inverted spectrum `IMI` (levels `1..IHMAX`, `w3partmd.F90:349`) into `IND`, the spectral indices
by ascending level, which `PT_FLD` floods in that order. `W3PART` calls it once per spectrum
under `PTMETH` 1, 2 and 3 (`w3partmd.F90:353`); `PTMETH` 4 and 5 return before it
(`:281-308`, `:313-338`).

## What was ported

| | |
|---|---|
| WW3 routine | `PTSORT`, `model/src/w3partmd.F90:511-613` at NOAA-EMC/WW3 `03adda91`; its only `#ifdef` is `W3_S` (not ported). Reference switch `switch_NCEP_st4` (GFS-Wave) |
| Kokkos code | [`../src/ww_kokkos/part_ptsort.hpp`](../src/ww_kokkos/part_ptsort.hpp), `ww::part::ptsort(nspec, ihmax, imi, ind, numv, iaddr, iorder)`; its `ind` is the input of `ww::part::pt_fld` (`part_fld.hpp`), which does not include this header |
| C shim | none yet; see "What the shim must do" |
| Translated before | none (checked kokkos/ports.tsv, kokkos/src, remote branches, PRs, 2026-10-16) |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), issue #42 |

## What the shim must do

- Pass `NSPEC` of `W3GDATMD` for the current grid and `IHMAX` of the current output set
  (`OUTPTS(IMOD)%OUT6%IHMAX`, `w3odatmd.F90:1836`), both module variables in the Fortran.
- Give `numv` and `iaddr` `IHMAX` ints each and `iorder` `NSPEC` ints, none overlapping each
  other, `imi` or `ind`: the Fortran's are automatic arrays (`w3partmd.F90:569-570`).
- Values keep the Fortran's 1-based meaning in 0-based arrays: `imi[i-1] = IMI(i)` in
  `1..IHMAX`, `ind[k-1] = IND(k)` in `1..NSPEC`.

## What WW3 does here at the pin

- `IADDR(1) = 1` is written whatever `IHMAX` is (`w3partmd.F90:587`); with `IHMAX < 1` that is a
  write past an automatic array of zero size (the object mallocs `MAX(4*MAX(IHMAX, 0), 1)` bytes, read from
  the GIMPLE). Not reachable from a mod_def built by `ww3_grid`, which sets `IHMAX = MAX(50, IHM)`
  (`w3gridmd.F90:3140`) (v); the translation keeps the same write.
- The sort relies on `IMI` lying in `1..IHMAX` and does not check it; `W3PART` guarantees it with
  `MAX(1, MIN(IHMAX, NINT(...)))` (`w3partmd.F90:349`).

## Arithmetic

Reference: the `-O3` object of `w3partmd.F90` as WW3's CMake Release build compiles it under
`switch_NCEP_st4` (gfortran 16.2, the pinned `pratico#ww3` shell), identical to the CMake build's
object (v); the C++ compiled with the `openmp-release` flags and `-ffp-contract=off`, warnings
clean (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`). Read by the translator, 2026-10-16.

- (v) Integer only: no floating point, no `MIN`/`MAX`, no `INT()`, no libm. Every operation is a
  32-bit signed add, a load or a store, in both GIMPLEs; no value can overflow (counts and
  addresses are at most `NSPEC + 1`).
- (v) The four loops in the Fortran's order and bounds: zero `NUMV(1:IHMAX)` (gfortran: a `memset`
  guarded by `IHMAX > 0`; g++: an 8-wide vector store loop, the same zeros); count per level in
  `I = 1..NSPEC` order; the prefix sum `IADDR(I+1) = IADDR(I) + NUMV(I)` for `I = 1..IHMAX-1`
  (gfortran keeps `IADDR(I)` in a register, g++ reloads it; same values); the address pass in `I`
  order, which makes the sort stable (ties keep ascending `I`); the scatter `IND(IORDER(I)) = I`.
- (v) `IADDR(1) = 1` is stored unconditionally by both, before the prefix loop.
- (v) CUDA compiled (`sm_89`, `--fmad=false`, one spectrum per thread), never run: no `FFMA`,
  `DFMA` or `MUFU` in the SASS; nothing in it depends on device math.

Commands (scratch, compile only): `dump.sh` (the object and GIMPLE), `tu.sh` (the C++ GIMPLE and
assembly), `cuda.sh` (the SASS), each run inside `nix develop picasso/labs/pratico#ww3` (`#cuda`
for the last).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
