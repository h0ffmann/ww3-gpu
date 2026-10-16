# DELTA_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A helper of the IC3 sea-ice source term: the bin widths of a discretised function, one-sided
differences at both ends and the centred difference halved inside. `W3IC3WNCG_V1` uses it for the
group velocity in ice, `CG_IC3 = DELTA(SIGMA) / DELTA(WN_R)` (`w3sic3md.F90:827`).
`W3IC3WNCG_V1` is called by `W3SIC3` (`w3sic3md.F90:549`) and by `W3WAVE` when the Cheng method
is not used (`w3wavemd.F90:1063`). The routine's own remark (`:1762-1763`) says the Cheng update
does not use it.

## What was ported

| | |
|---|---|
| WW3 routine | `DELTA`, `model/src/w3sic3md.F90:1732-1790` at NOAA-EMC/WW3 `03adda91`; compiled under switch `IC3`, by 46 regtest switch files (`ww3_tic1.1`, `ww3_tic1.2`, `ww3_tic1.3`, `ww3_ts1`; `git grep -lw IC3 -- '*switch*'`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`). No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/ic3_delta.hpp`](../src/ww_kokkos/ic3_delta.hpp), `ww::ic3::delta`; the `ALLOCATABLE` result `DX` (`:1777`, `malloc` in the object) is a caller-provided array of length `nx` |
| C shim | none yet; its caller is not ported |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3sic3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object; its GIMPLE (`-fdump-tree-optimized-lineno`) and disassembly read
against those of a throwaway C++ unit compiled with the `openmp-release` flags and
`-ffp-contract=off`, 2026-10-16.

- (v) The three branches are one `subss` each, `X(IX+1)-X(IX)` at `IX = 1`, `X(IX)-X(IX-1)` at
  `IX = NX`, and `X(IX+1)-X(IX-1)` followed by `mulss` by `0.5` inside: both compilers fold
  `/ 2.` into the exact `* 0.5`, and the Fortran's parentheses (a `PAREN_EXPR`) change nothing
  here. Same operand order in both objects.
- (v) The loop is sequential and scalar in both (the only vectorised statement in the Fortran is
  the array descriptor set-up at `:1777`); no FMA, no reciprocal, no libm or libmvec call, no
  reduction, no MIN/MAX. The C++ unit compiles warning-free.
- (v) `DX = 0.` (`:1778`) is removed as dead in the Fortran object (the result is a fresh
  allocation); in the C++ it stays a `memset`, since `dx` could alias `x`. With `dx` not
  overlapping `x`, the header's documented precondition, the bits are the same.
- ⚠ For `NX = 1` the Fortran reads `X(2)`, outside `X` (`:1782`), in the object as in the source;
  the port documents `nx >= 2` as the caller's precondition. The caller passes `SIGMA` and `WN_R` of
  length `SIZE(CG)`, the number of frequencies (`:755-756`).
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. The module was recompiled from `build_mixa`'s
  `flags.make` and found identical to the CMake object; the `-O3` GIMPLE and disassembly of
  `DELTA` (both the unit-stride and the strided loop versions) were read against the C++ unit's:
  the same three `subss` with the same operands, `mulss` by `0.5` (`0x3f000000`) only in the
  interior branch, the same branch order (`IX == 1` tested before `IX == NX`), no vectorised loop
  in either, the C++ unit warning-free. Added to the header: `x` must be contiguous, since the
  Fortran's assumed-shape `X` also accepts a strided section (its only call site, `:827`, passes
  `SIGMA` and the dummy `WN_R(:)`).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
