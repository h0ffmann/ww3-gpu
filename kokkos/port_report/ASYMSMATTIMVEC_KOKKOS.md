# ASYMSMATTIMVEC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A subroutine of the generalized kinetic equation module of the `NL5` nonlinear source term
(switch `NL5`, `W3GKEMD`, Gramstad and Stiassnie 2013): the product `y = (A + s Aᵀ) x` of an
upper triangular sparse matrix `A` in compressed sparse row (CSR) storage with a vector `x`,
with `s = Symb`. Its only caller passes `s = -1`, the antisymmetric case that expresses
`dN₁/dt = -dN₃/dt` of the four-wave interaction.

## What was ported

| | |
|---|---|
| WW3 routine | `ASymSmatTimVec`, `model/src/w3gkemd.F90:983-1061` at NOAA-EMC/WW3 `03adda91` (79 lines), private to `W3GKEMD`. Only caller: `CalcQRSNL` (`:1872`, `Symb = -1.0`; the `Symb = 1.0` call at `:1901` is commented out). Compiled by the switches of `ww3_ts1/input_nl5_gaussian`, `input_nl5_growth`, `input_nl5_matrix` (`git grep -lw NL5 -- '*switch*'`), by no NOAA operational switch. No `#ifdef` inside it |
| Kokkos code | [`../src/ww_kokkos/gke_asymsmattimvec.hpp`](../src/ww_kokkos/gke_asymsmattimvec.hpp), `ww::gke::asymsmattimvec(n, a, ja, ia, x, y, symb)`; `ja` and `ia` keep the Fortran's 1-based index values; one thread, since the transposed product scatters into `y(ja(k))` |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3gkemd.F90` (WW3's CMake Release flags, gfortran
16.2, `switch_lab_shrd` with `NL5`, the `build_nl5` reference build; recompiled object identical
to the CMake one), `__w3gkemd_MOD_asymsmattimvec`, against a throwaway C++ unit with the
openmp-release flags, read operation by operation, 2026-10-16.

- (v) No vectorisation of either loop in the Fortran object (`-fopt-info-vec` reports nothing
  for `:1043-1056`; the body is scalar `mulss`/`addss`): the row sum `t` is accumulated
  sequentially in `k`, with no partial sums, and each `y(ja(k))` is loaded, added and stored per
  `k`. The C++ object does the same (no vectorised loop reported for it).
- (v) Per `k`, both objects: `t = t + a(k)*x(ja(k))`; `(Symb*a(k))*x(i)` left to right
  (`a*Symb` commuted in both, same bits); `y(ja(k)) + that`. After the row, `y(i) + t`.
  Three multiplies and two adds per `k`, no FMA. `y(1:n) = 0.0` is a `memset` in both.
- (v) Assumed-shape `a(:)`, `ja(:)`: the Fortran object has a second loop for non-unit strides;
  the translation takes contiguous arrays, which is what `CalcQRSNL` passes (whole arrays
  `Mnpqr`, `qi_icCos`).
- (v) The C++ object reloads `x(i)` inside the inner loop (it cannot assume `y` and `x` do not
  overlap; Fortran can). Same value whenever the Fortran's no-alias rule holds, so same bits.
- (v) CUDA compiled (`nvcc_wrapper`, sm_89, `--fmad=false`), compile only: SASS has 3 `FMUL`,
  3 `FADD`, no `FFMA`, no `MUFU`. Compiled warning-free on the host.
- No libm call, no MIN/MAX, no integer conversion of a real.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3gkemd.F90` from the `build_nl5` flags (object identical to the CMake one), confirmed `ASymSmatTimVec` is not inlined into `CalcQRSNL` (`Symb` passed by reference, so no `-1.0` folding), and compared its GIMPLE with the C++ unit's operation by operation: same memset, same per-`k` order `t + a*x(ja)` and `y(ja) + (a*Symb)*x(i)`, same row add, no vectorisation in either. The CUDA compile was not repeated.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
