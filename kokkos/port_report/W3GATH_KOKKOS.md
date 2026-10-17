# W3GATH_KOKKOS

**Status:** ported, phase 1, wired and regtested on the CPU (2026-10-16). WW3 calls it under `WW_KOKKOS_GATH=1` (or `WW_KOKKOS_ALL=1`) at h0ffmann/WW3 `357f575c` (`ww3-gpu/kokkos-wave-support`); the WW3 regtest matrix has not run it ⚠.

Gathers one spectral component `ISPEC` of the sea-point spectra `VA` into a 2-D field over the
grid, before the spatial propagation in `W3WAVE`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3GATH`, `model/src/w3wavemd.F90:3024-3329` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`SHRD` branch only, `w3wavemd.F90:3158-3166`) |
| Kokkos code | [`../src/ww_kokkos/wave_gath_scat.hpp`](../src/ww_kokkos/wave_gath_scat.hpp), a header-only device function, one `parallel_for` over `ISEA` |
| C shim | [`../src/fortran_iface/wave_support_shim.cpp`](../src/fortran_iface/wave_support_shim.cpp), `ww_wave_gath`, wired by `W3KOKKOS_GATH` in the fork's `w3kokkosmd.F90` ([`PATCH.md`](../src/fortran_iface/PATCH.md), section 8) |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of `w3wavemd.F90` (WW3's CMake Release flags, gfortran 16.2,
byte-identical to the CMake build's inside the pinned `ww3` shell), read as GIMPLE and assembly,
2026-10-16.

- (v) `FIELD = 0.` is one `__builtin_memset` of the whole extent; both `ISEA` loops are scalar
  and do no floating-point arithmetic, only an integer load of `MAPSF(ISEA,3)` and a `movss`
  load/store of the `REAL`, which moves NaN payloads and `-0.0` unchanged.
- (v) `parallel_for` over `ISEA` gives the same bits: each iteration is a pure copy and the
  targets do not collide; `MAPSF(:,3)` is injective in `ISEA` on a rectilinear grid
  (`w3iogrmd.F90:1073`, `w3gridmd.F90:5020-5028`). Grids built elsewhere (SMC, unstructured)
  are not checked ⚠.
- (v) A fresh reviewer found no defect: index bases, the `VA` column (`VA(NSPEC,0:NSEALM)`,
  `w3wdatmd.F90:491`), the zero fill and the injectivity argument all hold.
- The `W3_MPI` buffer management, the `W3_MPIT` test print and `W3_S` STRACE are not compiled
  under the lab switch and are not ported.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

2026-10-16, on the CPU (Kokkos 5.2.2 OpenMP backend, gfortran/g++ 16.2.0, the pinned `ww3`
shell), never on the GPU: `scripts/ww3-gpu.sh --ww3-gpu full --cpu` with
`--kokkos all` (`WW_KOKKOS_ALL=1`) and with the W3WAVE support parts alone (PATCH.md, section 8,
"The checkpoint"). Calls (`WW_KOKKOS_WAVE_COUNT=1`): 13 824 calls (576 spectral bins x 24 propagation steps). `out_grd.ww3` (and in the smoke
`out_pnt.ww3`, `restart.ww3`) and the `ww3_ounf` NetCDF file were byte-identical (`cmp`) to the
Fortran of the same executable, and the binary files to `--ww3`, pure NOAA WW3 (v). ⚠ Not run:
the WW3 regtest matrix (`just ww3-gpu-matrix`), the OpenMP (`OMPG`) entries, the GPU; `out_grd.ww3`
holds only the fields each case requests.

Before the wiring, 2026-10-16 (v): a throwaway translation unit with the `openmp-release` flags
compiled the header with no warning and no FMA, and `wave_support.cpp` instantiates its launcher
on the default execution space in the `serial-debug`, `openmp-release` and `cuda-release` builds.
