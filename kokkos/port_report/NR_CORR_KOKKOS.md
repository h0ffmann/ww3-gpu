# NR_CORR_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The Newton-Raphson step `DK = f(k)/f'(k)` of the viscoelastic sea-ice dispersion relation of
`W3SIC5` (switch `IC5`), `f(k) = (C1*k**4 + C2)*k*tanh(kH) - 1`, for a complex wavenumber `k`.
For `REAL(REAL(kH)) <= 7.5` it uses the form multiplied by `cosh(kH)`,
`f = LAM*k*sinh(kH) - cosh(kH)`, `f' = LAM*kH*cosh(kH) + (LAMPR - H)*sinh(kH)`; otherwise the
`tanh` form, `f = LAM*k*tanh(kH) - 1`, `f' = LAMPR*tanh(kH) + LAM*kH*(1 - tanh(kH)**2)`, with
`LAM = C1*k**4 + C2` and `LAMPR = 5*C1*k**4 + C2` (`w3sic5md.F90:1726-1746`). `K`, `C1`, `C2` are
`COMPLEX(KDPC)` = `COMPLEX(8)` (`:143`, `:1703`; the header table at `:1653` calls `C2` real,
the declaration makes it complex), `H` is `REAL(KDP)` = `REAL(8)`. Its only caller is `NR_ROOT`
(`:1880`, `:1888`), which gfortran `-O3` inlines it into; the object also keeps
`__w3sic5md_MOD_nr_corr` out of line.

## What was ported

| | |
|---|---|
| WW3 routine | `NR_CORR`, `model/src/w3sic5md.F90:1602-1750` at NOAA-EMC/WW3 `03adda91` (arithmetic at `:1726-1746`); compiled by the `IC5` switches of `ww3_tic1.1` (`input_IC5_M1`, `_M2`, `_M3`; `git grep -lw IC5 -- '*switch*'`), by no NOAA operational switch (`model/bin/switch_*`). Only `#ifdef W3_S` (tracing, no arithmetic) |
| Kokkos code | [`../src/ww_kokkos/ic5_nr_corr.hpp`](../src/ww_kokkos/ic5_nr_corr.hpp), `ww::ic5::nr_corr(k, c1, c2, h)` with `Kokkos::complex<double>` and `double h`. It calls `ww::ic5::cmplx_tanh2` ([`ic5_cmplx_tanh2.hpp`](../src/ww_kokkos/ic5_cmplx_tanh2.hpp)), the `cexp` of [`ic5_cmplx_sinh.hpp`](../src/ww_kokkos/ic5_cmplx_sinh.hpp) and `ww::is2::gfortran_pow_c8_i4` ([`is2_funcd_fval.hpp`](../src/ww_kokkos/is2_funcd_fval.hpp)) |
| C shim | none yet; its only caller is `NR_ROOT` |
| Plan | issue #42, batch `goal/42-port-batch-3-shims` |

## Arithmetic

Reference: the `-O3` object of `w3sic5md.F90` (WW3's CMake Release flags, gfortran 16.2, the lab
switch with `NL4 IC5 BT9 NLS`, `build_mixb`), recompiled and checked identical to the CMake object,
compared with a throwaway C++ unit under the `openmp-release` flags (g++ 16.2, warning-free),
2026-10-16. Both objects were traced symbolically, path by path (the `sinh`/`tanh` branch, the two
branches of the `tanh` quotient, the two branches of the final quotient: six paths), instruction
by instruction, keeping the operand order of every operation.

- (v) Every result, call argument and branch condition is the same expression of the inputs on all
  six paths, with the same subtraction and division operand order, the same constants and no FMA
  (`-ffp-contract=off`; no `vfmadd` in the C++ object). Every non-NaN result is bit-identical.
- (v) `K**4` is not expanded by gfortran: the object calls libgfortran's `_gfortran_pow_c8_i4(K, 4)`.
  Its code in the pinned `libgfortran.so.5` was disassembled: two squarings
  `(xr*xr - xi*xi, t + t)`, `t = xr*xi`, then `(1,0)*X4` without `__muldc3`. The header calls the
  existing `ww::is2::gfortran_pow_c8_i4`; g++ folds `1.0*x` to `x`, the same bits.
- (v) `TKH = K*H` widens `H` to `(H, 0)` and multiplies in full: the `x*0.0` products stay in both
  objects. `5*C1` likewise with `(5, 0)`. `LAMPR - H` subtracts `H` from the real part only (the
  imaginary `- 0.0` is folded by both compilers). `FV = ... - 1` subtracts from the real part only.
- (v) The branch test `REAL(REAL(TKH)) <= KH_LIM` rounds the double to `float` (`cvtsd2ss`) and
  compares with `7.5f` by `comiss`/`jb` in both objects; a NaN takes the `tanh` branch in both.
- (v) In the `sinh` branch gfortran inlines `CMPLX_SINH` and `CMPLX_COSH` on `TKH` and computes one
  pair of `cexp` calls, `cexp(TKH)` first, then `cexp(-TKH)` (sign flip of both parts by `xorpd`),
  shared by the `sinh` and the `cosh` and by `FV` and `DF`. The header does the same: two `cexp`
  calls in that order, the `sinh` and `cosh` arithmetic of `ic5_cmplx_sinh.hpp`/`ic5_cmplx_cosh.hpp`
  written out on the shared pair, each with the full product by `(0.5, 0.0)`.
- (v) In the `tanh` branch the three `CMPLX_TANH2(TKH)` are one value (one `cexp` call), as in the
  header's single `cmplx_tanh2` call. `CMPLX_TANH2(TKH) ** 2.` has a `REAL` exponent: the object
  calls glibc's `cpow(T, (2.0, 0.0))`, not `T*T`. The header calls
  `std::pow(std::complex<double>, std::complex<double>(2.0, 0.0))`, which g++ lowers to the same
  `__builtin_cpow` (one `call cpow` with `(2.0, 0.0)` from `.rodata`). `1 - T**2` enters as
  `(1, 0)`: the imaginary part is `0.0 - x`, kept in both objects.
- (v) `FV / DF` is Smith's algorithm (`-fcx-fortran-rules`), no scaling, no NaN recovery; the
  branch on `|Re DF| < |Im DF|` and its unordered case go the same way in both objects.
- ⚠ At 41 commutative products and sums g++ puts the operands in the other order than gfortran
  (e.g. `c1r*K4r` against `K4r*c1r`, `cexp(-TKH)%re + cexp(TKH)%re` against `cexp(TKH)%re +
  cexp(-TKH)%re` in the `cosh` sum, and one product of the second squaring inside the shared
  `gfortran_pow_c8_i4`). x86 returns the first source's NaN when both operands are NaN, so the
  result can differ only when both operands of such a site are NaN with different encodings. g++
  chooses that order itself; swapping the source operands of the `cosh` sum did not change it.
  Every non-NaN result is bit-identical.
- ⚠ The host path calls glibc `cexp` and `cpow`. A device has neither: the device path falls back
  to `Kokkos::exp` and `Kokkos::pow`, which are not bit for bit with glibc. CUDA not compiled.
- ⚠ The Fortran result depends on libgfortran's `pow_c8_i4`; the translation matches the pinned
  toolchain only.
- (v) Fresh review 2026-10-16: no arithmetic defect. The module was recompiled from `build_mixb`'s
  flags (identical to the CMake object; only `W3_S` changes the routine) and its optimized GIMPLE
  compared statement by statement with a throwaway C++ unit (openmp-release flags, warning-free):
  the same products, subtraction and division operand order, constants `0.5`, `2.0`, `5.0`, `7.5f`,
  the shared `cexp(TKH)`, `cexp(-TKH)` pair in that order, one `cexp` and one `cpow(T, (2,0))` in the
  `tanh` branch, Smith's quotient branches, all scalar, no `vfmadd`. The libgfortran
  `pow_c8_i4` loop was read against `gfortran_pow_c8_i4` (`t + t` against `t * 2.0`, the same bits).
  The commutative swaps of the ⚠ above were seen in the GIMPLE (the `cosh` sum, `Im TKH`, `+ C2` of
  `LAM`), not recounted in the assembly. Fixed: the `C2` table line cite, `:1652` to `:1653`.

## Benchmarks

None. No timing without parity.

## How it was regtested

Not yet, and nothing was run. Its checkpoint (ADR-0011) needs a shim and `NR_ROOT`'s caller hunk;
the regtests that compile it are the `IC5` cases of `ww3_tic1.1`.
