# VPFUNC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The second-order coefficient `V^{(+)}_{1,2,3}` of the Zakharov kernel in the generalized
kinetic equation module of the `NL5` nonlinear source term (switch `NL5`, `W3GKEMD`, Gramstad
and Stiassnie 2013), as written in Janssen (2009), appendix A:

`V⁺ = √(1/32) [ (k₀·k₁ + q₀q₁) (g q₂/(q₀q₁))^{1/4} + (k₀·k₂ + q₀q₂) (g q₁/(q₀q₂))^{1/4} + (k₁·k₂ + q₁q₂) (g q₀/(q₁q₂))^{1/4} ]`,

with `q = k tanh(kd)` from `QFunc` and `g = 9.806 m s⁻²` (the module's `qr_grav`).

## What was ported

| | |
|---|---|
| WW3 routine | `VpFunc`, `model/src/w3gkemd.F90:275-315` at NOAA-EMC/WW3 `03adda91` (41 lines), private to `W3GKEMD`. Its only caller is `TFunc` (`:506-507`), which calls it out of line (two calls to `__w3gkemd_MOD_vpfunc` in the object); none outside the module. Compiled by the switches of `ww3_ts1/input_nl5_gaussian`, `input_nl5_growth`, `input_nl5_matrix` (`git grep -lw NL5 -- '*switch*'`), by no NOAA operational switch. No `#ifdef` inside it |
| Kokkos code | [`../src/ww_kokkos/gke_vpfunc.hpp`](../src/ww_kokkos/gke_vpfunc.hpp), `ww::gke::vpfunc(k0x, k0y, k1x, k1y, k2x, k2y, qr_depth)`; calls `ww::gke::qfunc` ([`QFUNC_KOKKOS.md`](QFUNC_KOKKOS.md)); the module variable `qr_depth` (`:165`) is an argument. `kQrGrav` is `qr_grav` (`:198`), `kSqrt1o32` the folded `SQRT(1.0/32.0)` (`:309`) |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3gkemd.F90` (WW3's CMake Release flags, gfortran
16.2, `switch_lab_shrd` with `NL5`, recompiled object identical to the CMake build's), against a
throwaway C++ unit with the openmp-release flags, read operation by operation, 2026-10-16.

- (v) The three inlined `QFunc` copies match `qfunc`: `kx*kx + ky*ky` (for `q2` both objects add
  `ky² + kx²`, commutative), `sqrtss`; one ordered `comiss` pair on `qr_depth` against
  `0x34000000` and `0x453b8000` guards all three (the same test, hoisted by both compilers);
  then `q*d` or `d*q` (commutative), scalar `tanhf`, `q*tanh`. A NaN depth takes the deep-water
  branch in both.
- (v) Each term: `(kₐx*k_bx + kₐy*k_by) + qₐq_b`, then `(g*q_c)/(qₐq_b)`, `sqrtss`, `sqrtss`,
  times the term; the sum left to right, `(t01 + t02) + t12`; times `0x3e3504f3` last. Both
  objects reuse `qₐq_b` for the sum and the divisor. The constants are the same `.rodata` words:
  `0x34000000`, `0x453b8000`, `0x411ce560` (9.806), `0x3e3504f3` (`SQRT(1.0/32.0)`, checked by a
  `static_assert` on `__builtin_sqrtf`).
- (v) The Fortran's parentheses (`PAREN_EXPR` around each term sum, each `qₐq_b` and the whole
  bracket) are kept: g++ reassociates none of them, so no `__builtin_assoc_barrier` is needed.
- (v) libm: scalar `tanhf` only, three calls in both. No reduction, no MIN/MAX, no FMA, no
  reciprocal. Compiled warning-free.
- ⚠ g++ keeps `errno` for `sqrtf`: the first `sqrt` of each term branches to a `sqrtf` call
  when its argument is below zero, where gfortran (`-fno-math-errno`) only has `sqrtss`. The
  argument is `≥ 0` or NaN for any finite wavenumbers; on a negative argument glibc's `sqrtf`
  returns the default NaN as `sqrtss` does. Same bits by argument, not by identical code.
- (v) CUDA compiled (`nvcc_wrapper`, sm_89, `--fmad=false`), compile only. ⚠ The SASS has
  `FFMA` and `MUFU.RSQ`/`MUFU.RCP`/`MUFU.EX2` from CUDA's `tanhf`, `sqrtf` and division: not
  shown to give glibc's or SSE's bits.
- (v) Fresh review 2026-10-16: no defect. The `w3gkemd.F90` object was recompiled from `build_nl5`'s flags
  (identical to the CMake object) and the C++ unit rebuilt; GIMPLE and assembly of `vpfunc` and the three
  inlined `QFunc` copies read operation by operation: same multiplies, adds, divides and `sqrtss` per
  term, one hoisted `qr_depth` test, three scalar `tanhf`, the same four `.rodata` words, parentheses
  kept. Operand order differs only in commuted multiplies and adds (`k1x*k0x`, `tanh*q`), which can
  change bits only when both operands are NaN with different payloads ⚠. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
