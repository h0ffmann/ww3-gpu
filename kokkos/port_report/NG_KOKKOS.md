# NG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The dissipation rate and the muddy wave number of a viscous fluid-mud layer after Ng (2000),
for one frequency (switch `BT9`). With `d~ = DTILDE` the mud depth over the mud Stokes boundary
layer thickness `SBLTM`, `zeta = ZETA` and `gamma = GAMMA` (water over mud density), it builds
Ng's coefficients `B1`, `B2`, `B3` from `sinh`, `cosh`, `sin`, `cos` of `d~` (`:531-543`), then
`DISS = -SBLTM*(B1/B3+B2/B3)*k^2/(sinh(2kh)+2kh)` and
`WKDR = k - BR*k/(sinh(kh)*cosh(kh)+kh)`, `BR = k*SBLTM*(B1-B2)/(2*B3) + gamma*k*DM` (`:545-553`).
Its only caller is `W3SBT9` (`w3sbt9md.F90:334`), once per frequency where `k*h < KDCUTOFF` and
the mud depth exceeds `1.0E-5`.

## What was ported

| | |
|---|---|
| WW3 routine | `NG`, `model/src/w3sbt9md.F90:429-557` at NOAA-EMC/WW3 `03adda91`; compiled by the regtest switches of `ww3_tbt1.1` and `ww3_tbt2.1` (`switch_BT9`, `switch_BT9_MPI`; `git grep -lw BT9 -- '*switch*'`), by no NOAA operational switch. No `#ifdef` in the routine |
| Kokkos code | [`../src/ww_kokkos/bt9_ng.hpp`](../src/ww_kokkos/bt9_ng.hpp), `ww::bt9::ng` (`WKDR`, `DISS` by reference, `INTENT(OUT)`) |
| C shim | none yet; it is called from inside `W3SBT9`, so it enters WW3 with the port of `W3SBT9` |
| Plan | [`EASY_BATCH_KOKKOS_PLAN_202610.md`](../../docs/EASY_BATCH_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3sbt9md.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd` with `NL4 IC5 BT9 NLS`, `build_mixb`), recompiled and checked identical to the
CMake object, against a C++ unit with the `openmp-release` flags, 2026-10-16.

- (v) The `-O3` GIMPLE of both is the same 74 scalar arithmetic operations and one negation on the same operands: 44
  multiplications, 14 additions, 11 subtractions, 5 divisions, and the negation is an `xorps`, in
  either object; `2.0*X` is `X+X` in both. gfortran folds `-2.0*GAMMA**2+2.0*GAMMA-1.` to
  `2*GAMMA - 2*GAMMA**2 - 1` and shares it between `B1` and `B2`; the C++ writes that folded
  form (`y+(-x)` and `y-x` round alike). `DISS` is `-(a/b)` in both.
- (v) Libm calls, the same six in the same order in both objects: one `sincosf` for
  `SIN(DTILDE)`/`COS(DTILDE)` (g++ merges `::sinf` and `::cosf` of one argument the same way),
  `sinhf`, `coshf` of `DTILDE`, `sinhf(2kh)`, `sinhf(kh)`, `coshf(kh)`. All glibc scalar, no
  libmvec, no FMA, no reduction, no MIN/MAX, not vectorised. `NG` is not inlined into `W3SBT9`
  in the Fortran object.
- ⚠ The operands of some commutative multiplications and additions appear in the other order
  in the C++ GIMPLE (for example `GAMMA*(...)` against `(...)*GAMMA`). That changes nothing but
  which payload a NaN carries when both operands are NaN; a NaN `DISS` stops the model at the
  check of `W3SBT9` (`:383-391`).
- ⚠ `SIGMA` is a dummy the Fortran never reads, and `BI` (`:547`) is computed and never used;
  gfortran drops it, and so does the C++, which does not write it.
- ⚠ No guard against `B3 = 0` or a zero denominator, as in the Fortran: `Inf`/`NaN` the same
  way in both.
- ⚠ On a device `sinh`, `cosh`, `sin`, `cos` are CUDA's, not shown to return glibc's bits. CUDA
  not compiled.
- (v) Fresh review 2026-10-16: no defect. The module was recompiled from `build_mixb`'s flags
  (identical to its CMake object) and both functions were evaluated symbolically from their
  `-O3` assembly, libm calls included: `WKDR` and `DISS` are the same expressions, constants and
  calls in both, up to the operand order of 12 commutative operations (NaN payload only), with
  the negation of `DISS` applied to the whole quotient in both. Nothing was changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
