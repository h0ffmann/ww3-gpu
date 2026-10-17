# W3UWND_KOKKOS

**Status:** ported, phase 1, wired and regtested on the CPU (2026-10-16). WW3 calls it under `WW_KOKKOS_UWND=1` (or `WW_KOKKOS_ALL=1`) at h0ffmann/WW3 `357f575c` (`ww3-gpu/kokkos-wave-support`); the WW3 regtest matrix has not run it ⚠.

Wind at the model time by linear interpolation of speed, direction and air-sea temperature
difference between the two input fields, then the 10 m wind relative to the surface current.

## What was ported

| | |
|---|---|
| WW3 routine | `W3UWND`, `model/src/w3updtmd.F90:488-822` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`WNT1` and `RWND` branches, `w3updtmd.F90:684,746-770`) |
| Kokkos code | [`../src/ww_kokkos/updt_uwnd.hpp`](../src/ww_kokkos/updt_uwnd.hpp), a header-only device function |
| C shim | [`../src/fortran_iface/wave_support_shim.cpp`](../src/fortran_iface/wave_support_shim.cpp), `ww_wave_uwnd`, wired by `W3KOKKOS_UWND` in the fork's `w3kokkosmd.F90` ([`PATCH.md`](../src/fortran_iface/PATCH.md), section 8) |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of `w3updtmd.F90` (WW3's CMake Release flags, gfortran 16.2,
byte-identical to the CMake build's inside the pinned `ww3` shell), read as GIMPLE and assembly,
2026-10-16.

- (v) `SQRT(x**2+y**2)` is `(x*x)+(y*y)` then `__builtin_sqrtf` (`sqrtss`), not `hypotf`.
- (v) `ATAN2` is a libm `atan2f` call; `COS`/`SIN` of `UD` are one `sincosf` call.
- (v) `MOD(a,TPI)` is `__builtin_fmodf`, expanded inline as an x87 `fprem` loop (exact
  remainder); g++ expands `std::fmod` to the same loop and, under `-fmath-errno`, calls `fmodf`
  on a NaN result only to set `errno`, so the returned bits match for every input.
- (v) `SIGN(1.,DD)` is `copysignf(1.,DD)`; `ABS` is a mask; the `MAX` of the operand order the
  assembly emits.
- Not compiled under the lab switch and not ported: `WNT0`, `WNT2`, `OASACM`, `MGW` (so `VGX`,
  `VGY` are unused), `SMC`, `WCOR`, `STAB2`, `OMPG`, `S`, `T`. `DSEC21` (`w3timemd.F90`) is
  integer and double time arithmetic on the host: the caller computes `DT0N` and `DT0T` and
  passes the `REAL`s the Fortran stores (`w3updtmd.F90:677-678`).
- (v) Fresh review, 2026-10-16: the reviewer rebuilt the Fortran object (WW3's flags plus the
  nix wrapper's hardening flags, without which the object differs) identical to the CMake
  build's, and found no arithmetic defect: no loop of `:488-822` vectorised, no FMA, the
  constants `1.E-7`, `0.001`, `PI`, `TPI` with the same bits, `MAX` operand order at `:684`,
  `:753`, `:767` the same (a NaN gives the constant), every `UA0`/`UAI` branch the same. One
  comment cited the wrong allocation line for `U10`, now `w3adatmd.F90:1015`.
- ⚠ On CUDA, `atan2f`, `fmodf`, `sincosf` are CUDA's own; at `-O0` (`serial-debug`) `fmod` and
  `sqrt` are libm calls, NaN payloads not compared.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

2026-10-16, on the CPU (Kokkos 5.2.2 OpenMP backend, gfortran/g++ 16.2.0, the pinned `ww3`
shell), never on the GPU: `scripts/ww3-gpu.sh --ww3-gpu smoke|full --cpu` with
`--kokkos all` (`WW_KOKKOS_ALL=1`) and with the W3WAVE support parts alone (PATCH.md, section 8,
"The checkpoint"). Calls (`WW_KOKKOS_WAVE_COUNT=1`): 2 161 calls in the smoke, 25 in the full. `out_grd.ww3` (and in the smoke
`out_pnt.ww3`, `restart.ww3`) and the `ww3_ounf` NetCDF file were byte-identical (`cmp`) to the
Fortran of the same executable, and the binary files to `--ww3`, pure NOAA WW3 (v). ⚠ Not run:
the WW3 regtest matrix (`just ww3-gpu-matrix`), the OpenMP (`OMPG`) entries, the GPU; `out_grd.ww3`
holds only the fields each case requests.

Before the wiring, 2026-10-16 (v): a throwaway translation unit with the `openmp-release` flags
compiled the header with no warning and no FMA, and `wave_support.cpp` instantiates its launcher
on the default execution space in the `serial-debug`, `openmp-release` and `cuda-release` builds.
