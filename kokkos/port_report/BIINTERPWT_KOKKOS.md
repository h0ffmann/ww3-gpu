# BIINTERPWT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A subroutine of the generalized kinetic equation module of the `NL5` nonlinear source term
(switch `NL5`, `W3GKEMD`, Gramstad and Stiassnie 2013). For each quartet it locates the fourth
wavenumber `k4 = (qr_k4x, qr_k4y)` on the model's (k, theta) grid and stores the four
surrounding spectral indices (`qi_bind`) and bilinear weights (`qr_bwgh`); with the tail
extension (`qi_bound = 1`) it also stores `qr_bdry`, 0 below the lowest wavenumber and
`(k4/kmax)**(qr_fpow/2 - 2)` above the highest (van Vledder 2006, Fig. 9).

## What was ported

| | |
|---|---|
| WW3 routine | `BiInterpWT`, `model/src/w3gkemd.F90:1483-1618` at NOAA-EMC/WW3 `03adda91` (136 lines), private to `W3GKEMD`. Only caller: `PrepKernelIO` (`:1317`, `call BiInterpWT(nk, nth, qr_wn1, th)`) when `qi_interp = 1`, once when the quartet set is built, out of line in the object. Compiled by the switches of `ww3_ts1/input_nl5_gaussian`, `input_nl5_growth`, `input_nl5_matrix` (`git grep -lw NL5 -- '*switch*'`), by no NOAA operational switch. No `#ifdef` inside it |
| Variants | `qi_bound` is a `PARAMETER` equal to 1 (`:184`), so the `qi_bound = 0` branches (`:1513`, `:1535-1539`, `:1608`) are dead in the object and not translated; `qr_kpow = qr_fpow/2 - 2` is the folded constant -4.5 (`qr_fpow = -5.`, `:188`) |
| Kokkos code | [`../src/ww_kokkos/gke_biinterpwt.hpp`](../src/ww_kokkos/gke_biinterpwt.hpp), `ww::gke::biinterpwt(nk, nth, wn, th, qi_nnz, qr_k4x, qr_k4y, qi_bind, qr_bwgh, qr_bdry)`; arrays 0-based; `qi_bind` and `qr_bwgh` are the column-major `(4, qi_nnz)` arrays, `qi_bind` holds the Fortran's 1-based spectral indices; `qi_nnz` is `std::int64_t` (`INTEGER(8)`, `:209`); one thread. Reuses `kQrTpi` of `gke_prepkgrid.hpp` and `ww::st4::fint` |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3gkemd.F90` (WW3's CMake Release flags, gfortran
16.2, `switch_lab_shrd` with `NL5`, the `build_nl5` reference build; recompiled object identical
to the CMake one), `__w3gkemd_MOD_biinterpwt`, against a throwaway C++ unit with the
openmp-release flags (warning-free), read operation by operation, 2026-10-16.

- (v) Initialisation (`:1511-1513`): `qi_bind = 0` and `qr_bwgh = 0.` are `memset` calls in both
  objects over the allocated `4*qi_nnz` elements; `qr_bdry = 1.` is a vectorised fill in both
  (4 lanes in the Fortran, 8 in the C++), the same stored values.
- (v) `MINVAL(wn)` and `MAXVAL(wn)` (`:1522-1523`) as gfortran inlines them: a scan for the first
  element with `wn <= +Inf` (`>= -Inf` for MAXVAL), then `w < lim ? w : lim` (`w > lim ? w : lim`);
  all NaN gives NaN, `nk <= 0` gives `HUGE` (`-HUGE`). The C++ `minval`/`maxval` reproduce the
  scan, and the inner `vminss`/`vmaxss` keep the new element as first operand, as `minss`/`maxss`
  in the Fortran object.
- (v) `k4R = sqrt(qr_k4x**2. + qr_k4y**2.)` (`:1532`): `**2.` is a multiply in the object
  (`powmult`), and `addss` adds `ky*ky + kx*kx` with `ky*ky` first; the C++ writes that order and
  `vaddss` has the same first operand. `sqrtss` in both.
- (v) `atan2f(ky, kx)` (`:1533`) and `powf(k4R/kmax, -4.5)` (`:1612`) are glibc calls in both
  objects; no libmvec call.
- (v) `MAX(kmin, MIN(k4R, kmax))` (`:1536`): the Fortran object computes `kmax < k4R ? kmax : k4R`,
  then `m > kmin ? m : kmin` (`minss`/`maxss` destination rule), so a NaN `k4R` gives `kmin`. The
  C++ writes those two ternaries and `vminss`/`vmaxss` have the same operands in the same order.
- (v) `dth = qr_tpi / REAL(nth)` with `qr_tpi = 0x40c90fdb`; `r_jth = (angR - aRef)/dth + 1.`
  and `r_jk = (REAL(jkU) - 1.) + (k4T - wn(jkU-1))/delK` (`:1557`, `:1582`): same operations,
  same first operands of `addss` in both objects.
- (v) `FLOOR` (`:1558`, `:1585`) is inlined by gfortran as `cvttss2si`, then `REAL(i) <= x ? i : i-1`.
  The C++ `ffloor` uses `fint` (INT_MIN for NaN or out of range, without a UB cast) and a
  wrapping decrement: a NaN gives `INT_MAX`, as the object's `sub $1`. g++ splits the out-of-range
  cases into branches; for `x >= 2^31` it adds `2^31` where the Fortran subtracts `REAL(INT_MIN)`,
  the same value.
- (v) `MOD(jth4T-1+nth, nth)` and `MOD(jth4T+nth, nth)` (`:1561-1562`) are 32-bit adds then
  `idiv` in both objects; the index sums of `:1590-1593` and `MIN(jk4+1, nk)` (`:1587`) are 32-bit
  integer arithmetic. The C++ does them with wrapping helpers, so a NaN angle or wavenumber
  (`FLOOR` = `INT_MAX`) gives the object's wrapped indices without undefined behaviour.
- (v) The `do while (k4T > wn(jkU))` search (`:1573-1576`) compares with `comiss`/`ja` in both and
  exits on `jkU > nk` before reading `wn(nk+1)`. For `jkU = 1` both objects fold `r_jk = 1`,
  `w_k4 = 0`, `1 - w_k4 = 1`, and `qr_bwgh(4) = w_th4` without the multiply by 1.
- ⚠ Weight products (`:1596-1599`): `qr_bwgh(2)` and `qr_bwgh(4)` have the same `mulss` first
  operand in both objects. For `qr_bwgh(1)` the Fortran object has `(1-w_k4)` first and g++
  `(1-w_th4)`; for `qr_bwgh(3)` the Fortran has `w_k4` first and g++ `w_th4`, whichever order the
  C++ source writes, and `__builtin_assoc_barrier` does not steer it. The products are equal for
  every input except when both factors are NaN with different payloads, where the first operand's
  payload is returned. The four values are packed with `unpcklps`/`movlhps` and stored as one
  vector in both.
- (v) `k4R < kmin` and `k4R > kmax` (`:1609`, `:1611`) are ordered `comiss` in both: a NaN `k4R`
  leaves `qr_bdry = 1`.
- (v) The `iq` loop runs to `INT(qi_nnz, 4)` in both (`(integer(kind=4)) qi_nnz`, `:1531`). ⚠ With
  `qi_nnz > 2^31-1` both truncate the same way and the result is wrong in both.
- ⚠ CUDA compiled (`nvcc_wrapper`, sm_89, `--fmad=false`), compile only, never run. The SASS
  contains `FFMA`, which comes from CUDA's `atan2f`, `powf` and division sequences (libdevice),
  not from the C++; CUDA's math library is not shown to return glibc's bits.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
