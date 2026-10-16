# WAVNU2_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the `W3IOGR` wiring first.

Wavenumber and group velocity from the linear dispersion relation `w**2 = g k tanh(k h)` by
Newton iteration, the general-depth form `DISTAB` fills its tables with.

## What was ported

| | |
|---|---|
| WW3 routine | `WAVNU2`, `model/src/w3dispmd.F90:204-346` at NOAA-EMC/WW3 `03adda91` |
| Kokkos code | [`../src/ww_kokkos/disp_distab.hpp`](../src/ww_kokkos/disp_distab.hpp), a header-only device function |
| C shim | none yet |
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
- ⚠ Compiled for CUDA: the SASS of the header has 87 fused multiply-adds, suspected from CUDA's
  own `tanh`/`cosh`/`sinh` and division routines but not traced; `--fmad=false` governs the
  port's own expressions, not CUDA's math library. On the GPU the bit-for-bit rule needs those
  bits (`AGENTS.md`, "Bit for bit with the Fortran").
- ⚠ The fresh reviewer was stopped before it reported (the session hit its usage limit); the
  translator's comparison above stands, the independent check does not.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a throwaway translation unit with
the `openmp-release` flags compiled the header with no warning and no FMA, and the header
compiled for CUDA (the FFMA finding above). The header is not in a CMake target yet.
