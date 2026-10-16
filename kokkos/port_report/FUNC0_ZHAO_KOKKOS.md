# FUNC0_ZHAO_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A helper of the IC3 sea-ice source term, "V1" path: the residual of the open-water dispersion
relation at a complex wavenumber k and depth d,
`FUNC0 = SIGMA**2 - G k tanh(k d)`, with `tanh(k d)` written as
`(EXP(k d) - EXP(-k d)) / (EXP(k d) + EXP(-k d))` when `REAL(k d) <= 4`, and `tanh = 1` otherwise
(`w3sic3md.F90:1454-1460`). It is the `JUDGE == 0` branch of `F_ZHAO_V1` (`:1364`), the function
whose root `CMPLX_ROOT_MULLER_V1` finds; `JUDGE = 0` is passed only by `WN_CMPLX_V1` (`:953`,
`:972`), which gives the open-water first guess `WN0`. The chain is reached from `W3IC3WNCG_V1`,
called by `W3SIC3` (`:549`) and `W3WAVE` (`w3wavemd.F90:1063`) when `IC3CHENG` is false. The
routine's own remark (`:1424-1425`) says the Cheng update does not use it.

## What was ported

| | |
|---|---|
| WW3 routine | `FUNC0_ZHAO`, `model/src/w3sic3md.F90:1372-1462` at NOAA-EMC/WW3 `03adda91`; compiled under switch `IC3`, by 46 regtest switch files (`ww3_tic1.1`, `ww3_tic1.2`, `ww3_tic1.3`, `ww3_ts1`; `git grep -lw IC3 -- '*switch*'`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`); exercised only by regtest namelists with `IC3CHENG = F` (for example `ww3_tic1.1/input_IC3/namelists_*_NOCHENG.nml`, `ww3_tic1.2/input_IC3_A*/namelists_1-D.nml`). No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/ic3_func0_zhao.hpp`](../src/ww_kokkos/ic3_func0_zhao.hpp), `ww::ic3::func0_zhao(const Kokkos::complex<double>& wn, double sigma, double depth) -> Kokkos::complex<double>`: `DOUBLE COMPLEX` is `Kokkos::complex<double>`, `DOUBLE PRECISION` is `double`; `ww::ic3::func0_zhao_cexp` is its `EXP` of a complex |
| C shim | none yet; its caller `F_ZHAO_V1` is not ported |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3sic3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object; its GIMPLE (`-fdump-tree-optimized-lineno`) and disassembly with
relocations read against those of a throwaway C++ unit compiled with the `openmp-release` flags
and `-ffp-contract=off`, 2026-10-16. The C++ unit compiles warning-free.

- (v) gfortran expands every complex operation inline, component by component, and the header
  writes them out the same way rather than through `Kokkos::complex` operators: `WN*DEPTH` with
  `DEPTH` promoted to `(DEPTH, 0)` is `(d wr - wi*0.0, wr*0.0 + d wi)`; `WN*DBLE(GRAV)` is
  `(wr g - wi*0.0, wr*0.0 + wi g)`; `TH*WN*DBLE(GRAV)` is the product `TH*WN`
  `(wr thr - wi thi, wr thi + wi thr)` then by `(g, 0)`. The products with `0.0` are kept by both
  compilers (they carry a NaN and the sign of zero), and `SIGMA**2. - z` is
  `(s s - z.re, 0.0 - z.im)`: a subtraction from `0.0`, not a negation, in both GIMPLEs.
- (v) `SIGMA**2.` (a real exponent) is folded by gfortran into `SIGMA*SIGMA`; no `pow` call.
- (v) The complex quotient `TH = A / B` (`:1455-1456`) is Smith's algorithm inlined by gfortran
  (Fortran complex rules, no `__divdc3` call): branch on `|B.re| < |B.im|`, ratio, denominator,
  two numerators, two divisions. The header writes the same two branches; the C++ GIMPLE has the
  same operations with the same operands in each. `Kokkos::complex`'s `operator/` scales by the
  larger modulus and would not give these bits.
- (v) `EXP` of a `DOUBLE COMPLEX` is a call to glibc `cexp` in the Fortran object (two
  `R_X86_64_PLT32 cexp` relocations, `EXP(-WN*DEPTH)` first). The host path calls
  `std::exp(std::complex<double>)`, which libstdc++ lowers to `__builtin_cexp`; the C++ object has
  the same two `cexp` calls in the same order on the same arguments, the negated `(-kd.re, -kd.im)`
  first.
- (v) `DBLE(GRAV)` is folded to `9.805999755859375` (`GRAV = 9.806` as `REAL`,
  `constants.F90:62`): the header writes the hex literal `0x1.39cacp+3` with a `static_assert`
  that it equals `double(9.806f)`.
- (v) The test `REAL(WN*DEPTH) <= 4.` goes to the `ELSE` branch for a NaN in both objects
  (`<=` is false).
- (v) The Fortran function is scalar; g++ SLP-vectorises some pairs in the C++ (the two
  `* 0.0`, the `(g, 0)` products, a `VEC_ADDSUB`). Each lane does the same IEEE operation on the
  same operands, so the bits are the same. g++ commutes some additions (`d wi + wr*0.0` for
  `wr*0.0 + d wi`), which is exact in IEEE arithmetic; only which NaN payload survives when both
  operands are NaN could differ ⚠ (not checked).
- No reduction, no MIN/MAX, no `INT`/`NINT`, no libmvec call.
- ⚠ On the device `func0_zhao_cexp` uses `Kokkos::exp`, `exp(re) * (cos(im), sin(im))`, which is
  not glibc's `cexp` algorithm (glibc scales near overflow and uses `sincos`); not bit-checked.
  CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. `w3sic3md.F90` recompiled from `build_mixa`'s flags,
  identical to the CMake object; the out-of-line `func0_zhao` GIMPLE and its five inlined copies
  (in `f_zhao_v1`, `cmplx_root_muller_v1` and callers) read operation by operation against the C++
  unit's GIMPLE: same products with `0.0`, same Smith branches and operands, same `cexp` calls,
  folded `GRAV`, `<= 4.0` branch and `0.0 - z.im`; no FMA in the C++ object. Citations and the 46
  regtest switch files checked at `03adda91`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
