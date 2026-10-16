# IC3PRECALC_CHENG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A set-up routine of the IC3 sea-ice source term (Cheng's viscoelastic ice layer): it fills one
column of the IC3 table, the real and imaginary wavenumber `k_r`, `k_i` and the group velocity
`c_g` of the ice-covered dispersion relation at every frequency `IK = 0..NK+1`, for one ice
thickness `ICE1` and the rheology `ICE2` (viscosity), `ICE3` (ice density), `ICE4` (shear
modulus). Per frequency it takes the open-water wavenumber from `WAVNU1` as the first guess and the
two previous complex roots, and calls `WN_PRECALC_CHENG` (Muller's method); then `SMOOTH_K`
replaces the mode-switch points and `CGINIC3_CHENG` differentiates `sigma(k_r)`. Its only caller is
`IC3TABLE_CHENG` (`w3sic3md.F90:2039`), once per thickness `JITK = 1..300` at `DPT = 999 m`.

## What was ported

| | |
|---|---|
| WW3 routine | `IC3PRECALC_CHENG`, `model/src/w3sic3md.F90:2047-2160` at NOAA-EMC/WW3 `03adda91` (`PRIVATE`, `:91`); compiled under switch `IC3`, by 46 regtest switch files (`git grep -lw IC3 -- '*switch*'`, 47 hits less `model/src/cmake/switches.json`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`). Exercised only where Cheng's method is selected (see [`IC3TABLE_CHENG_KOKKOS.md`](IC3TABLE_CHENG_KOKKOS.md)). No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/ic3_ic3precalc_cheng.hpp`](../src/ww_kokkos/ic3_ic3precalc_cheng.hpp), `ww::ic3::ic3precalc_cheng`, an `inline` host function template |
| Callees | `WAVNU1` ([`prop_wavnu1.hpp`](../src/ww_kokkos/prop_wavnu1.hpp), its `DispTable` passed in), `SMOOTH_K` ([`ic3_smooth_k.hpp`](../src/ww_kokkos/ic3_smooth_k.hpp), its automatic arrays `DIFF`, `REMOVEID` as caller-provided work arrays of `nk + 2` floats), `CGINIC3_CHENG` ([`ic3_cginic3_cheng.hpp`](../src/ww_kokkos/ic3_cginic3_cheng.hpp)); `WN_PRECALC_CHENG` (`:2164-2417`) has no header and is not claimed: the port takes it as a host callable with the Fortran's dummies in order, `wn` and `switchid` by reference |
| Host only | `WN_PRECALC_CHENG` `PRINT`s (`:2412-2414`); `IC3TABLE_CHENG`'s callable `(wn_r, wn_i, cg, ice1..ice4, dpt)` is this function with `nk`, `sig`, the table, the work arrays and the callee bound by the caller |
| C shim | none yet; its caller is not ported |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3sic3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object; its GIMPLE (`-fdump-tree-optimized-lineno`) and disassembly read
against those of a throwaway C++ unit compiled with the `openmp-release` flags and
`-ffp-contract=off`, `WN_PRECALC_CHENG` an external function, 2026-10-16.

- (v) The routine's own arithmetic is conversions only, the same in both objects: five
  `cvtss2sd` of `DPT`, `ICE1..ICE4` (`:2125-2129`); `WNM1`, `WNM2` as `cvtps2pd` of the pair
  `(WN_R, WN_I)` at `IK-1`, `IK-2` (`:2145-2146`); `cvtss2sd` of `SIG(IK-1)`, `SIG(IK)` and
  `K_OCEAN` (`:2147`, `:2152`, `:2150`); two `cvtsd2ss` of the result (`:2154-2155`). All widenings
  are exact; the narrowings round to nearest in both.
- (v) `K_NOICE = K_OCEAN` then `CMPLX(K_NOICE, 0.0)` (`:2137`, `:2150`): `CMPLX` without `KIND`
  narrows the `REAL(8)` back to `REAL`; gfortran folds the round trip to one widening of
  `K_OCEAN`, which the port writes directly.
- (v) `IK < KL+2` is `ik <= 1`; `KL = 0`, `KU = NK+1` and `SWITCHID = 0` as in the GIMPLE. The
  Fortran passes `IK`, `KU`, `SWITCHID` by reference and reloads `IK` and `KU` after the call;
  `WN_PRECALC_CHENG` writes only `WN` and `SWITCHID` (`:2385`, `:2396`, `:2407`), so the port
  passes `ik`, `ku` by value.
- (v) `SIG` goes through `_gfortran_internal_pack` before `SMOOTH_K` and `CGINIC3_CHENG`
  (`:2157-2158`); a copy, no arithmetic. `N = KU-KL+1 = NK+2` in both.
- (v) No libm or libmvec call, no reduction, no `MIN`/`MAX` in the routine's own code; the C++ unit
  compiles warning-free.
- ⚠ The Fortran calls `WAVNU1`, `SMOOTH_K` and `CGINIC3_CHENG` out of line; g++ inlines them into
  this routine and vectorises the loops of `SMOOTH_K` and `CGINIC3_CHENG`. Their arithmetic is
  covered by their own reports; the `CGINIC3_CHENG` note on the summation order of the scalar
  epilogue (a NaN payload only) applies here too.
- ⚠ `SWITCHID` is set to `IK`, an index of `0:NK+1` (`:2385`), while `SMOOTH_K` indexes `1:N` and
  tests `SWITCHID == I` (`:2511`), so it marks the two frequencies `SWITCHID-1`, `SWITCHID-2`
  below the switch point, and a switch at `IK <= 2` is never marked. Possibly an off-by-one in
  WW3; reproduced as written.
- ⚠ `SMOOTH_K` needs `N >= 3` and `CGINIC3_CHENG` `N >= 2`; the header requires `nk >= 1`.
- (v) Fresh review 2026-10-16: Fortran object recompiled and checked identical to `build_mixa`'s;
  the routine's GIMPLE read against the C++ unit's: the conversions, `IK <= 1`, the arguments of
  `WN_PRECALC_CHENG` in order (`IK`, `KU` never written by it), `N = NK+2` and the bases of
  `SIG`, `WN_R`, `WN_I`, `CG` passed to `SMOOTH_K` and `CGINIC3_CHENG`; the C++ object calls only
  the callable and `sqrtf` (the negative-input path of `WAVNU1`'s `sqrt`, same bits). No
  defect; header unchanged.
- ⚠ CUDA not compiled: a host routine (its callee prints).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
