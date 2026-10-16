# VMFUNC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The second-order interaction coefficient `V^(-)_{0,1,2}` of Janssen (2009), which is
`U^(1)_{0,1,2}` of Krasitskii (1994) up to a factor `1/(2π)`, in the generalized kinetic
equation module of the `NL5` nonlinear source term (switch `NL5`, `W3GKEMD`):

`V^(-) = sqrt(1/32) [ (k0·k1 − q0 q1) (g q2/(q0 q1))^(1/4) + (k0·k2 − q0 q2) (g q1/(q0 q2))^(1/4) + (k1·k2 + q1 q2) (g q0/(q1 q2))^(1/4) ]`,

with `q = k tanh(kd)` from `QFunc` and `g = 9.806` m/s² (`qr_grav`).

## What was ported

| | |
|---|---|
| WW3 routine | `VmFunc`, `model/src/w3gkemd.F90:319-354` at NOAA-EMC/WW3 `03adda91` (36 lines), private to `W3GKEMD`. Called only by `TFunc` (`:482-486`, `:491-495`, `:503-504`), out of line: 10 calls in the GIMPLE, no inlined copy. Compiled by the switches of `ww3_ts1/input_nl5_gaussian`, `input_nl5_growth`, `input_nl5_matrix` (`git grep -lw NL5 -- '*switch*'`), by no NOAA operational switch. No `#ifdef` inside it |
| Kokkos code | [`../src/ww_kokkos/gke_vmfunc.hpp`](../src/ww_kokkos/gke_vmfunc.hpp), `ww::gke::vmfunc(k0x, k0y, k1x, k1y, k2x, k2y, qr_depth)`; calls `ww::gke::qfunc` ([`QFUNC_KOKKOS.md`](QFUNC_KOKKOS.md)). The module variable `qr_depth` (`:165`) is an argument |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3gkemd.F90` (WW3's CMake Release flags, gfortran
16.2, `switch_lab_shrd` with `NL5`, object identical to the CMake build's), against a throwaway
C++ unit with the openmp-release flags, read operation by operation, 2026-10-16.

- (v) The three copies of `QFunc` inlined into `__w3gkemd_MOD_vmfunc` match the C++: `qr_depth`
  loaded once, `kx*kx + ky*ky` (for `k2` the object adds `ky²` first, commutative), `sqrtss`;
  ordered `comiss` with `0x34000000` and `0x453b8000`; then three `q*d`, `tanhf`, `q*tanh`.
- (v) The body (`:347-350`) is the same sequence in both objects: each term `a*b + c*d`, then
  `∓ qa*qb`; `q*9.806` (`0x411ce560`) divided by `qa*qb`; two `sqrtss`; the product; the three
  terms summed left to right; times `sqrt(1.0/32.0)`, which gfortran folds to `0x3e3504f3`
  (`kSqrt1o32`, `static_assert` against `__builtin_sqrtf(1.0f/32.0f)`). Operand swaps are only of
  commutative multiplications. No FMA (contraction off).
- (v) libm: scalar `tanhf` only (3 calls). The C++ object, built with math-errno as the
  openmp-release flags leave it, adds a guard before the first `sqrt` of each term and calls libm
  `sqrtf` when the ratio is negative; gfortran (no errno) uses `sqrtss` throughout. The ratio is
  never negative for real `q ≥ 0`; for NaN the guard takes `sqrtss`. ⚠ The negative path (not
  reachable from finite inputs) relies on glibc's `sqrtf` returning the `sqrtss` NaN; not read.
- (v) The Fortran parentheses appear as `PAREN_EXPR` around each term's dot product, each
  denominator and the sum; g++ folded nothing across them (same operations in the C++ GIMPLE), so
  no `__builtin_assoc_barrier` is needed.
- (v) No reduction, no MIN/MAX, no `#ifdef` variant. Compiled warning-free.
- ⚠ Not compiled for CUDA in this batch. On a device `qfunc`'s `tanh` is CUDA's, not glibc's
  (`QFUNC_KOKKOS.md`).
- ⚠ A zero wavenumber argument gives NaN in the Fortran and the C++ alike: `k0 = k2` at `:482`
  makes the third argument `0`, `q2 = 0`, and the third term is `0 * sqrt(sqrt(g q0/0)) = 0 * inf`.
  `TFunc` guards only the `L56` case (`:500`). Reproduced, not guarded.
- (v) Fresh review 2026-10-16: no arithmetic defect. `w3gkemd.F90` recompiled under `switch_nl5` (object identical
  to the CMake build's) and a throwaway C++ unit, `__w3gkemd_MOD_vmfunc` read against it instruction by
  instruction: the three inlined `QFunc` copies (squares, add, `sqrtss`, ordered `comiss` on `0x34000000` and
  `0x453b8000`, `q*d`, `tanhf`, `q*tanh`), the three terms, the divisions, the double `sqrtss`, the left-to-right
  sum and the `.rodata` bits (`0x411ce560`, `0x3e3504f3`) agree up to commutative operand swaps. Fixed line
  cites only: the body is `:347-350` (was `:350-353`), and the header's callers now list `:503-504`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
