# TABU_ERF_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The table of the error function the SHOWEX bottom friction interpolates (`ERFTABLE`, `DELXERF`), built once per process: a host function.

## What was ported

| | |
|---|---|
| WW3 routine | `TABU_ERF`, `model/src/w3sbt4md.F90:242-312` at NOAA-EMC/WW3 `03adda91`; its only `#ifdef` is `W3_S`. `BT4` is in the `ww3_tp2.17` switches and `switch_RWPS` |
| Kokkos code | [`../src/ww_kokkos/srce_bt4.hpp`](../src/ww_kokkos/srce_bt4.hpp), with the other two routines of `W3SBT4MD` |
| C shim | none yet; it selects `SBTCX` per grid (`MPARS(IMOD)`) and reads `DELAB` and `FWTABLE` at call time, since each mod_def read rewrites them (`w3iogrmd.F90:1265`) |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch B, issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3sbt4md.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd` with `BT1` replaced by `BT4`), identical to the CMake build's, read by the
translator and a fresh reviewer, 2026-10-16.

- (v) Folded constants with the object's bits (`2*GRAV*(SED_SG-1)`, `(SED_SG-1)*GRAV`, `SQRT(TPI)`,
  `XSUB(2)-XSUB(1)`, `DELXERF`); `erf(-4)` folded to -1 by both; the table `i*dx - 4` with the integer
  `2**(1/2) = 1`; libm calls matched by relocation (`erff` 1, `expf` 1, `log10f` 2, `powf` 4 with the
  REAL exponent 1.4, `sinhf` 1); every operation in order; no FMA.
- (v) All 18 `MIN`/`MAX` sites with the object's operand order and NaN behaviour, the three with the
  constant first (`:474,527,610`) included; the five run-time branches.
- (v) `INT()` (`:539,584,611`) through `fint` (`st4_params.hpp`), gfortran's `cvttss2si`: the review found
  that a plain C++ cast let g++ delete the `MAX(...,0)` clamp at `:584` (reading `ERFTABLE(INT_MIN)`
  where the Fortran reads `ERFTABLE(0)` for `PSIX >= 5.7e7`), and CUDA saturates to 299. With `fint`
  the object keeps the range check and gives `INDE = 0` as the Fortran does (v, read from the object).
- (v) The table built once per process is right: `FIRST` is `SAVE`, `ERFTABLE` and `DELXERF` are module
  variables that depend on no grid.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`), never run; CUDA's `erff`, `expf`, `log10f`, `powf`, `sinhf`
  are not shown to return glibc's bits.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
