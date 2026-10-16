# SMOOTH_K_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A helper of the IC3 sea-ice source term (Cheng's viscoelastic ice layer): after the complex
wavenumber `k = k_r + i k_i` is solved per frequency, `SMOOTH_K` marks the frequencies where
`k_r` is not increasing, jumps by more than three times the previous step, or sits at the mode
switch `SWITCHID`, and replaces `k_r` and `k_i` there by linear interpolation in `sigma`, three
passes; then it extrapolates the last frequency when it jumps upward. Called by
`W3IC3WNCG_CHENG` (`w3sic3md.F90:1946`, `SWITCHID = 0`) and `IC3PRECALC_CHENG` (`:2157`).

## What was ported

| | |
|---|---|
| WW3 routine | `SMOOTH_K`, `model/src/w3sic3md.F90:2497-2535` at NOAA-EMC/WW3 `03adda91`; compiled under switch `IC3`, by 46 regtest switch files (`ww3_tic1.1`, `ww3_tic1.2`, `ww3_tic1.3`, `ww3_ts1/input_icesct_IC3`; `git grep -lw IC3 -- '*switch*'`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`). No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/ic3_smooth_k.hpp`](../src/ww_kokkos/ic3_smooth_k.hpp), `ww::ic3::smooth_k`; the automatic arrays `DIFF`, `REMOVEID` (`:2499`, heap `malloc`/`free` in the object) are caller-provided work arrays of length `n` |
| C shim | none yet; its callers are not ported |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3sic3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object; its GIMPLE (`-fdump-tree-optimized-lineno`) and disassembly read
against those of a throwaway C++ unit compiled with the `openmp-release` flags and
`-ffp-contract=off`, 2026-10-16.

- (v) Both interpolations (`:2519-2522`, `:2529-2532`) are `a + ((b - c) / (d - e)) * (f - g)`
  in both objects, the same `subss`/`divss`/`mulss`/`addss` per line; the divisor and
  `SIGMA(I)-SIGMA(I-1)` are shared between the `WN_R` and `WN_I` lines in the Fortran and in a
  C++ unit with `__restrict` pointers, and recomputed after the `WN_R` store without it (equal
  operands, no change of bits).
- (v) The tests are `DIFF(I) <= 0`, `DIFF(I) > DIFF(I-1)*3.0`, `SWITCHID == I` in that order, in
  both; a NaN `DIFF` fails both comparisons in both.
- (v) The `DIFF` loop (`:2507-2509`) is vectorised in both (16-byte in the Fortran, 32-byte under
  `x86-64-v3` in the C++); it is element-wise subtraction, so the bits do not depend on the width.
- (v) The interpolation loop is sequential in both: `WN_R(I-1)` may be the value just
  interpolated.
- (v) No FMA, no reciprocal, no libm or libmvec call, no reduction in either object; the C++ unit
  compiles warning-free.
- (v) Fresh review 2026-10-16: no defect. The `w3sic3md` object was recompiled from
  `build_mixa`'s flags and found identical to the CMake object; its GIMPLE and assembly were read
  against a C++ unit with and without `__restrict` (operand order of every `-`, `/`, `*`, `+`,
  the folded `3.0`, the three comparisons and their NaN behaviour, the sequential interpolation
  loop, the vectorised `DIFF` loop, no FMA or libm call). Nothing changed in the header; the CSE
  bullet above was made exact.
- ⚠ For `N < 3` the Fortran reads outside `DIFF` and `WN_R` (`:2511`, `:2527-2532`); the port
  documents `n >= 3` as the caller's precondition. Both callers pass `KU-KL+1`, the number of
  frequencies.
- ⚠ CUDA not compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
