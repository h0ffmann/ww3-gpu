# GAULEG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Abscissas and weights of the `NPOIN`-point Gauss-Legendre quadrature on [-1, 1], found by Newton
iteration on the three-term Legendre recurrence. `INSNLGQM` calls it once per grid
(`w3snl1md.F90:1834`) to build the `OM2` quadrature of the Gaussian Quadrature Method (GQM) form
of `NL1`, which `W3IOGR` selects on reading a `mod_def` when `IQTPE <= 0`
(`w3iogrmd.F90:1799-1806`).

## What was ported

| | |
|---|---|
| WW3 routine | `GAULEG`, `model/src/w3snl1md.F90:1310-1346` at NOAA-EMC/WW3 `03adda91`, adapted from TOMAWAC. Compiled wherever `NL1` is (392 switch files under `git grep -lw NL1 -- '*switch*'`; NOAA operational `switch_NCEP_gwm`, `switch_NCEP_st2`, `switch_NCEP_st4`, `switch_NCEP_st4sbs`, `switch_NWS_rwps`); reached only with `IQTYPE <= 0` in the `SNL1` namelist (`w3gridmd.F90:1923`, `w3iogrmd.F90:1801`) (for example `regtests/ww3_ts1/input/namelists_ST4_T707.nml`). No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/snl1_gauleg.hpp`](../src/ww_kokkos/snl1_gauleg.hpp), `ww::snl1::gauleg`, an `inline` host function (a table built once) |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3snl1md.F90` built with the flags of WW3's CMake Release build
under `switch_lab_shrd` (gfortran 16.2.0, pinned shell), identical by disassembly to the CMake
object; compared with a throwaway C++ unit under the `openmp-release` flags, 2026-10-16.

- (v) `PI = ACOS(-1.)` (`:1324`) is a default-REAL `ACOS`: gfortran folds it to
  3.1415927410125732421875, the float nearest pi widened to double, not the double pi. The header
  writes it as `0x1.921fb6p+1` with a `static_assert` of how it is built.
- (v) Every double operation of `:1327`, `:1334`, `:1336-1339` and `:1341-1344` is the same in
  both optimised GIMPLE dumps, in the same order and association: `Z**2` and `PP**2` are `z*z` and
  `pp*pp`, the weight is `2/((1-z*z)*(pp*pp))`; `2.D0*DBLE(J)` is an exact `addsd` in both objects.
  No FMA, no vectorised loop in either.
- (v) One libm call per abscissa, glibc's scalar `cos` (double), in both objects; `ABS` is an
  `andpd`.
- (v) Store order kept: `X_LEG(I)`, `X_LEG(NPOIN+1-I)`, then the weight to both ends; for odd
  `NPOIN` the middle abscissa ends as `+Z`, as in the Fortran.
- ⚠ The Newton loop (`:1328-1340`) has no iteration cap in the Fortran and none here: an input
  that does not converge to `3.D-14` hangs both.
- CUDA: not compiled; the function is host only.
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3snl1md.F90` under `build_lab` flags (disassembly identical to the CMake object) and a C++ unit under the `openmp-release` flags; the optimised GIMPLE of `GAULEG` and of `ww::snl1::gauleg` match statement by statement (operand order, `2*dj` as a multiply in both, `z*z`, `pp*pp`, the folded float pi, `3.D-14`), one `cos` relocation in each object, no vectorised loop in either. Fixed the report: the routine is reached with `IQTYPE <= 0`, not `< 0`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
