# TFUNC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The kernel function of the generalized kinetic equation module of the `NL5` nonlinear source
term (switch `NL5`, `W3GKEMD`, Gramstad and Stiassnie 2013): the four-wave interaction
coefficient `T_{1,2,3,4}` of Janssen (2009) (`Ṽ^{(2)}_{0,1,2,3}` of Krasitskii 1994, up to
`(1/2π)²`), for quartets with `n0 ≠ n2` and `n0 ≠ n3`,

`T = W - L14 - L23 - L56`,

where `W` is the sum of six `UFunc` terms (direct interaction), `L14` and `L23` are products of
two `VmFunc` with `1/(ω_a + ω_{b-c} - ω_d)` sums (virtual states from differences), and `L56`
the `VmFunc`/`VpFunc` products with `1/(ω_{0+1} ∓ ω_0 ∓ ω_1)` (virtual states from sums), with
`ω = √(g q)` and `q = k tanh(kd)` from `QFunc`. `L56` and `ω_{0+1}`, `ω_{2+3}` stay 0 when
`|k0x+k1x| ≤ ε` and `|k0y+k1y| ≤ ε` (`ε = EPSILON(100.0)`), where `ω_{0+1} = 0` would divide by 0.

## What was ported

| | |
|---|---|
| WW3 routine | `TFunc`, `model/src/w3gkemd.F90:400-517` at NOAA-EMC/WW3 `03adda91` (118 lines), private to `W3GKEMD`. Only caller: `PrepKernelIO` (`:1295`, once per quartet; one out-of-line call in the object). Compiled by the switches of `ww3_ts1/input_nl5_gaussian`, `input_nl5_growth`, `input_nl5_matrix` (`git grep -lw NL5 -- '*switch*'`), by no NOAA operational switch. No `#ifdef` inside it |
| Kokkos code | [`../src/ww_kokkos/gke_tfunc.hpp`](../src/ww_kokkos/gke_tfunc.hpp), `ww::gke::tfunc(k0x, k0y, k1x, k1y, k2x, k2y, k3x, k3y, qr_depth)`; calls `qfunc` ([QFUNC_KOKKOS.md](QFUNC_KOKKOS.md)) ten times, `ufunc` ([UFUNC_KOKKOS.md](UFUNC_KOKKOS.md)) six times, `vmfunc` ([VMFUNC_KOKKOS.md](VMFUNC_KOKKOS.md)) ten times and `vpfunc` ([VPFUNC_KOKKOS.md](VPFUNC_KOKKOS.md)) twice; the module variable `qr_depth` (`:165`) is an argument passed on to each |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3gkemd.F90` (WW3's CMake Release flags, gfortran
16.2, `switch_lab_shrd` with `NL5`, the `build_nl5` reference build; recompiled object identical
to the CMake one), out-of-line `__w3gkemd_MOD_tfunc`, against a throwaway C++ unit with the
openmp-release flags, 2026-10-16. The C++ unit was compiled twice: as is (everything inlined)
and with `--param max-inline-insns-single=40 --param max-inline-insns-auto=0`, which keeps
`ufunc`, `vmfunc`, `vpfunc` out of line as the Fortran object does, so that `tfunc`'s own body
can be compared.

- (v) Static symbolic comparison of the two GIMPLEs: a throwaway script walked each function's
  control-flow graph for all eight outcomes of the three tests (depth in `(ε, 3000)`,
  `|k0x+k1x| > ε`, `|k0y+k1y| > ε`) and built the expression of the returned value. On all
  eight paths the Fortran and the C++ expressions are the same tree of operations: the same
  ten `tanhf`, the same `sqrt`, the same operands and argument lists of the six `ufunc`, ten
  `vmfunc` and two `vpfunc` calls (`-k` a negation, `k3x-k1x` computed as written, `-k0x-k1x`
  as `(-k0x)-k1x`), the same left-to-right `W` sum, the same `1.0/(a+b-c)` sums and
  `W - L14 - L23 - L56`. The only differences are swapped operands of a commutative `+` or `*`
  (`x²+y²` against `y²+x²` in the inlined `QFunc`, `q*g` against `g*q`, `om2+om02` against
  `om02+om2`), some of which gfortran itself writes swapped from the source.
- ⚠ A swapped `+` or `*` gives the same bits for every operand pair except two NaNs with
  different payloads or signs, where `addss`/`mulss` return the first. The inputs are finite
  wavenumbers; a NaN only arises from `0/0` in `vmfunc`/`vpfunc` (a zero wavenumber), where
  both NaNs are the default one.
- (v) The fully inlined C++ unit folds `(-a)+b` from the negated arguments of `ufunc` into
  `b-a` and `(-a)*(-a)` into `a*a`; both are exact for finite `a`, `b` (the zero signs agree),
  and differ only in a NaN's sign bit.
- (v) Instruction set: no FMA, no packed arithmetic, no libmvec in either object (scalar `tanhf`
  only). The C++ calls `sqrtf` on the errno path of a negative or NaN argument
  (the Fortran is `-fno-math-errno`): glibc's `sqrtf` returns the NaN `sqrtss` returns. No
  reduction, no MIN/MAX. Compiled warning-free.
- ⚠ The tests at `:464` and `:500` are ordered compares: a NaN `k0x+k1x` and `k0y+k1y` leave
  `om0p1 = om2p3 = L56 = 0` in both. If both sums are within `ε` of 0 but not 0, `L56` is
  dropped by design of the Fortran; reproduced, not changed.
- ⚠ CUDA compiled only (nvcc_wrapper, `sm_89`, `--fmad=false`, 2026-10-16), never run.
  `gke_vmfunc.hpp` now guards its `static_assert` on `__builtin_sqrtf` with
  `!defined(__CUDACC__)` as `gke_vpfunc.hpp` does; the host objects of the C++ unit are
  byte-identical before and after. The SASS still holds `FFMA` from CUDA's `tanhf`, square root
  and IEEE division sequences, so device bits depend on CUDA's math library, as for every
  `W3GKEMD` function.
- (v) Fresh review 2026-10-16: no defect in `tfunc`. `w3gkemd.F90` recompiled from
  `build_nl5`'s flags (object identical to the CMake one); the optimised GIMPLE of
  `__w3gkemd_MOD_tfunc` read against the out-of-line C++ `ww::gke::tfunc` statement by
  statement (the ten `ω`, both `ε` tests, the six `ufunc`, ten `vmfunc`, two `vpfunc` argument
  lists, every `1.0/(a±b∓c)` and the final subtraction order agree up to commuted `+`/`*`
  operands); no FMA, packed or libmvec instruction in either object. Fixed: the unguarded
  `static_assert` of `gke_vmfunc.hpp` that stopped the CUDA compile.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
