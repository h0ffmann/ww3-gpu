# USOLC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Solves `U x = y` with `U` unit upper triangular, stored in compressed sparse column form (`au`,
`jau`, `iau`, diagonal implied 1, not stored): `x` is set to `y`, then for each column `k` from `n`
down to 1 the value `x(k)` is subtracted, times the column's entries, from the rows they name.
A SPARSKIT routine kept in `w3profsmd.F90`.

## What was ported

| | |
|---|---|
| WW3 routine | `usolc`, `model/src/w3profsmd.F90:3581-3624` at NOAA-EMC/WW3 `03adda91`, 44 lines; no `#ifdef` inside it |
| Callers | none: `git -C WW3-noaa grep -nw usolc -- model/src` finds only the `PUBLIC` list (`w3profsmd.F90:64`), the module's index comment (`:2851`) and the routine itself. Translated for completeness; no WW3 run can exercise it |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:117-125`), so by nearly every switch file; compiled is not called |
| Kokkos code | [`../src/ww_kokkos/profs_usolc.hpp`](../src/ww_kokkos/profs_usolc.hpp), `ww::profs::usolc`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based holding the Fortran 1-based indices |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) Same double-precision sequence in both objects: per column `t` loaded once from `x(k)`
  (`:3615`), then per entry `x(jau(j)) = x(jau(j)) - (au(j) * t)` as one `mulsd` then one `subsd`,
  stored back before the next entry is loaded, in increasing `j`, columns from `n` down to 1
  (SSE in the Fortran object, VEX in the C++ one from `-march=x86-64-v3`, the same IEEE operations).
  The product is `au(j) * t` in the Fortran GIMPLE and `t * au[j]` in the C++; IEEE multiplication
  is commutative, so the bits are the same.
- (v) No FMA, no reciprocal, no libm or libmvec call.
- (v) Only the copy loop `x(k) = y(k)` (`:3611-3613`) is vectorised: 16-byte vectors in the
  Fortran object, 32-byte vectors with a runtime alias check in the C++ one. A copy has no
  rounding, so the vector width does not change bits. The elimination loop is scalar in both.
- ⚠ The Fortran assumes `x` and `y` do not overlap (dummy arguments); the C++ handles an overlap
  through its alias check. With distinct arrays, the only legal Fortran call, both agree.
- ⚠ If `jau(j)` equals the column `k` (a diagonal entry stored despite the unit diagonal), `x(k)`
  is updated after `t` was read and the update does not feed back into `t`; reproduced as written.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3profsmd.F90` under `switch_lab_shrd` (object identical to the CMake one) and the header in a throwaway unit; the GIMPLE and assembly of the elimination loop agree operation by operation (one `mulsd`/`vmulsd`, one `subsd`/`vsubsd`, `x(jau(j)) - t*au(j)`, scalar, no FMA), the copy loop only is vectorised; no `#ifdef` in the routine; nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing in WW3 calls `usolc`, so the WW3 regtests cannot exercise it.
