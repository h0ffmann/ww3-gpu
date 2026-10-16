# W3IC3WNCG_CHENG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Calculation of the complex wavenumber and group velocity for waves in a viscoelastic ice layer
(IC3 "Cheng"), outsourced from `W3SIC3` so it can be updated at each time step an ice parameter
changes (`w3sic3md.F90:1811-1815`). It reads a pre-calculated table, then re-solves the dispersion
relation for frequencies where the open-water/wave-in-ice transition is shallow enough.

## What was ported

| | |
|---|---|
| WW3 routine | `W3IC3WNCG_CHENG`, `model/src/w3sic3md.F90:1800-1959` at NOAA-EMC/WW3 `03adda91` (160 lines); compiled under switch `IC3`, by 46 switch files of the repository, by no NOAA operational switch. No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/ic3_w3ic3wncg_cheng.hpp`](../src/ww_kokkos/ic3_w3ic3wncg_cheng.hpp), `ww::ic3::w3ic3wncg_cheng(...)`; module state is passed explicitly (`NK`, `SIG`, `IC3PARS(10:11)`, the `IC3WN_R/I/CG` tables and the `WAVNU1` dispersion table) |
| Callees | `WAVNU1` through `ww::prop::wavnu1` ([`prop_wavnu1.hpp`](../src/ww_kokkos/prop_wavnu1.hpp)); `CMPLX_ROOT_MULLER_CHENG` through a callable ([`ic3_cmplx_root_muller_cheng.hpp`](../src/ww_kokkos/ic3_cmplx_root_muller_cheng.hpp)); `SMOOTH_K` ([`ic3_smooth_k.hpp`](../src/ww_kokkos/ic3_smooth_k.hpp)); `CGinIC3_CHENG` ([`ic3_cginic3_cheng.hpp`](../src/ww_kokkos/ic3_cginic3_cheng.hpp)) |
| Not translated | the `WRITE(NDSE,900)` and `EXTCDE(3)` on an unrecognised array size; the caller must pass `n == NK` or `n == NK+2` |
| Structural change | the automatic `SIGMA` array is a pointer to the caller-provided `sig` slice; the `ALLOCATE`/`DEALLOCATE` are removed |
| C shim | none yet; its caller is `W3SIC3` |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3sic3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object, 2026-10-16. Its GIMPLE was read against a throwaway C++ unit
compiled with the `openmp-release` flags and `-ffp-contract=off`. That unit compiles warning-free.

- (v) Array bounds and table indexing: the Fortran's `I1:I2` slice maps to the 0-based offsets
  `0:NK+1` when `n == NK+2` and `1:NK` when `n == NK`; the selected column `I` is
  `MIN(NINT(HICE/IC3_DITK), ITKNUM)` with `HICE` capped by `DBLE(IC3HILIM)`, reproduced with
  `Kokkos::fmin` and `Kokkos::round`.
- (v) `WN_R(IK)*DEPTH > 4.0` is reproduced with `double(wn_r[ik-1]) * depth > 4.0`, matching the
  Fortran's promotion of `WN_R` (default `REAL`) and the literal `4.0` to `REAL(8)`.
- (v) `X0 = X1*(1-RR)` and `X2 = X1*(1+RR)` with `RR = 0.01` keep the products with `0.0` in the
  imaginary part, as the object emits them.
- (v) `CMPLX_ROOT_MULLER_CHENG` is called with `JUDGE = 1` and the double-promoted `SIGMA(IK)`,
  `ES_MOD`, `NU`, `DICE`, `HICE`, `DEPTH`. The returned complex parts are narrowed back to
  default `REAL` with `static_cast<Real>`.
- (v) `SMOOTH_K` is called with `n = KU - KL + 1` and `switchid = 0`, matching the Fortran's
  `CALL SMOOTH_K(WN_R,WN_I,SIGMA,KU-KL+1,0)`.
- (v) The `IC3KILIM` cap is `MIN(WN_I(IK),IC3KILIM)` in the Fortran's order; `Kokkos::fmin`
  matches.
- (v) `CGinIC3_CHENG(CG,SIGMA,WN_R,KU-KL+1)` is reproduced with the same 0-based arrays and `n`.
- ⚠ The host path uses glibc's `cabs`, `csqrt`, `cexp`, `cpow` inside `CMPLX_ROOT_MULLER_CHENG`
  and `Kokkos::fmin`/`Kokkos::round`. A device path would need the same bit-for-bit guarantees
  as every other IC3 routine. CUDA not compiled.
- No reduction, no parallel loop in this host helper.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Its checkpoint (ADR-0011) needs a shim and `W3SIC3`'s caller hunk;
the regtests that compile it are the `IC3` cases of `ww3_tic1.1`.
