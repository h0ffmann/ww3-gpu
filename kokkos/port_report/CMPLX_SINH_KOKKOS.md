# CMPLX_SINH_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Complex hyperbolic sine of a double-precision complex argument in the sea-ice module (switch
`IC5`), `(EXP(X) - EXP(-X)) * 0.5`. Its only caller is `NR_CORR` (`w3sic5md.F90:1735`, `:1736`),
the Newton-Raphson correction of the Fox-Squire dispersion relation, reached through
`W3IC5WNCG` (`:619`) → `FSDISP` → `NR_ROOT` (`:875`) → `NR_CORR` (`:1880`, `:1888`).

## What was ported

| | |
|---|---|
| WW3 routine | `CMPLX_SINH`, `model/src/w3sic5md.F90:1940-2034` at NOAA-EMC/WW3 `03adda91` (`PRIVATE`, `:131`); compiled by the `IC5` switches of `ww3_tic1.1` (`input_IC5_M1`, `_M2`, `_M3`; `git grep -lw IC5 -- '*switch*'`), by no NOAA operational switch. Its only `#ifdef` blocks are `W3_S` tracing |
| Kokkos code | [`../src/ww_kokkos/ic5_cmplx_sinh.hpp`](../src/ww_kokkos/ic5_cmplx_sinh.hpp), `ww::ic5::cmplx_sinh(x)` on `Kokkos::complex<double>` (`COMPLEX(KDPC)`, `KDPC = KIND((1.0D0, 1.0D0))`, `:143`) |
| C shim | none yet |
| Plan | issue #42, batch goal/42-port-batch-easy |

## Arithmetic

Reference: the `-O3` object of `w3sic5md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixb`: `switch_lab_shrd` with `NL4 IC5 BT9 NLS`), recompiled and checked identical to the
CMake object; C++ unit at the `openmp-release` flags, g++ 16.2, 2026-10-16.

- (v) GIMPLE and assembly of both: `cexp(x)` (glibc), sign flip of both parts by `xorpd`,
  `cexp(-x)`, `dr = e+.re - e-.re`, `di = e+.im - e-.im`, then
  `re = dr*0.5 - di*0.0`, `im = dr*0.0 + di*0.5`. Same two `cexp` calls, same subtraction order,
  same four products and the same subtract/add. g++ packs the two lanes into one `vmulpd` pair and
  a `vaddsubpd`; each lane is the same IEEE double operation as the Fortran's scalar `mulsd`,
  `subsd`, `addsd`. No FMA in either object.
- (v) The default `REAL` `0.5` is converted to `COMPLEX(8)` `(0.5, 0.0)` and multiplied as a full
  complex product, inline, without `__muldc3` (gfortran's Fortran complex rules). The `*0.0`
  terms are kept: they make an infinite part NaN, so the C++ writes them out.
- (v) The inlined copies (one in `nr_corr`, two in `nr_root` where `NR_CORR` is inlined at `:1880`
  and `:1888`) carry the same sequence as the out-of-line function. In each, gfortran computes
  `CMPLX_SINH(TKH)` once for `:1735` and `:1736` (same argument, same value), and shares its two `cexp` results with `CMPLX_COSH(TKH)`; a caller port may call
  it twice without changing a bit.
- ⚠ The host path calls glibc `cexp` through `std::exp(std::complex<double>)`, which g++ lowers to
  `__builtin_cexp` (seen in the GIMPLE). A device has no glibc `cexp`; there the code falls back to
  `Kokkos::exp` (`exp(re)*(cos(im), sin(im))`), which is not bit for bit with glibc. CUDA not
  compiled.
- `CMPLX_COSH` (`:2050-2144`) is translated separately in `ic5_cmplx_cosh.hpp` with the same
  `cexp` rule.
- (v) Fresh review 2026-10-16: no defect. `w3sic5md.F90` recompiled from `build_mixb`'s flags and found identical to the CMake object; its GIMPLE and assembly compared with a C++ unit at the `openmp-release` flags operation by operation: two glibc `cexp` calls, sign flip by `xorpd`, the two subtractions in the same order, the products by `(0.5, 0.0)` and `(0.0, 0.5)` (`.rodata` bits read) and the subtract/add, no FMA; the line cites, the `IC5` switches and the inlined copies in `nr_corr`/`nr_root` checked. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
