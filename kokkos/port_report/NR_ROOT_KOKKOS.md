# NR_ROOT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The Newton-Raphson iteration of `W3SIC5` (switch `IC5`) for the complex wavenumber `k` of the
viscoelastic sea-ice dispersion relation: from `K0 = GUESS` (a root of `POLYROOTS`), repeat
`K1 = K0 - DK`, `DK = NR_CORR(K0, C1, C2, H)`, while `ABS(DK) > ERRTOL`; when `IC5PARS(7) > 0.5`
a random kick `K1 = K1 + 2*TRANVAL` is added whenever `ABS(REAL(K1)) < IC5PARS(8)`, and the run
stops after `IC5PARS(6)` iterations (`w3sic5md.F90:1876-1915`). `C1`, `C2`, `GUESS` are
`COMPLEX(KDPC)` = `COMPLEX(8)` (`:143`, `:1854`; the header table at `:1797` calls `C2` real, the
declaration makes it complex), `H` is `REAL(KDP)`. Its only caller is `FSDISP` (`:875`).

## What was ported

| | |
|---|---|
| WW3 routine | `NR_ROOT`, `model/src/w3sic5md.F90:1765-1924` at NOAA-EMC/WW3 `03adda91` (arithmetic at `:1876-1915`); compiled by the `IC5` switches of `ww3_tic1.1` (`input_IC5_M1`, `_M2`, `_M3`; `git grep -lw IC5 -- '*switch*'`), by no NOAA operational switch (`model/bin/switch_*`). Only `#ifdef W3_S` (tracing, no arithmetic) |
| Kokkos code | [`../src/ww_kokkos/ic5_nr_root.hpp`](../src/ww_kokkos/ic5_nr_root.hpp), `ww::ic5::nr_root(c1, c2, h, guess, root, ic5maxiter, ic5rkick, ic5kfilter, init_random_seed, random_r4)`, returning 0 or `kTooManyIterations`. It calls `ww::ic5::nr_corr` ([`ic5_nr_corr.hpp`](../src/ww_kokkos/ic5_nr_corr.hpp), [NR_CORR_KOKKOS.md](NR_CORR_KOKKOS.md)) |
| Not translated | `INIT_RANDOM_SEED` (`:2266-2347`, `SYSTEM_CLOCK` and `RANDOM_SEED`) and `RANDOM_NUMBER`: libgfortran's generator stays with the caller, passed in as the callables `init_random_seed()` and `random_r4()` |
| Structural change | the `WRITE(NDSE, 1001)` and `EXTCDE(1)` of `:1908-1911` become the return code `kTooManyIterations`; the shim writes and stops. `root` is not written then |
| C shim | none yet; its only caller is `FSDISP` |
| Plan | issue #42, batch `goal/42-port-batch-3-shims` |

## Arithmetic

Reference: the `-O3` object of `w3sic5md.F90` (WW3's CMake Release flags, gfortran 16.2, the lab
switch with `NL4 IC5 BT9 NLS`, `build_mixb`), recompiled and checked identical to the CMake object,
compared with a throwaway C++ unit under the `openmp-release` flags (g++ 16.2, warning-free),
2026-10-16.

- (v) gfortran inlines `NR_CORR` into `NR_ROOT` twice, once in each copy of the loop it unswitches
  on `IC5RKICK > 0.5`; the first call, before the loop, stays a call to the out-of-line
  `__w3sic5md_MOD_nr_corr`. g++ inlines none: the C++ calls one out-of-line clone of `nr_corr`
  (`.isra.0`, the same arithmetic as the function compared in NR_CORR_KOKKOS.md). Each inlined
  Fortran copy was traced symbolically on all six paths of `NR_CORR` and is the same expression
  of `K0, C1, C2, H`, with the same calls (`pow_c8_i4`, `cexp`, `cpow`) and branch predicates, as
  the out-of-line `NR_CORR`; so was the C++ clone, `K**4` mapped to its inline expansion as in
  NR_CORR_KOKKOS.md.
- ⚠ Inlining reorders operands: against the out-of-line `NR_CORR`, the inlined copies put the
  operands of 39 (no-kick copy) and 19 (kick copy) commutative products and sums in the other
  order, and the C++ has the 41 swaps listed in NR_CORR_KOKKOS.md. The result can differ only when
  both operands of such a site are NaN with different encodings; every non-NaN result is
  bit-identical.
- (v) `K1 = K0 - DK` is `k0 - dk` per part (`subsd`, same operand order) in both objects.
- (v) `ABS(DK)` is glibc `cabs` in both objects (`std::abs` of `std::complex<double>` lowers to
  it), compared with `ERRTOL` (`1.E-12`, a default `REAL`, `:144`) widened to double: the same
  constant bits (`0x3D71979980000000`) in both. A NaN `DK` ends the loop in both (`comisd`, `jbe`).
- (v) `IC5RKICK > 0.5` is a float compare with `0.5f` (`comiss`, `ja`). `ABS(REAL(K1)) < IC5KFILTER`
  compares in double, `IC5KFILTER` widened by `cvtss2sd`.
- (v) The kick: `2*TRANVAL` is `TRANVAL + TRANVAL` in float (`addss`), widened, then
  `(double(2t) + Re K1, 0.0 + Im K1)`: the `0.0 + Im` is kept in both objects (it turns `-0` into
  `+0`), with the same first source.
- (v) `ITER >= IC5MAXITER` converts `ITER` to float (`cvtsi2ss`) and compares with `comiss`; a NaN
  `IC5MAXITER` never stops the loop in either object. The test comes after the kick, as in the
  Fortran.
- ⚠ The kick path depends on libgfortran's generator seeded from `SYSTEM_CLOCK`: two runs of WW3
  itself differ there, so it is not reproducible bit for bit; the translation only takes the
  numbers from the caller.
- ⚠ The host path calls glibc `cabs` (and, through `nr_corr`, `cexp`, `cpow`). A device has none
  of them: the device path falls back to `Kokkos::abs`, `Kokkos::exp`, `Kokkos::pow`, not bit for
  bit with glibc. CUDA not compiled.

## Benchmarks

None. No timing without parity.

## How it was regtested

Not yet, and nothing was run. Its checkpoint (ADR-0011) needs a shim and `FSDISP`'s caller hunk;
the regtests that compile it are the `IC5` cases of `ww3_tic1.1`.
