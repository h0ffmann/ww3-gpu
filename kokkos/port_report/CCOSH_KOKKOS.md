# CCOSH_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Complex hyperbolic cosine of a `COMPLEX(4)` argument, a helper of the fluid-mud dissipation
`W3SBT8` (Dalrymple and Liu 1978 two-layer model, switch `BT8`), called at
`w3sbt8md.F90:338`, `:340`, `:347`.

## What was ported

| | |
|---|---|
| WW3 routine | `CCOSH`, `model/src/w3sbt8md.F90:523-530` at NOAA-EMC/WW3 `03adda91`; compiled by the `BT8` switches of `ww3_tbt1.1` and `ww3_tbt2.1` (`switch`, `switch_BT8`, `switch_BT8_MPI`; `git grep -lw BT8 -- '*switch*'`), by no NOAA operational switch. No `#ifdef` in the routine |
| Kokkos code | [`../src/ww_kokkos/bt8_ccosh.hpp`](../src/ww_kokkos/bt8_ccosh.hpp), `ww::bt8::ccosh(c, cc)` with `Kokkos::complex<float>` |
| C shim | none yet; its only caller is `W3SBT8` |
| Plan | issue #42, batch `goal/42-port-batch-easy` |

## Arithmetic

Reference: the `-O3` object of `w3sbt8md.F90` (WW3's CMake Release flags, gfortran 16.2, the
lab switch with `IC3 IS2 BT8 TIDE UOST FLD2 IG1`), recompiled and checked identical to the CMake
object, compared with a throwaway C++ unit under the `openmp-release` flags, 2026-10-16.

- (v) `X`, `Y` are implicit default `REAL` (the module has no `IMPLICIT NONE`; the one at `:189`
  is inside `W3SBT8`), so `float`; GIMPLE types them `real(kind=4)`.
- (v) Both objects call `sincosf(Y)` once (gfortran and g++ fold `SIN(Y)`, `COS(Y)` into
  `__builtin_cexpif`), then `coshf(X)` and `sinhf(X)` from glibc; no libmvec call.
- (v) Real part `coshf(X)*cos(Y)`, imaginary part `sin(Y)*sinhf(X)`: the same operand order in
  both GIMPLE dumps, one `mulss` each, no FMA, no `__mulsc3`. g++ calls `sinhf` before `coshf`;
  both are pure, so the bits do not change.
- ⚠ The imaginary-part multiply has the same operands but not the same first source in the two
  objects: gfortran emits `mulss` with `sinhf(X)` as destination (`w3sbt8md.o` `__w3sbt8md_MOD_ccosh+0x45`),
  g++ `vmulss` with `sin(Y)` as first source (`tu.o` `+0x61`), whichever order the C++ source
  writes (both orders tried; GIMPLE canonicalises the commutative product and register allocation
  picks the operand). x86 returns the first source's NaN when both are NaN, so the payload of
  `AIMAG(CC)` can differ only when `X` is NaN and `SIN(Y)` is NaN (`Y` NaN or infinite). Every
  finite or infinite non-NaN result is bit-identical. Not fixable in the header.
- ⚠ Not compiled for CUDA. On a GPU `Kokkos::cosh`, `sinh`, `sin`, `cos` resolve to CUDA's math
  library, whose results need not equal glibc's (`AGENTS.md`, bit for bit).
- (v) Fresh review 2026-10-16: no defect for non-NaN results. The `w3sbt8md.F90` object recompiled
  from `build_mixa`'s flags in the pinned shell was identical to the CMake object; `W3SBT8` calls
  the standalone `__w3sbt8md_MOD_ccosh` (not inlined, `:338`, `:340`, `:347`); its GIMPLE and
  disassembly were compared with a throwaway C++ unit (calls `sincosf`, `sinhf`, `coshf`, same
  products, no FMA); found the NaN-payload operand order above, header unchanged.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
