# COOCSRIND_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A subroutine of the generalized kinetic equation module of the `NL5` nonlinear source term
(switch `NL5`, `W3GKEMD`, Gramstad and Stiassnie 2013), adapted from `coocsr` of SPARSKIT
(Saad 1994). It converts the row indices of a sparse matrix of quartet interactions from
coordinate (COO) to compressed sparse row (CSR) storage without touching values: it returns
`ind_translate`, with `a_csr = a_coo(ind_translate)`, and the row pointer `iao`.

## What was ported

| | |
|---|---|
| WW3 routine | `CooCsrInd`, `model/src/w3gkemd.F90:874-979` at NOAA-EMC/WW3 `03adda91` (106 lines), private to `W3GKEMD`. Only caller: `PrepKernelIO` (`:1334`, `call CooCsrInd(qi_nrsm, qi_nnz, irow_coo, qi_icCos, icooTcsr, qi_irCsr)`), once when the quartet set is built. Compiled by the switches of `ww3_ts1/input_nl5_gaussian`, `input_nl5_growth`, `input_nl5_matrix` (`git grep -lw NL5 -- '*switch*'`), by no NOAA operational switch. No `#ifdef` inside it |
| Kokkos code | [`../src/ww_kokkos/gke_coocsrind.hpp`](../src/ww_kokkos/gke_coocsrind.hpp), `ww::gke::coocsrind(nrow, nnz, ir, jc, ind_translate, iao)`; `ir`, `ind_translate` and `iao` keep the Fortran's 1-based index values; `nnz` is `std::int64_t` (`INTEGER(8)`, `:209`); one thread, since the count and the fill scatter into `iao(ir(k))` in `k` order |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3gkemd.F90` (WW3's CMake Release flags, gfortran
16.2, `switch_lab_shrd` with `NL5`, the `build_nl5` reference build; recompiled object identical
to the CMake one), `__w3gkemd_MOD_coocsrind`, against a throwaway C++ unit with the
openmp-release flags, read statement by statement, 2026-10-16.

- (v) Integer only: no real arithmetic, no libm call, no MIN/MAX, no conversion of a real.
- (v) gfortran converts the `INTEGER(8)` limit `nnz` to `INTEGER(4)`, the kind of `k`, once
  (`_130 = (integer(kind=4)) nnz`, `:942`) and both `k` loops (`:942`, `:957`) run to that value.
  The C++ does the same with `static_cast<int>(nnz)` (modular in C++20). ⚠ With
  `qi_nnz > 2^31-1` both truncate the same way and the result is wrong in both; `ind_translate`
  is `INTEGER(4)` anyway.
- (v) `iao(1:nrow+1) = 0` is a `memset` guarded by `nrow+1 > 0` in both objects. The count loop,
  the prefix loop (`k0`, `iao(j) = k`, `k = k + k0`) and the fill loop have the same loads and
  stores in the same order in both GIMPLE dumps.
- (v) `j = jc(k)` (`:959`) is dead in the Fortran object (only a debug bind); the C++ never reads
  `jc`.
- (v) The shift loop `iao(j+1) = iao(j)`, `j = nrow..1` (`:972-974`) is scalar in the Fortran
  object and vectorised by g++ (alias-versioned, overlapping backward copy). An integer copy, so
  the stored values are the same; no `GCC novector` needed.
- (v) CUDA compiled (`nvcc_wrapper`, sm_89, `--fmad=false`), compile only, no floating-point
  instruction in the SASS. Compiled warning-free on the host.
- The Fortran comment at `:965` says `iao(nrow+1)` stays `1 + nnz`; the shift at `:973` with
  `j = nrow` overwrites it with `iao(nrow)`, which after the fill equals `1 + nnz`, so the
  final row pointer is right. Reproduced as written.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3gkemd.F90` from `build_nl5`'s flags (object identical to the CMake one) and a new C++ unit with the openmp-release flags (warning-free); compared the GIMPLE of `__w3gkemd_MOD_coocsrind` with the C++ statement by statement: the `INTEGER(8)` to `INTEGER(4)` limit conversion, the `memset` guard, the count, prefix and fill loops (same loads and stores, same order, scalar in both), and the shift loop (scalar in the Fortran, vectorised by g++ as a backward integer copy that loads before it stores, so the same values). Argument order and 1-based index values match the dummies; the citations `:209`, `:959`, `:965`, `:1334` and the three `NL5` switches were read at `03adda91`. Nothing changed. The CUDA bullet is the translator's and was not repeated.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
