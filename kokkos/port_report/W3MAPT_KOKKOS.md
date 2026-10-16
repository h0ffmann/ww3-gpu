# W3MAPT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the `W3WAVE` wiring first.

The map `MAPTRN` of the `PR3`/`UQ` scheme: a sea point whose smallest cell-boundary
transparency (`ATRNX`, `ATRNY`, from `W3UTRN`) is below `TRNMIN = 0.95` is flagged, so the
propagation turns the GSE alleviation off there.

## What was ported

| | |
|---|---|
| WW3 routine | `W3MAPT`, `model/src/w3pro3md.F90:527-625` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`W3_S` out); called by `W3WAVE` at `w3wavemd.F90:1467`, after `W3MAP3` and `W3UTRN` |
| Kokkos code | [`../src/ww_kokkos/prop_mapt.hpp`](../src/ww_kokkos/prop_mapt.hpp), a header-only device function (`w3mapt_point`) and its launcher (`w3mapt`); instantiated by `wave_support.cpp` in every preset |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of `w3pro3md.F90` (WW3's CMake Release flags, gfortran 16.2),
rebuilt by the translator byte-identical to the CMake build's object, 2026-10-16.

- (v) `:615-616`: `minss` of `ATRNY(+1)` with `ATRNY(-1)`, of `ATRNX(+1)` with `ATRNX(-1)`, then
  of the two results, the `Y` result first; the header's `min_r(a,b) = a<b ? a : b` with the same
  first operands gives the same result for NaN and ±0 ties (translator: C++ object; reviewer:
  Fortran object).
- (v) `TRNMIN = 0.95` (`w3pro3md.F90:129`) as `0x3f733333` in both objects; `comiss` then `seta`,
  true when `0.95 > m`, false for a NaN `m`; the header's `m < kTrnMin`. No FMA, no float
  arithmetic beyond the minima.
- (v) Layout: `ATRNX`/`ATRNY` are `(NY*NX,-1:1)` (`w3adatmd.F90:1370`), `MAPTRN` is `(NY*NX)`
  (`:1445`); `(IXY,K)` maps to `(IXY-1,K+1)` as in `updt_utrn.hpp`, and `prop_xyp3.hpp` reads
  `maptrn[ixy-1] != 0`.
- (v) The routine sets `MAPTRN` at sea cells only and never initialises the others; the port
  leaves them untouched too.
- (v) CUDA (`sm_89`, `--fmad=false`, compile only): each minimum is `FSETP.GEU` with `FSEL`,
  `a<b ? a : b` with `minss`'s NaN behaviour; no `FMNMX`.
- ⚠ Points run in parallel: safe because `MAPSF(:,3)` is one cell per sea point
  (`w3gridmd.F90:5184`, checked for `W3UTRN`), and nothing is reduced.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a translator compared the C++
object (`openmp-release` flags, and the CUDA SASS) with the Fortran object, and a fresh reviewer
re-read the Fortran object and the logic and found no defect.
