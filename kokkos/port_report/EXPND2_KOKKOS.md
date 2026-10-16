# EXPND2_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The routine internal to `W3SNL3` that copies one compact `(NTH,NFRCUT)` block of a Generalized
Multiple DIA (`NL3`) contribution into the array extended in direction and frequency, so the
quadruplet loop of `W3SNL3` addresses neighbours without index wrapping. `W3SNL3` calls it five
times, on `DSB` and `DD1` to `DD4` (`w3snl3md.F90:548-552`); nothing else calls it.

## What was ported

| | |
|---|---|
| WW3 routine | `EXPND2`, `model/src/w3snl3md.F90:717-773` at NOAA-EMC/WW3 `03adda91`, internal to `W3SNL3`. Compiled under `NL3`: `switch_NCEP_glwu`, `switch_UoM_nl3`, `switch_UoM_nl3s` and the `ww3_ts1` regtest switch `switch_ST4_GMD` (`git grep -lw NL3 -- '*switch*'`). No `#ifdef` inside it |
| Kokkos code | [`../src/ww_kokkos/snl3_gmd.hpp`](../src/ww_kokkos/snl3_gmd.hpp), `snl3_detail::expnd2`, translated with `W3SNL3` ([`W3SNL3_KOKKOS.md`](W3SNL3_KOKKOS.md)) and checked again here without a change; `TEMP` (`:754`) is the caller's `Snl3Scratch::temp` of `NTH*NFRCUT` |
| C shim | none yet; it is reached only through `W3SNL3`'s |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3snl3md.F90` (WW3's CMake Release flags, gfortran 16.2, the
lab switch with `NL3` in place of `NL1`), recompiled with `-fdump-tree-optimized-lineno`; its
disassembly is identical to the CMake object's. Read 2026-10-16.

- (v) `EXPND2` stays out of line (local symbol `expnd2.2`). Its body is data movement only: a
  `malloc` of `TEMP`, a `memcpy` per column of `ARIN` into `TEMP` (`:758`), a `memset` to zero of
  `AROUT(:,NFRMIN:0)` (`:760`), a `memcpy` per column of `TEMP` into `AROUT(1:NTH,:)` (`:762`),
  then scalar element copies for `:765` and `:766`, the `:765` loop over frequencies before the
  `:766` loop, per `ITH`. No floating-point arithmetic, no libm call. The C++ does the same copies
  in the same order; its `-O3` object (openmp-release flags, `-ffp-contract=off`) has only
  `vmovss` among floating-point instructions, so the bits of every element, NaN payloads included,
  are those of the Fortran.
- (v) The order of the direction wrap (`:764-767`) is kept: with `NTHMAX > NTH` a row read at
  `:765` or `:766` was written by an earlier iteration of the same loop, and the C++ reads it
  after that write too.
- (v) The index map: `AROUT(ITH,IFR)` of the dummy `(1-NTHMAX:NTH+NTHMAX, NFRMIN:NFRCUT)` is
  element `ITH + (IFR-1)*NTHEXP` of the actual `(NSPMIN:NSPMX2)` array, since
  `NSPMIN = 1 + (NFRMIN-1)*NTHEXP - NTHMAX` (`:1155`); the C++ writes `arr[ith + (ifr-1)*nthexp - nspmin]`.
- ⚠ The calls `EXPND2(DSB(1:NTH*NFRCUT), DSB)` (`:548-552`) associate `ARIN` (`INTENT(IN)`) and
  `AROUT` (`INTENT(OUT)`) with the same storage and then define `AROUT`, which the Fortran standard
  does not allow. The result is defined only because the `-O3` object passes the section without
  a copy and finishes the `memcpy` into `TEMP` before its first store to `AROUT`; the C++ copies
  into `temp` first as well. A different compiler or flag set could reorder this in the Fortran.
- (v) Fresh review 2026-10-16: no defect. The `expnd2.2` GIMPLE was read again (`malloc` of `TEMP`, `memcpy` from `ARIN` before the `memset` and the `memcpy` into `AROUT`, then the `:765`/`:766` loops). A new C++ unit's `expnd2` has 8 `vmovss` and no other floating-point instruction or call. Nothing changed.
- ⚠ CUDA not compiled for this check (data movement only, the same code as the host).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
