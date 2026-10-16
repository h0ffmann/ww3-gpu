# CMPLX_ROOT_MULLER_V1_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A helper of the IC3 sea-ice source term, "V1" path: Muller's method for the complex root of
`F_ZHAO_V1`. From three guesses `X0, X1, X2` it fits a parabola through `F_ZHAO_V1` at the last
three iterates and steps to its root nearer `P2`, until `|P3 - P2| < 1E-5` or `|F(P3)| < 1E-5`,
for at most 1000 steps. `JUDGE = 0` finds the open-water wavenumber (`FUNC0_ZHAO`), `JUDGE = 1`
the viscoelastic-ice one (`FUNC1_ZHAO`). Callers: `WN_CMPLX_V1` (`w3sic3md.F90:953`, `:958`,
`:965`, `:972`, `:977`) and `WN_CMPLX_HF` (`:1090`, `:1097`), both under `W3IC3WNCG_V1`, which
`W3SIC3` (`:549`) and `W3WAVE` (`w3wavemd.F90:1063`) call when `IC3CHENG` is false.

## What was ported

| | |
|---|---|
| WW3 routine | `CMPLX_ROOT_MULLER_V1`, `model/src/w3sic3md.F90:1106-1276` at NOAA-EMC/WW3 `03adda91`; compiled under switch `IC3`, by 46 regtest switch files (`ww3_tic1.1`, `ww3_tic1.2`, `ww3_tic1.3`, `ww3_ts1`; `git grep -lw IC3 -- '*switch*'`, less `model/src/cmake/switches.json`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`); exercised only by regtest namelists with `IC3CHENG = F`. No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/ic3_cmplx_root_muller_v1.hpp`](../src/ww_kokkos/ic3_cmplx_root_muller_v1.hpp), `ww::ic3::cmplx_root_muller_v1(const Kokkos::complex<double>& x0, x1, x2, int judge, double sigma, double es, double nu, double dice, double hice, double depth, Kokkos::complex<double>& p3) -> int`, and the callable `ww::ic3::CmplxRootMullerV1` with that signature, the `root` that `wn_cmplx_hf` ([`WN_CMPLX_HF_KOKKOS.md`](WN_CMPLX_HF_KOKKOS.md)) takes. Calls `f_zhao_v1` ([`F_ZHAO_V1_KOKKOS.md`](F_ZHAO_V1_KOKKOS.md)) with `Func1Zhao{}` ([`FUNC1_ZHAO_KOKKOS.md`](FUNC1_ZHAO_KOKKOS.md)) |
| Structural change | The Fortran result is written to `p3`; the return is 0, or 2 where the Fortran writes four diagnostic lines to `NDSE` and calls `EXTCDE(2)`, which aborts: `A = B = 0` (`:1239-1244`) and no convergence in 1000 steps (`:1264-1268`). The shim does the `WRITE`; on a nonzero return `p3` holds the last iterate (0 if the first step fails) |
| C shim | none yet; its callers `WN_CMPLX_V1` (not ported) and `WN_CMPLX_HF` (header only) are not wired |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3sic3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object; its GIMPLE (`-fdump-tree-optimized-lineno`) of
`__w3sic3md_MOD_cmplx_root_muller_v1` read against that of a throwaway C++ unit (an external
function calling `cmplx_root_muller_v1`) compiled with the `openmp-release` flags and
`-ffp-contract=off`, 2026-10-16. The C++ unit compiles warning-free.

- (v) Constants: `EPSI = DLTA = 1.E-5`, a REAL literal stored in DOUBLE PRECISION, is
  `9.99999974737875163555145263671875e-6` (`0x1.4f8b58p-17`) in both GIMPLEs, with a
  `static_assert` of how it is built. The literals `1.`, `2.`, `4`, `2` and `1` multiplying or
  adding a DOUBLE COMPLEX become `(x, 0.0)`: the products with `0.0` and the `+ 0.0` on the
  imaginary part are kept in both GIMPLEs (NaN and the sign of zero).
- (v) `Q**2.` and `(1 + Q)**2.` and `B**2.` are `__builtin_cpow(z, (2.0, 0.0))` in both, not
  `z*z`; `Q**2.*Y0` is computed once and shared by `A` and `B` in both. `(1.+Q)` and `(1+Q)` are
  the same value, computed once in both. `SQRT(DISC)` is `__builtin_csqrt`, every `ABS` of a
  complex `__builtin_cabs` (glibc's `cpow`, `csqrt`, `cabs` on the host, as the object calls).
- (v) Every complex product is the four-product form (no `__muldc3`), every quotient Smith's
  algorithm inlined with the same two branches (`|Im| > |Re|` of the divisor) and the same
  ratio, denominator and numerator operations: `Q`, `2*C/DEN1`, `2*C/DEN2`, `C/B`. Each real
  and imaginary part reads the same products and subtractions in the same order.
- (v) Branches: `ABS(A) /= 0`, then `ABS(B) /= 0`; `ABS(DEN1) < ABS(DEN2)` is
  `cabs(DEN2) > cabs(DEN1)` in both (a NaN takes the `DEN1` branch). The exit test
  `ABS(P3-P2) < DLTA .OR. ABS(Y3) < EPSI` evaluates `P3 - P2` in the same order.
- ⚠ g++ and gfortran put the two addends of some imaginary parts (and the final `+ Q**2*Y0` of
  `A`) in the other order in GIMPLE: `qr*y2i + qi*y2r` against `y2r*qi + y2i*qr`. Each sum is
  the same IEEE value; only which NaN payload or sign survives when both addends are NaN could
  differ, and the assembly's `addsd` operand order was not read for this routine.
- ⚠ gfortran inlines `FUNC1_ZHAO` for `Y3` inside the loop and calls it out of line for
  `Y0..Y2`; g++ calls `func1_zhao` out of line everywhere. The operations are the same; their
  SLP pairing in the inlined copy, and so a NaN payload, was not compared.
- (v) No reduction, no MIN/MAX, no `INT`/`NINT`, no libmvec call.
- ⚠ On a device `cpow`, `csqrt`, `cabs` and `cexp` are Kokkos's algorithms, not glibc's (see
  `FUNC1_ZHAO_KOKKOS.md`). CUDA not compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
