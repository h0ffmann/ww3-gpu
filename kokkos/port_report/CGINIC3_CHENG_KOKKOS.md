# CGINIC3_CHENG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A helper of the IC3 sea-ice source term (Cheng's viscoelastic ice layer): the group velocity
`C_g = d sigma / d k_r` by finite differences over the frequency grid, one-sided at the first and
last frequency and the harmonic mean `2 / (1/C_g1 + 1/C_g2)` of the two one-sided slopes inside.
Called by `W3IC3WNCG_CHENG` (`w3sic3md.F90:1953`) and `IC3PRECALC_CHENG` (`:2158`), both after
`SMOOTH_K`.

## What was ported

| | |
|---|---|
| WW3 routine | `CGINIC3_CHENG`, `model/src/w3sic3md.F90:2420-2493` at NOAA-EMC/WW3 `03adda91`; compiled under switch `IC3`, by 46 regtest switch files (`ww3_tic1.1`, `ww3_tic1.2`, `ww3_tic1.3`, `ww3_ts1`; `git grep -lw IC3 -- '*switch*'`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`). No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/ic3_cginic3_cheng.hpp`](../src/ww_kokkos/ic3_cginic3_cheng.hpp), `ww::ic3::cginic3_cheng`; the automatic array `CG0` (`:2479`) is never used and is absent from the object, so no work array |
| C shim | none yet; its callers are not ported |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3sic3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object; its GIMPLE (`-fdump-tree-optimized-lineno`) and disassembly read
against those of a throwaway C++ unit compiled with the `openmp-release` flags and
`-ffp-contract=off`, with and without `__restrict`, 2026-10-16. The routine is not inlined into
its callers in the Fortran object.

- (v) The end points (`:2483`, `:2485`) are one `subss`, one `subss`, one `divss`
  (`dsigma / dk`) each, stored `CG(1)` first, then `CG(N)`, in both objects.
- (v) Inside (`:2487-2489`): two quotients, `1.0 / CG2`, `1.0 / CG1`, one add, `2.0 / sum`; true
  divisions in both (no reciprocal), no FMA. The Fortran object adds with `1./CG2` as the `addss`
  destination although the source reads `1./CG1 + 1./CG2`; the header writes
  `one / cg2 + one / cg1` and the C++ object then has `1/cg2` as the first source in the
  `__restrict` loop, the vectorised body, the non-vectorised fallback loop and the second and
  third epilogue steps.
- ⚠ In the C++ unit without `__restrict`, the first scalar epilogue step after the vectorised loop
  (`vaddss` with `1/cg1` as first source) has the operands the other way round. The sum is the
  same for any operands that are not both NaN; when both terms are NaNs of different payload or
  sign (for example `0/0`, which is negative, against a positive input NaN), that one element can
  carry the other NaN. Not fixable from C++ source, since g++ treats `+` as commutative.
- (v) gfortran reuses `CG(1)` as `CG1` for `IK = 2` and carries `SIGMA(IK)`, `WN_R(IK)` across
  iterations; g++ recomputes them. Same operands, same operations, same bits.
- (v) g++ vectorises the inner loop (32-byte, 16-byte epilogue, versioned for aliasing); the
  Fortran object does not. Every lane does the same element-wise `subps`/`divps`/`addps` as the
  scalar code, with no reduction, so the bits do not depend on the width.
- (v) No libm or libmvec call, no reduction in either object; the C++ unit compiles warning-free.
- ⚠ For `N = 1` the Fortran reads `SIGMA(2)`, `WN_R(2)` outside the arrays (`:2483`); the port
  documents `n >= 2` as the caller's precondition. Both callers pass `KU-KL+1`, the number of
  frequencies.
- ⚠ Where `WN_R(IK+1) = WN_R(IK)` the quotient is an infinity or a NaN and the harmonic mean
  propagates it; `SMOOTH_K` runs first to remove such points but does not guarantee it. Reproduced
  as written.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: the Fortran module recompiled from `build_mixa`'s `flags.make`
  and found identical to the CMake object; its GIMPLE and disassembly read against a C++ unit
  (with and without `__restrict`) site by site: end points, quotients, true divisions, constants
  `1.0`/`2.0`, no FMA, no libm call, no reduction, argument order, `n >= 2`. Found that the claim
  of `1/cg2` first at every C++ add site was wrong for one epilogue step; the claim was corrected
  and the NaN-only difference marked ⚠ above. No arithmetic change to the header.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
