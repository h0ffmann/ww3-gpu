# DISTAB_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the `W3IOGR` wiring first.

Fills the interpolation tables of nondimensional wavenumber `EWN1` and group velocity `ECG1`
against nondimensional frequency that `WAVNU1` reads, each entry a Newton solution of the linear
dispersion relation at depth `h = 1` through `WAVNU2`.

## What was ported

| | |
|---|---|
| WW3 routine | `DISTAB`, `model/src/w3dispmd.F90:552-683` at NOAA-EMC/WW3 `03adda91`, called from `W3IOGR` (`w3iogrmd.F90:2019`) |
| Kokkos code | [`../src/ww_kokkos/disp_distab.hpp`](../src/ww_kokkos/disp_distab.hpp), a header-only device function |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of `w3dispmd.F90` (WW3's CMake Release flags, gfortran 16.2,
byte-identical to the CMake build's inside the pinned `ww3` shell), read as GIMPLE and assembly,
2026-10-16.

- (v) `DISTAB` calls the out-of-line `__w3dispmd_MOD_wavnu2` (no clone, no inlining): `EPS` and
  `NMAX` arrive by reference from `.rodata`, `1E-7 = 0x33d6bf95` and `15`.
- (v) `SIMAX`, `DSIE` and `ECG1(0)` are folded at compile time (`DSIE = 0x3d82e8c3`,
  `ECG1(0) = SQRT(GRAV) = 0x404869bb`); `SQRT` is the `sqrtss` instruction, correctly rounded,
  so the run-time `sqrt` gives the same bits.
- (v) Same libm picture as `WAVNU2`: scalar `tanhf`/`coshf`/`sinhf`, `divss` throughout, no
  `MIN`/`MAX`, no FMA. `W3_S` (STRACE) is not compiled under the lab switch and not ported.
- ⚠ Compiled for CUDA: the SASS of the header has 87 fused multiply-adds, suspected from CUDA's
  own math routines but not traced (`WAVNU2`'s report has the detail).
- ⚠ The fresh reviewer was stopped before it reported (the session hit its usage limit); the
  translator's comparison above stands, the independent check does not.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a throwaway translation unit with
the `openmp-release` flags compiled the header with no warning and no FMA, and the header
compiled for CUDA (the FFMA finding above). The header is not in a CMake target yet.
