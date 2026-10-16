# CMPLX_COSH_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Complex hyperbolic cosine of a `COMPLEX(KDPC)` argument, `(EXP(X) + EXP(-X)) * 0.5`, a helper of
the viscoelastic sea-ice dispersion solver of `W3SIC5` (switch `IC5`). `KDPC = KIND((1.0D0, 1.0D0))`
(`w3sic5md.F90:143`), so the argument is `COMPLEX(8)`. Its only caller is the Newton-Raphson
correction `NR_CORR` (`w3sic5md.F90:1735-1736`, `FV` and `DF`), which gfortran `-O3` inlines.

## What was ported

| | |
|---|---|
| WW3 routine | `CMPLX_COSH`, `model/src/w3sic5md.F90:2050-2144` at NOAA-EMC/WW3 `03adda91` (arithmetic at `:2140`); compiled by the `IC5` switches of `ww3_tic1.1` (`input_IC5_M1`, `_M2`, `_M3`; `git grep -lw IC5 -- '*switch*'`), by no NOAA operational switch (`model/bin/switch_*`). Only `#ifdef W3_S` (tracing, no arithmetic) |
| Kokkos code | [`../src/ww_kokkos/ic5_cmplx_cosh.hpp`](../src/ww_kokkos/ic5_cmplx_cosh.hpp), `ww::ic5::cmplx_cosh(x)` with `Kokkos::complex<double>` |
| C shim | none yet; its only caller is `NR_CORR` |
| Plan | issue #42, batch `goal/42-port-batch-easy` |

## Arithmetic

Reference: the `-O3` object of `w3sic5md.F90` (WW3's CMake Release flags, gfortran 16.2, the lab
switch with `NL4 IC5 BT9 NLS`, `build_mixb`), recompiled and checked identical to the CMake object,
compared with a throwaway C++ unit under the `openmp-release` flags (g++ 16.2, warning-free),
2026-10-16.

- (v) `EXP` of a `COMPLEX(8)` is glibc's `cexp`: GIMPLE `__builtin_cexp`, two `call cexp` in
  `__w3sic5md_MOD_cmplx_cosh`. The host path calls `std::exp(std::complex<double>)`, which libstdc++
  lowers to the same `__builtin_cexp`; the C++ object has the same two `cexp` calls. `Kokkos::exp`
  of a complex is not used on the host: it computes `exp(re)*(cos(im), sin(im))` itself.
- (v) `-X` negates both parts by sign flip (`xorpd` in both objects), not `0 - X`.
- (v) The default `REAL` 0.5 is promoted to `(0.5, 0.0)` and gfortran multiplies in full without
  NaN recovery (no `__muldc3`): real `s_r*0.5 - s_i*0.0`, imaginary `s_r*0.0 + s_i*0.5`, with
  `s = EXP(X) + EXP(-X)` summed per part. The header writes those four products explicitly; g++
  keeps the products by 0.0 (no `-ffast-math`), so an infinite part becomes NaN as in the Fortran.
- (v) g++ packs the four products into two `vmulpd` and one `vaddsubpd`: lane 0 is
  `s_r*0.5 - s_i*0.0`, lane 1 `s_r*0.0 + s_i*0.5`, the same IEEE double operations, the same
  rounding, no FMA (`-ffp-contract=off`).
- (v) The two sums `EXP(X) + EXP(-X)` take `cexp(X)` as first source in both objects: gfortran
  adds with `cexp(X)` as destination (`w3sic5md.o` `__w3sic5md_MOD_cmplx_cosh+0x67c`, `+0x680`);
  the header calls `cexp(-X)` first, which makes g++ emit `vaddsd` with `cexp(X)` as first source
  (`tu.o` `+0x49`, `+0x4e`). With the calls in Fortran order g++ swapped the sources, so a part NaN
  in both `cexp` results (`X` NaN, or an infinite imaginary part) took the payload and sign of
  `cexp(-X)`. `cexp` is pure, so the call order does not change its bits. The ordering is what g++
  16.2 emits for this unit; a caller that inlines it must check its own object.
- (v) Inside `NR_CORR` gfortran inlines `CMPLX_SINH` and `CMPLX_COSH` on the same `TKH` and reuses
  the two `cexp` results of the sinh for the cosh (one pair of `cexp` calls); `cexp` is pure, so
  the bits are those of this routine. The port of `NR_CORR` has to be compared on its own object.
- (v) Consistent with `ic5_cmplx_sinh.hpp` (same `cexp` path, same full product by `(0.5, 0.0)`);
  both headers compile in one unit.
- (v) Fresh review 2026-10-16: `w3sic5md.F90` recompiled from `build_mixb`'s flags, object identical
  to the CMake one; GIMPLE and assembly of `CMPLX_COSH` compared with the C++ unit operation by
  operation (both `cexp` calls, sign-flip negation, the two sums, the four products by `0.5` and
  `0.0`, the subtract and add with the Fortran's first sources, no FMA, no `__muldc3`). Found the
  sum operands swapped (NaN payload and sign when both parts are NaN); fixed by calling `cexp(-X)`
  first, recompiled and read again in the object. Only `W3_S` (tracing) varies the routine.
- ⚠ Not compiled for CUDA. A device has no glibc `cexp`: the device path uses `Kokkos::exp`, which
  is not bit for bit with it (`AGENTS.md`, bit for bit).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
