# CSINH_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Complex hyperbolic sine of the fluid-mud dissipation module (switch `BT8`), called three times
per wavenumber by `W3SBT8` (`w3sbt8md.F90:337`, `:339`, `:348`) on `K*H_WDEPTH`, `K*THICKM` and
`LAM2*THICKM`.

## What was ported

| | |
|---|---|
| WW3 routine | `CSINH`, `model/src/w3sbt8md.F90:504-511` at NOAA-EMC/WW3 `03adda91`; compiled by the `BT8` switches of `ww3_tbt1.1` and `ww3_tbt2.1` (`switch`, `switch_BT8`, `switch_BT8_MPI`; `git grep -lw BT8 -- '*switch*'`), by no NOAA operational switch. No `#ifdef` in the routine |
| Kokkos code | [`../src/ww_kokkos/bt8_csinh.hpp`](../src/ww_kokkos/bt8_csinh.hpp), `ww::bt8::csinh(c, cs)` on `Kokkos::complex<float>` |
| C shim | none yet |
| Plan | issue #42, batch goal/42-port-batch-easy |

## Arithmetic

Reference: the `-O3` object of `w3sbt8md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object, 2026-10-16.

- (v) GIMPLE of both: `__builtin_cexpif(y)` (one `sincosf` call), `sinhf(x)`, `re = sinh*cos`,
  `coshf(x)`, `im = sin*cosh`, stored as a 2-float vector. Same calls, same products, same
  operand pairs. For finite or single-NaN operands a product's bits do not depend on operand
  order.
- (v) `X`, `Y` are implicit default `REAL` (the module has no `IMPLICIT NONE`; the one at `:189`
  is inside `W3SBT8`), so single precision in both.
- (v) `W3SBT8` calls `__w3sbt8md_MOD_csinh` out of line (three `call` relocations), so the
  routine translated is the one that runs.
- (v) No FMA, no libmvec call, no reduction.
- ⚠ Operand order in the machine code differs for the real part: the Fortran object computes
  `mulss` with `sinhf(x)` as first operand (`sinh*cos`), the C++ unit `vmulss` with `cos(y)`
  first; both compute `cosh*sin` for the imaginary part. x86 returns the first operand's payload
  when both operands are NaN, so the real part's NaN bits can differ when `x` is NaN and `y` is
  NaN or infinite (for example `x` = `0x7fc00000`, `y` = `+Inf`: Fortran `0x7fc00000`, C++
  glibc's `cosf(Inf)` NaN). Swapping the source operands does not change the register choice, and
  once inlined into a caller the order is the caller's codegen; not fixable in this header.
  Finite inputs are unaffected.
- (v) Fresh review 2026-10-16: no defect for finite inputs. Recompiled `w3sbt8md.F90` from
  `build_mixa`'s `flags.make` (identical to the CMake object) and the header in a C++ unit with
  the openmp-release flags (warning-free); `-O3` GIMPLE identical statement by statement
  (`__builtin_cexpif`, `sinhf`, `coshf`, two products, one 2-float vector store); assembly differs
  only in the real part's `mulss` operand order, which matters only for two NaN operands (bullet
  above, added by this review). Nothing fixed in the header.
- ⚠ CUDA not compiled. On a GPU `sinhf`, `coshf`, `sincosf` are CUDA's own and need not return
  glibc's bits (`AGENTS.md`, "Bit for bit").

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
