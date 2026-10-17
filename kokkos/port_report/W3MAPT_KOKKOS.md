# W3MAPT_KOKKOS

**Status:** ported, phase 1, wired and regtested on the CPU (2026-10-16). WW3 calls it under `WW_KOKKOS_MAPT=1` (or `WW_KOKKOS_ALL=1`) at h0ffmann/WW3 `357f575c` (`ww3-gpu/kokkos-wave-support`); the WW3 regtest matrix has not run it ⚠.

The map `MAPTRN` of the `PR3`/`UQ` scheme: a sea point whose smallest cell-boundary
transparency (`ATRNX`, `ATRNY`, from `W3UTRN`) is below `TRNMIN = 0.95` is flagged, so the
propagation turns the GSE alleviation off there.

## What was ported

| | |
|---|---|
| WW3 routine | `W3MAPT`, `model/src/w3pro3md.F90:527-625` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`W3_S` out); called by `W3WAVE` at `w3wavemd.F90:1467`, after `W3MAP3` and `W3UTRN` |
| Kokkos code | [`../src/ww_kokkos/prop_mapt.hpp`](../src/ww_kokkos/prop_mapt.hpp), a header-only device function (`w3mapt_point`) and its launcher (`w3mapt`); instantiated by `wave_support.cpp` in every preset |
| C shim | [`../src/fortran_iface/wave_support_shim.cpp`](../src/fortran_iface/wave_support_shim.cpp), `ww_wave_mapt`, wired by `W3KOKKOS_MAPT` in the fork's `w3kokkosmd.F90` ([`PATCH.md`](../src/fortran_iface/PATCH.md), section 8) |
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

2026-10-16, on the CPU (Kokkos 5.2.2 OpenMP backend, gfortran/g++ 16.2.0, the pinned `ww3`
shell), never on the GPU: `scripts/ww3-gpu.sh --ww3-gpu full --cpu` with
`--kokkos all` (`WW_KOKKOS_ALL=1`) and with the W3WAVE support parts alone (PATCH.md, section 8,
"The checkpoint"). Calls (`WW_KOKKOS_WAVE_COUNT=1`): 1 call on the 120 x 80 grid. `out_grd.ww3` (and in the smoke
`out_pnt.ww3`, `restart.ww3`) and the `ww3_ounf` NetCDF file were byte-identical (`cmp`) to the
Fortran of the same executable, and the binary files to `--ww3`, pure NOAA WW3 (v). ⚠ Not run:
the WW3 regtest matrix (`just ww3-gpu-matrix`), the OpenMP (`OMPG`) entries, the GPU; `out_grd.ww3`
holds only the fields each case requests.

Before the wiring, 2026-10-16 (v): a throwaway translation unit with the `openmp-release` flags
compiled the header with no warning and no FMA, and `wave_support.cpp` instantiates its launcher
on the default execution space in the `serial-debug`, `openmp-release` and `cuda-release` builds.
