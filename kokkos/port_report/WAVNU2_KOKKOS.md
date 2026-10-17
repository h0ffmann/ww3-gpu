# WAVNU2_KOKKOS

**Status:** ported, phase 1, wired and regtested on the CPU (2026-10-16). WW3 calls it under `WW_KOKKOS_DISTAB=1` (or `WW_KOKKOS_ALL=1`) at h0ffmann/WW3 `357f575c` (`ww3-gpu/kokkos-wave-support`); the WW3 regtest matrix has not run it ⚠.

Wavenumber and group velocity from the linear dispersion relation `w**2 = g k tanh(k h)` by
Newton iteration, the general-depth form `DISTAB` fills its tables with.

## What was ported

| | |
|---|---|
| WW3 routine | `WAVNU2`, `model/src/w3dispmd.F90:204-346` at NOAA-EMC/WW3 `03adda91` |
| Kokkos code | [`../src/ww_kokkos/disp_distab.hpp`](../src/ww_kokkos/disp_distab.hpp), a header-only device function |
| C shim | [`../src/fortran_iface/wave_support_shim.cpp`](../src/fortran_iface/wave_support_shim.cpp), `ww_wave_distab (inside DISTAB)`, wired by `W3KOKKOS_DISTAB` in the fork's `w3kokkosmd.F90` ([`PATCH.md`](../src/fortran_iface/PATCH.md), section 8) |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

`WAVNU2` is also translated inside `st4_insin4.cpp` for `INSIN4`; that copy is left alone (the
ST4 checkpoint runs on it) and this one belongs to `DISTAB`.

## Arithmetic

The reference is the `-O3` object of `w3dispmd.F90` (WW3's CMake Release flags, gfortran 16.2,
byte-identical to the CMake build's inside the pinned `ww3` shell), read as GIMPLE and assembly,
2026-10-16.

- (v) `TANH`, `COSH`, `SINH` are scalar calls of glibc's `tanhf`, `coshf`, `sinhf`; no sincos,
  no libmvec, no vectorised loop.
- (v) `(COSH(x))**2` is `c*c`; `W0**2` and `W0*W0` are both `w*w` on the signed `W` (same bits);
  every division is a `divss`, never a reciprocal multiply; no `MIN`/`MAX`; no FMA.
- (v) Fresh review, 2026-10-16: the optimised GIMPLE of `WAVNU2` and `DISTAB` (gfortran 16.2,
  `-O3`, WW3's flags, the pinned `ww3` shell) matches the header statement by statement: the
  folded `EPS` `0x33d6bf95`, `DSIE` `0x3d82e8c3`, `ECG1(0)` `0x404869bb`, the loop exit and the
  `ICON` tests; `2*K*H` is `H*(K*2)` in the GIMPLE and `H*(K+K)` in the header, the same bits.
  The C++ object has no `vfmadd`. The copy of `WAVNU2` in `st4_insin4.cpp:37-90` agrees.
- (v) CUDA, `sm_89`, `--fmad=false`, compiled and never run: the 87 fused multiply-adds of the
  `DISTAB` kernel are 50 in ten correctly rounded divisions (`MUFU.RCP`, `FCHK` and five `FFMA`
  each, header lines 85, 87, 92, 109, 111, 120, 124), 12 in the division slow path and 25 in
  libdevice's `tanhf` (12), `sinhf` (7), `coshf` (6); the header's own products and sums stay
  `FMUL` and `FADD`.
- ⚠ Whether libdevice's `tanhf`, `coshf`, `sinhf` return glibc's bits is not known, and that is
  the GPU risk; `DISTAB` runs once at set-up, so its shim may compute on the host and copy.
- ⚠ `-ffp-contract=off` is private to the `ww_kokkos` target: code outside it that includes this
  header would let GCC fuse lines 102, 109 or 124.
- The reviewer also linked the Fortran object and the header into a scratch program, against the
  owner's "no test run", and found 0 differing bits in `K`, `CG`, `ICON` over 1 110 523 034
  `WAVNU2` calls (every float `W` in [0,8], 2×10⁷ random inputs, special values) and in the 122
  `DISTAB` entries; it is not committed and is not evidence under ADR-0010.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

2026-10-16, on the CPU (Kokkos 5.2.2 OpenMP backend, gfortran/g++ 16.2.0, the pinned `ww3`
shell), never on the GPU: `scripts/ww3-gpu.sh --ww3-gpu smoke --cpu` with
`--kokkos all` (`WW_KOKKOS_ALL=1`) and with the W3WAVE support parts alone (PATCH.md, section 8,
"The checkpoint"). Calls (`WW_KOKKOS_WAVE_COUNT=1`): 121 calls per DISTAB call; 0 of the 246 table values it produces differ from the Fortran's. `out_grd.ww3` (and in the smoke
`out_pnt.ww3`, `restart.ww3`) and the `ww3_ounf` NetCDF file were byte-identical (`cmp`) to the
Fortran of the same executable, and the binary files to `--ww3`, pure NOAA WW3 (v). ⚠ Not run:
the WW3 regtest matrix (`just ww3-gpu-matrix`), the OpenMP (`OMPG`) entries, the GPU; `out_grd.ww3`
holds only the fields each case requests.

Before the wiring, 2026-10-16 (v): a throwaway translation unit with the `openmp-release` flags
compiled the header with no warning and no FMA, and `wave_support.cpp` instantiates its launcher
on the default execution space in the `serial-debug`, `openmp-release` and `cuda-release` builds.
