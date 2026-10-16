# CALC_ND_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The factor `ND` of the Ng (2000) fluid-mud dissipation (switch `BT9`): with `kd = KWAVE*H_WDEPTH`,
`ND = (2kd/sinh(2kd)) * (0.5/d - k/FAC3)`, `FAC2 = SND2/kd`, `FAC3 = 2*FAC2/(1+FAC2^2)`. Its only
caller is `W3SBT9`, for the rock and the mud wave numbers (`w3sbt9md.F90:307`, `:355`). The
Fortran documents neither its meaning nor its author (`:558-570`, "NA") ⚠.

## What was ported

| | |
|---|---|
| WW3 routine | `CALC_ND`, `model/src/w3sbt9md.F90:571-590` at NOAA-EMC/WW3 `03adda91`; compiled by the regtest switches of `ww3_tbt1.1` and `ww3_tbt2.1` (`switch_BT9`, `switch_BT9_MPI`; `git grep -lw BT9 -- '*switch*'`), by no NOAA operational switch. No `#ifdef` in the routine |
| Kokkos code | [`../src/ww_kokkos/bt9_calc_nd.hpp`](../src/ww_kokkos/bt9_calc_nd.hpp), `ww::bt9::calc_nd` (`ND` by reference, `INTENT(OUT)`) |
| C shim | none yet; it is called from inside `W3SBT9`, so it enters WW3 with the port of `W3SBT9` |
| Plan | [`EASY_BATCH_KOKKOS_PLAN_202610.md`](../../docs/EASY_BATCH_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3sbt9md.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd` with `NL4 IC5 BT9 NLS`), recompiled and checked identical to the CMake object,
against a C++ unit with the `openmp-release` flags, 2026-10-16.

- (v) Same scalar operations in the same order in both objects: `k*d`, `kd+kd` (gfortran's `2.*X`,
  exact), `sinhf`, `SND2/kd`, `FAC2+FAC2`, `FAC2*FAC2`, `+1`, `0.5/d`, the three divisions, the
  subtraction and the final product; constants `0.5f` and `1.0f` in both `.rodata`.
- (v) `SINH` is glibc's scalar `sinhf` in both objects; no libmvec call, no FMA, no reduction, no
  MIN/MAX, not vectorised. `CALC_ND` is not inlined into `W3SBT9` in the Fortran object.
- ⚠ No guard against `kd = 0` or `FAC3 = 0` (`SND2 = 0`), as in the Fortran: a division by zero
  gives `Inf`/`NaN` the same way in both.
- ⚠ On a device `sinh` is CUDA's, not shown to return glibc's bits. CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. The Fortran object was recompiled from `build_mixb`'s flags (identical to the CMake object) and its `-O3` GIMPLE and assembly read against a fresh C++ unit operation by operation: the same 14 scalar operations in the same order with the same operands (`kd+kd`, `sinhf`, the four divisions, `-`, `*`), `0.5f`/`1.0f` in `.rodata`, no `#ifdef` in `:571-590`, both call sites (`:307`, `:355`) out of line. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
