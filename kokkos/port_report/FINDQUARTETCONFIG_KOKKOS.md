# FINDQUARTETCONFIG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A subroutine of the generalized kinetic equation module of the `NL5` nonlinear source term
(switch `NL5`, `W3GKEMD`, Gramstad and Stiassnie 2013). It lists every quartet
(k1, k2, k3, k4) of the (k, theta) grid that `FindQuartetNumber` counted: k1, k2, k3 on the grid,
k4 = k1 + k2 - k3 inside [kmin, kmax] (or [kmin/9, 9 kmax] with `qi_interp = 1`), quasi-resonant
(|w1 + w2 - w3 - w4| / min(w) <= `oml`, or every quartet when `oml <= epsilon`), with the nearest
grid point to k4 (`MINLOC`) different from k1 and k2, not below k3, and the column index above the
row index. It stores the four indices, k4 and w4 = sqrt(g q(k4)) per quartet.

## What was ported

| | |
|---|---|
| WW3 routine | `FindQuartetConfig`, `model/src/w3gkemd.F90:732-869` at NOAA-EMC/WW3 `03adda91` (138 lines), private to `W3GKEMD`. Only caller: `PrepKernelIO` (`:1285-1287`), once when the quartet set is built, after `FindQuartetNumber` (`:1274`) gave `qi_nnz`; the object calls it out of line. Compiled by the switches of `ww3_ts1/input_nl5_gaussian`, `input_nl5_growth`, `input_nl5_matrix` (`git grep -lw NL5 -- '*switch*'`), by no NOAA operational switch. No `#ifdef` in its body: no variant |
| Kokkos code | [`../src/ww_kokkos/gke_findquartetconfig.hpp`](../src/ww_kokkos/gke_findquartetconfig.hpp), `ww::gke::findquartetconfig(ns, kx, ky, om, oml, nnz, nn, pp, qq, rr, k4x, k4y, om4, qr_depth, qi_interp, k)`, returning 0 or 1; calls `ww::gke::qfunc` (`gke_qfunc.hpp`). Index outputs keep the Fortran's 1-based values; `nnz` is `std::int64_t` (`INTEGER(8)`). One thread: `s` counts the quartets in loop order |
| Structural changes | The automatic array `k(ns)` (`:784`, `malloc`/`free` in the object) is the caller-provided work array `k[ns]`. The `WRITE` of format 1001 and `call exit(1)` (`:860-863`) are the return value 1; the shim does the write and the exit. The module variables `qr_depth` and `qi_interp` are arguments |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3gkemd.F90` (WW3's CMake Release flags, gfortran
16.2, `switch_lab_shrd` with `NL5`, the `build_nl5` reference build; recompiled object identical
to the CMake one), `__w3gkemd_MOD_findquartetconfig`, against a throwaway C++ unit with the
openmp-release flags, read statement by statement, 2026-10-16.

- (v) `k = sqrt(kx*kx + ky*ky)` (`:789`) is vectorised in both objects (4+2 lanes plus a scalar
  tail in the Fortran, 8+4 lanes in the C++) with `.SQRT`, correctly rounded in every lane, so
  every element has the same value. The Fortran adds `ky*ky + kx*kx`, the C++ `kx*kx + ky*ky`: a
  NaN payload could differ, but `k` is only compared (`MINVAL`, `MAXVAL`) and never stored.
- (v) `MINVAL(k)` and `MAXVAL(k)` (`:790-791`) as gfortran expands them: the first element with
  `k <= +Inf` (resp. `>= -Inf`), that is not NaN, starts the scan, then
  `x < lim ? x : lim` (resp. `x > lim ? x : lim`) to the end; all NaN gives NaN. The C++ writes
  the same scan and select; its GIMPLE has the same compares, selects and NaN phi.
- (v) `qi_bound` is the PARAMETER 1 (`:184`), so `:794` tests only `qi_interp == 1`; `kmin / 9.`
  is a `divss` (not a reciprocal multiply) and `kmax * 9.` a `mulss` in both.
- (v) k4 (`:815-816`): `(kx1 + kx2) - kx3` in that order; `k4T = sqrt(k4x*k4x + k4y*k4y)`
  (`:817`), same order in both. `:820` is two ordered compares in both.
- (v) `QFunc(k4xT, k4yT)` is inlined in both objects and its `sqrt` is the same value as `k4T`
  (CSE in both); `tanhf` from glibc on the host, as the Fortran object calls it; `qr_grav * q`
  is `q * 9.806` (`0x411ce560`), then `sqrtf` (`:822`). ⚠ The C++ keeps a `sqrtf` call for a
  negative argument (errno, `-fmath-errno`), where the Fortran has `sqrtss` only; glibc's
  x86-64 `sqrtf` is `sqrtss`, so the bits are the same; `q >= 0` for every finite input anyway.
- (v) `MIN(om(i1), om(i2), om(i3), om4T)` (`:824-825`): the Fortran object pairs it as
  `minss` on `(om1, om2)`, `(om3, om4T)`, then `(m34, m12)`, each keeping the first operand
  unless the second is strictly smaller (the AT&T destination rule). The C++ writes those three
  selects explicitly; its GIMPLE has the same three `cond ? a : b` with the same operand roles.
  Numerator `|((om1 + om2) - om3) - om4T|`, `andps` then `divss`, same in both.
- (v) Both objects compute `dom` only when `oml > epsilon(100.0)` (`0x34000000`); the
  condition is `oml >= dom` in both (`:826`).
- (v) `MINLOC((kx - k4xT)*(kx - k4xT) + (ky - k4yT)*(ky - k4yT), 1)` (`:828-829`): the first
  element that is not NaN starts the scan, `d < lim` keeps the first minimum, all NaN gives 1.
  The Fortran's `PAREN_EXPR` around each difference changes nothing (the difference is squared
  as one value in both). The two sums are written `dx2 + dy2` in the first loop and `dy2 + dx2`
  in the second in both GIMPLE dumps; the value is only compared.
- (v) Integer indices `row = i1 + ns*(i2-1)`, `col = i3 + ns*(i4-1)` (`:833-834`) in 32-bit
  `int` in both; the seven stores (`:840-847`) at `s - 1`; `s` widened to 64 bits for the
  compare with `nnz` (`:860`).
- (v) Host compile warning-free; CUDA compiled (`nvcc_wrapper`, sm_89, `--fmad=false`), compile
  only. ⚠ On a device `Kokkos::sqrt` is IEEE, but `tanh` (through `qfunc`) is not shown to
  return glibc's bits.
- ⚠ The output arrays are written at `s` with no bound against `nnz` (`:840-847`); only after
  the loops does `:860` compare. A count that exceeds `nnz` writes past the end in the Fortran
  and in the C++ alike. `FindQuartetNumber` (`:522-728`) applies the same criteria, so the counts
  agree when both are compiled the same way; nothing guards it.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
