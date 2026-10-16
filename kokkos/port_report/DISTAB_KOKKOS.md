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

Not yet. What was done, 2026-10-16 (v): a throwaway translation unit with
the `openmp-release` flags compiled the header with no warning and no FMA, and the header
compiled for CUDA (the FFMA finding above). Since then `wave_support.cpp` instantiates
`distab` on the default execution space in the `serial-debug`, `openmp-release` and
`cuda-release` builds (the CUDA one built, never run).
