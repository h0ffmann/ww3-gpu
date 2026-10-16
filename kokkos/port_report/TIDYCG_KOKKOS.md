# TIDYCG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The closing step of the SPARSKIT `BCGSTAB` solver in `W3PROFSMD`: sets the return code
`IPAR(1)`, the convergence rate `FPAR(7) = log10(FPAR(3)/FPAR(6)) / (IPAR(7)-IPAR(13))`, and
adds the last correction to the solution, `SOL = SOL + DELX`.

## What was ported

| | |
|---|---|
| WW3 routine | `TIDYCG`, `model/src/w3profsmd.F90:2560-2591` at NOAA-EMC/WW3 `03adda91`, private to the module; no `#ifdef` inside it |
| Callers | only `BCGSTAB` (`:2362`, `:2364`), which `RUNRC` drives from `W3XYPFSNIMP` (`:1239`), which `W3XYPUG` calls when `FSNIMP` is set (`:257-258`), the implicit N scheme of unstructured grids selected in `ww3_grid` (`w3gridmd.F90:3724-3726`, `IMPFSN`) |
| Switches that compile it | every switch with `PR1`, `PR2` or `PR3` (`model/bin/build_utils.sh:588-590`), so the lab switch and the NOAA operational `switch_NCEP_glwu`, `_gwm`, `_st2`, `_st4`, `_st4sbs`, `switch_NWS_rwps`. ⚠ A regtest that runs it: `ww3_ts4/input_ug` sets `IMPFSN = T` (`git grep -i "IMPFSN *= *T" -- regtests`); whether that run selects scheme 4 was not checked |
| Kokkos code | [`../src/ww_kokkos/profs_tidycg.hpp`](../src/ww_kokkos/profs_tidycg.hpp), `ww::profs::tidycg`; `IPAR`, `FPAR` as 0-based `[16]` arrays, `REAL*8` as `double` |
| C shim | none yet |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` from WW3's CMake Release build under
`switch_lab_shrd` (gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked
identical to the CMake object; against a throwaway C++ unit with the `openmp-release` flags
(`-ffp-contract=off`), compiled warning-free, 2026-10-16.

- (v) `FPAR(7)`: both objects compute `divsd` of `FPAR(3)/FPAR(6)`, call libm `log10` (double,
  `R_X86_64_PLT32 log10`; no libmvec), convert the 32-bit difference `IPAR(7)-IPAR(13)` with
  `cvtsi2sd` and divide (`divsd`). No FMA, no reciprocal.
- (v) `SOL = SOL + DELX`: gfortran vectorises it with `addpd` (2 lanes), the C++ with `vaddpd`
  (4 and 2 lanes) and a scalar tail; each element is one independent addition, so the bits are
  the same whatever the lane count.
- (v) Branches of `IPAR(1)`: the comparisons and their operands match `:2570-2581`; the
  Fortran `.and.`/`.or.` have no side effects, so evaluation order does not matter.
- No reduction, no `MIN`/`MAX`, no `INT`/`NINT`.
- ⚠ Not compiled for CUDA. On a GPU `Kokkos::log10` is CUDA's, which need not return glibc's bits.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3profsmd.F90` from `build_lab`'s
  `flags.make` (identical to the CMake object) and the header in a throwaway unit; compared
  `__w3profsmd_MOD_tidycg` with the C++ instruction by instruction: same branch tree for
  `IPAR(1)` (`comisd`/`jae` on `FPAR(4)` against `FPAR(6)`, so a NaN takes the same path), same
  `divsd`, `log10` call, 32-bit `sub` then `cvtsi2sd`, `divsd`, and element-wise `SOL + DELX`.
  `TIDYCG` has no `#ifdef`; its two call sites (`:2362`, `:2364`) are merged into one tail call
  in the object, not inlined. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
