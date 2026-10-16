# UFUNC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A function of the generalized kinetic equation module of the `NL5` nonlinear source term
(switch `NL5`, `W3GKEMD`, Gramstad and Stiassnie 2013): the intermediate quantity
`U_{1,2,3,4}` of Janssen (2009) (`V_{0,1,2,3}` of Krasitskii 1994, up to `(1/2π)²`),

`U = (1/16) (q₂q₃ / (q₀q₁))^{1/4} [2(|k₀|² q₁ + |k₁|² q₀) - q₀q₁ (q(k₀+k₂) + q(k₁+k₂) + q(k₀+k₃) + q(k₁+k₃))]`,

with `q = k tanh(kd)` from `QFunc`. `TFunc` sums six of them into the third-order coefficient
`W^{(2)}_{1,2,3,4}` (`:473-478`).

## What was ported

| | |
|---|---|
| WW3 routine | `UFunc`, `model/src/w3gkemd.F90:358-396` at NOAA-EMC/WW3 `03adda91` (39 lines), private to `W3GKEMD`. Only caller: `TFunc` (`:473-478`, six calls, out of line in the object). Compiled by the switches of `ww3_ts1/input_nl5_gaussian`, `input_nl5_growth`, `input_nl5_matrix` (`git grep -lw NL5 -- '*switch*'`), by no NOAA operational switch. No `#ifdef` inside it |
| Kokkos code | [`../src/ww_kokkos/gke_ufunc.hpp`](../src/ww_kokkos/gke_ufunc.hpp), `ww::gke::ufunc(k0x, k0y, k1x, k1y, k2x, k2y, k3x, k3y, qr_depth)`; calls `ww::gke::qfunc` ([`gke_qfunc.hpp`](../src/ww_kokkos/gke_qfunc.hpp), [QFUNC_KOKKOS.md](QFUNC_KOKKOS.md)) eight times; the module variable `qr_depth` (`:165`) is an argument |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3gkemd.F90` (WW3's CMake Release flags, gfortran
16.2, `switch_lab_shrd` with `NL5`, the `build_nl5` reference build; recompiled object identical
to the CMake one), out-of-line `__w3gkemd_MOD_ufunc`, against a throwaway C++ unit with the
openmp-release flags, read operation by operation, 2026-10-16.

- (v) The eight inlined copies of `QFunc` in `UFunc` are QFunc's sequence (`kx*kx + ky*ky`,
  `sqrtss`, ordered compares with `qr_eps` and `qr_dmax`, `q*d`, `tanhf`, `q*tanh`), with the
  depth test shared by the first four and by the last four in both objects; some adds and
  products are written commuted in one object and not the other (`ky²+kx²`, `d*q`), same bits.
- (v) The body: `q2*q3`, `q0*q1`, divide, `sqrt`, `sqrt`, `*0.0625`; `|k0|²*q1 + |k1|²*q0`
  (the squares reused from the `QFunc` copies), `*2.0`; the four shifted `q` summed left to
  right `((q02+q12)+q03)+q13`; `(q0*q1)*sum`; subtract; multiply. Same in both GIMPLEs.
  Counts in both objects: 39 `mulss`, 23 `addss`, 10 `sqrtss`, 1 `divss`, 1 `subss`, 8 `tanhf`,
  no FMA. `.rodata` words: `0x34000000` (`qr_eps`), `0x453b8000` (3000), `0x3d800000` (1/16).
- (v) The C++ object adds a `sqrtf` call on the path where `q2*q3/(q0*q1)` is negative or NaN
  (errno; the Fortran has `-fno-math-errno`): glibc's `sqrtf` returns the same NaN `sqrtss` does,
  so the same bits. No reduction, no MIN/MAX, no libmvec. Compiled warning-free.
- ⚠ CUDA compiled (`nvcc_wrapper`, sm_89, `--fmad=false`), compile only. Its SASS has `FFMA`,
  `MUFU.RSQ`, `MUFU.RCP` and `MUFU.EX2` from CUDA's `tanhf`, `sqrtf` and divide: not shown to
  give glibc's bits.
- ⚠ If any `q` is 0 (a zero wavenumber vector), the divide gives Inf or NaN in both; the
  translation follows the Fortran and does not guard it.
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3gkemd.F90` from `build_nl5`'s
  flags (object identical to the CMake one) and compared its `__w3gkemd_MOD_ufunc` GIMPLE and
  assembly with a throwaway C++ unit, operation by operation: the eight `QFunc` copies (squares,
  `sqrtss`, both ordered depth tests, `q*d`, `tanhf`, `q*tanh`), the operands of `q2*q3/(q0*q1)`,
  `|k0|²q1 + |k1|²q0`, the shifted sums `((q02+q12)+q03)+q13`, and the instruction counts above.
  Fixed only a comment: `TFunc` calls it at `:473-478`, not `:473-480`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
