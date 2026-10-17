# W3MAP3_KOKKOS

**Status:** ported, phase 1, wired and regtested on the CPU (2026-10-16). WW3 calls it under `WW_KOKKOS_MAP3=1` (or `WW_KOKKOS_ALL=1`) at h0ffmann/WW3 `357f575c` (`ww3-gpu/kokkos-wave-support`); the WW3 regtest matrix has not run it ⚠.

The index maps of the `PR3`/`UQ` scheme: `MAPX2`, `MAPY2`, `MAPAXY` (the points of each
one-dimensional pass), `MAPCXY` (sea points, centred ones first), `MAPTH2`, `MAPWN2` (the
spectral passes) and the counters `NMX0..2`, `NMY0..2`, `NACT`, `NCENT` that `W3XYP3` and
`W3KTP3` read.

## What was ported

| | |
|---|---|
| WW3 routine | `W3MAP3`, `model/src/w3pro3md.F90:139-518` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it; called by `W3WAVE` at `w3wavemd.F90:1463`, before `W3UTRN` and `W3MAPT` |
| Kokkos code | [`../src/ww_kokkos/prop_map3.hpp`](../src/ww_kokkos/prop_map3.hpp), a header-only function (`w3map3`) run as one kernel iteration; instantiated by `wave_support.cpp` in every preset |
| C shim | [`../src/fortran_iface/wave_support_shim.cpp`](../src/fortran_iface/wave_support_shim.cpp), `ww_wave_map3`, wired by `W3KOKKOS_MAP3` in the fork's `w3kokkosmd.F90` ([`PATCH.md`](../src/fortran_iface/PATCH.md), section 8) |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

None: the routine is integer only. Its `-O3` object (WW3's CMake Release flags, gfortran 16.2,
rebuilt byte-identical to the CMake build's) has no floating-point operation and no vectorised
loop, and neither has the C++ object (`openmp-release` flags) (v, translator, 2026-10-16).

- (v) Serial, in source order: the counters number the entries in loop order and `MAPCXY` fills
  from both ends, so a parallel scan would be a reorganisation, not a translation. It runs only
  when `FLMAP` is set.
- (v) Fresh review against the preprocessed Fortran: loop bounds and nesting of all seven map
  loops (`:263-491`), `IX2 = 1+MOD(IX,NX)`, the counter chains, every stored value `IXY0+IY`
  (1-based, at 0-based slot `n-1`), `MAPCXY`'s tests in order (`:399-427`), the
  `MAPTH2(1) .NE. 0` return after the `GTYPE` block, the `MAPWN2` sequence, and the `GTYPE < 3`
  branch (other grids keep the caller's counters). The extents of `w3gdatmd.F90:1733` and
  `w3adatmd.F90:1439-1447`, and the reads of `prop_xyp3.hpp`, `prop_ktp3.hpp`, `prop_params.hpp`,
  agree.
- (v) `MAPSTA` is read at the Fortran's column-major address `(IX-1)*NY+IY-1`, so its reads of
  rows 0 and `NY+1` hit the same element as the Fortran's.
- ⚠ At the two ends of the array (`IX = 2, IY = 1` and `IX = NX-1, IY = NY`) such a read leaves
  the array in both; W3GRID sets the sea points of rows 1 and `NY` to `NSTAT = 2` or `-1`
  (`w3gridmd.F90:4581-4584,4773-4776`), so the first branch catches them, but whether a
  run-time `MAPSTA` update (ice, water level) can bring one back to 1 was not checked.
- ⚠ The header zeroes `mapcxy.extent(0)` elements: equal to the Fortran's `MAPCXY = 0` only when
  the shim sizes the View to `NSEA`.
- ⚠ One thread on a GPU is slow on a large grid; that is the price of keeping the order.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

2026-10-16, on the CPU (Kokkos 5.2.2 OpenMP backend, gfortran/g++ 16.2.0, the pinned `ww3`
shell), never on the GPU: `scripts/ww3-gpu.sh --ww3-gpu full --cpu` with
`--kokkos all` (`WW_KOKKOS_ALL=1`) and with the W3WAVE support parts alone (PATCH.md, section 8,
"The checkpoint"). Calls (`WW_KOKKOS_WAVE_COUNT=1`): 1 call on the 120 x 80 grid (the smoke has one cell). `out_grd.ww3` (and in the smoke
`out_pnt.ww3`, `restart.ww3`) and the `ww3_ounf` NetCDF file were byte-identical (`cmp`) to the
Fortran of the same executable, and the binary files to `--ww3`, pure NOAA WW3 (v). ⚠ Not run:
the WW3 regtest matrix (`just ww3-gpu-matrix`), the OpenMP (`OMPG`) entries, the GPU; `out_grd.ww3`
holds only the fields each case requests.

Before the wiring, 2026-10-16 (v): a throwaway translation unit with the `openmp-release` flags
compiled the header with no warning and no FMA, and `wave_support.cpp` instantiates its launcher
on the default execution space in the `serial-debug`, `openmp-release` and `cuda-release` builds.
