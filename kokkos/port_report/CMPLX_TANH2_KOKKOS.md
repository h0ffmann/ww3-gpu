# CMPLX_TANH2_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Complex hyperbolic tangent of the viscoelastic sea-ice module (switch `IC5`), written as
`tanh(x) = (1 - exp(-2x)) / (1 + exp(-2x))` to avoid the overflow of `sinh/cosh` for large `kh`.
Called three times by `NR_CORR` (`w3sic5md.F90:1741-1743`), the Newton-Raphson correction of the
complex ice-covered dispersion relation, itself called by `NR_ROOT` (`:1880`, `:1888`), called by
`FSDISP` (`:875`). Upstream comment (`:2214-2215`): the formula misbehaves as `x -> -inf`, "in our
cases x is alway >0" ⚠ (not checked).

## What was ported

| | |
|---|---|
| WW3 routine | `CMPLX_TANH2`, `model/src/w3sic5md.F90:2160-2256` at NOAA-EMC/WW3 `03adda91`; compiled by the `IC5` switches of `ww3_tic1.1` (`input_IC5_M1`, `_M2`, `_M3`; `git grep -lw IC5 -- '*switch*'`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`). Only `#ifdef W3_S` (tracing, no arithmetic) |
| Kokkos code | [`../src/ww_kokkos/ic5_cmplx_tanh2.hpp`](../src/ww_kokkos/ic5_cmplx_tanh2.hpp), `ww::ic5::cmplx_tanh2(x)` on `Kokkos::complex<double>` (`COMPLEX(KDPC)`, `KDPC = KIND((1.0D0, 1.0D0))`, `:143`) |
| C shim | none yet |
| Plan | issue #42, batch goal/42-port-batch-easy |

## Arithmetic

Reference: the `-O3` object of `w3sic5md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixb`: `switch_lab_shrd` with `NL4 IC5 BT9 NLS`), recompiled and checked identical to the
CMake object, 2026-10-16. C++ unit: the header behind an `extern "C"` wrapper, openmp-release
flags, warning-free.

- (v) The routine does not call `CMPLX_SINH` or `CMPLX_COSH`, although its header lists them.
- (v) `2*X` is a full complex product with `(2,0)`: GIMPLE `re = xr*2 - xi*0`, `im = xr*0 + xi*2`;
  the `*0` terms are kept (NaN, signed zero) in both objects. The Fortran object writes `xr*2` as
  `xr+xr` (same bits).
- (v) `EXP(-2*X)` is one `__builtin_cexp` (glibc `cexp`) in both; the C++ host path reaches it
  through `std::exp(std::complex<double>)`, which libstdc++ lowers to `__builtin_cexp` (one
  `cexp` relocation in the unit).
- (v) The constant `1` enters as `(1,0)`: numerator `(1-er, 0-ei)`, denominator `(er+1, ei+0)`;
  `0-ei` and `ei+0` are not folded in either object.
- (v) The quotient is Smith's algorithm (`-fcx-fortran-rules`, no scaling, no NaN recovery):
  branch on `|dr| < |di|`, then `r`, `den`, and the two quotients in the Fortran's operand
  pairs, statement by statement in the GIMPLE. `Kokkos::complex`'s division scales differently,
  so the header writes the quotient in scalars.
- (v) No FMA in either object (`-ffp-contract=off`). g++ packs the numerator and the two final
  quotients into 2-lane vectors (`vsubpd`, `vmulpd`, `vdivpd`); each lane is the same IEEE
  operation on the same operands as the Fortran's scalar one.
- ⚠ When both operands of an add or multiply are NaN, x86 returns the first source's NaN. Read
  site by site in the two objects, the operand orders differ at three commutative sites (`nr*r`,
  `ni*r`, `nr + ni*r` in the `|dr| >= |di|` branch, which NaN takes; `nr*r`, `nr*r + ni` in the
  other). The only one that can change a result is the real part's numerator `nr + ni*r` when
  `cexp` returns a real and an imaginary NaN of different bits: the Fortran's carries the
  imaginary NaN, the C++'s the real one. Whether glibc's `cexp` ever returns such a pair was not
  checked; finite results are unaffected.
- ⚠ In the object `NR_CORR` inlines its three calls (three `cexp` relocations in
  `__w3sic5md_MOD_nr_corr`); the inlined copies were not compared, only the out-of-line
  `__w3sic5md_MOD_cmplx_tanh2`.
- ⚠ On a device the header uses `Kokkos::exp`, not glibc's `cexp`: not bit-identical
  (`AGENTS.md`, "Bit for bit"). CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect in the finite-value arithmetic. The `w3sic5md.F90`
  object was recompiled from `build_mixb`'s flags and found identical to the CMake object; its
  GIMPLE and disassembly were compared with the C++ unit's, operation by operation (the `2*X`
  product, the negation, one `cexp`, the `(1,0)` numerator and denominator, the branch, NaN
  included, and Smith's quotient). The NaN-operand-order bullet above was made precise; the
  header was not changed. `ic5_cmplx_tanh2.hpp`, `ic5_cmplx_sinh.hpp` and `ic5_cmplx_cosh.hpp`
  compile together in one unit warning-free (no duplicate helper).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
