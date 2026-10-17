# W3DZXY_KOKKOS

**Status:** ported, phase 1, wired and regtested on the CPU (2026-10-16). WW3 calls it under `WW_KOKKOS_DZXY=1` (or `WW_KOKKOS_ALL=1`) at h0ffmann/WW3 `357f575c` (`ww3-gpu/kokkos-wave-support`); the WW3 regtest matrix has not run it ⚠.

Gradients `DZZDX`, `DZZDY` of a sea-point field `ZZ` on the rectilinear `(IY,IX)` grid, by
centred differences in the `(p,q)` index space, one-sided next to land, mapped to `(x,y)` by
`DPDX`, `DPDY`, `DQDX`, `DQDY`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3DZXY`, `model/src/w3updtmd.F90:3211-3529` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/updt_dzxy.hpp`](../src/ww_kokkos/updt_dzxy.hpp), a header-only device function |
| C shim | [`../src/fortran_iface/wave_support_shim.cpp`](../src/fortran_iface/wave_support_shim.cpp), `ww_wave_dzxy`, wired by `W3KOKKOS_DZXY` in the fork's `w3kokkosmd.F90` ([`PATCH.md`](../src/fortran_iface/PATCH.md), section 8) |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` GIMPLE of `w3updtmd.F90` (WW3's CMake Release flags, gfortran 16.2,
byte-identical to the CMake build's inside the pinned `ww3` shell), 2026-10-16. Per point and
per output: `tx = (ZZ(a) - ZZ(b)) * STX`, `ty = (ZZ(c) - ZZ(d)) * STY`,
`DZZDX = ((tx * DPDX) + (ty * DQDX)) * DFAC`, `DZZDY = ((tx * DPDY) + (ty * DQDY)) * DFAC`.

- (v) Fortran's left-to-right `a*b*c = (a*b)*c`; `tx` and `ty` shared between the two outputs
  (CSE, no bit change); no FMA (contraction off); the intermediate store of `DZZDX` before the
  `*DFAC` folded away (no bit change); neither loop nest vectorised.
- (v) A fresh reviewer found no bit change for finite inputs; the author line and the strided
  caller at `w3wavemd.F90:1507` (under `W3_REFRX`, which the lab switch does not set, passing
  `DCDX(IK,:,:)`, which a `LayoutLeft` View cannot alias: that caller must copy) were corrected
  after the review.
- ⚠ Three products are commuted in the assembly (`gy*DFAC` against `DFAC*gy`) whichever way the
  C++ source writes them; a product gives the same bits either way except which NaN payload
  survives when both operands are NaN, which needs a NaN grid metric `DPDY`/`DQDY` with a NaN
  `tx`/`ty`.
- `W3_S` (STRACE) and `W3_T` (the PRTBLK test print, `w3updtmd.F90:3486-3513`) are not compiled
  under the lab switch and not ported; `ZUNIT`, read only by that print, is not an argument.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

2026-10-16, on the CPU (Kokkos 5.2.2 OpenMP backend, gfortran/g++ 16.2.0, the pinned `ww3`
shell), never on the GPU: `scripts/ww3-gpu.sh --ww3-gpu full --cpu` with
`--kokkos all` (`WW_KOKKOS_ALL=1`) and with the W3WAVE support parts alone (PATCH.md, section 8,
"The checkpoint"). Calls (`WW_KOKKOS_WAVE_COUNT=1`): 1 call (the depth gradient `DDDX`, `DDDY`; the case has no current, and the `REFRX` call keeps the Fortran). `out_grd.ww3` (and in the smoke
`out_pnt.ww3`, `restart.ww3`) and the `ww3_ounf` NetCDF file were byte-identical (`cmp`) to the
Fortran of the same executable, and the binary files to `--ww3`, pure NOAA WW3 (v). ⚠ Not run:
the WW3 regtest matrix (`just ww3-gpu-matrix`), the OpenMP (`OMPG`) entries, the GPU; `out_grd.ww3`
holds only the fields each case requests.

Before the wiring, 2026-10-16 (v): a throwaway translation unit with the `openmp-release` flags
compiled the header with no warning and no FMA, and `wave_support.cpp` instantiates its launcher
on the default execution space in the `serial-debug`, `openmp-release` and `cuda-release` builds.
