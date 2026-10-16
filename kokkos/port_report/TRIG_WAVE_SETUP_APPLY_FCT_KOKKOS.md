# TRIG_WAVE_SETUP_APPLY_FCT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The matrix-vector product `TheOut = ASPAR * TheIn` of the wave setup Poisson solver on a
triangular PDLIB grid, in compressed sparse row form (`PDLIB_IA`, `PDLIB_JA`), over the rows with
`ACTIVESEC = 1`, followed by the halo exchange `PDLIB_exchange1Dreal`; switch `SETUP`.

## What was ported

| | |
|---|---|
| WW3 routine | `TRIG_WAVE_SETUP_APPLY_FCT`, `model/src/w3wavset.F90:1020-1107` at NOAA-EMC/WW3 `03adda91`; called only by `TRIG_WAVE_SETUP_SOLVE_POISSON_NEUMANN_DIR` (`:1331`). No regtest switch and no NOAA operational switch carries `SETUP` (`git grep -lw SETUP -- '*switch*'` finds only `model/src/cmake/switches.json`), so no regtest exercises it. Its only `#ifdef` block is `W3_S` tracing |
| Kokkos code | [`../src/ww_kokkos/wavset_trig_wave_setup_apply_fct.hpp`](../src/ww_kokkos/wavset_trig_wave_setup_apply_fct.hpp), `ww::wavset::trig_wave_setup_apply_fct`; arrays 0-based, `PDLIB_IA`/`PDLIB_JA` values kept 1-based; `REAL(rkind)` is `float` (`PDLIB/yowdatapool.F90:48`). `ACTIVE` is a dummy the Fortran never reads; it is kept in the signature |
| Not translated | `PDLIB_exchange1Dreal` (`:1106`), an MPI halo exchange: the C++ takes it as a caller-provided callable `exchange(theout)`, called at the same point |
| C shim | none yet; it passes `NPA` and yowNodepool's `PDLIB_IA`, `PDLIB_JA`, and wraps the exchange |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3wavset.F90` in WW3's CMake Release build with
`regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP` (`DIST MPI PDLIB SCOTCH`), gfortran 16.2;
the module recompiled with that build's `flags.make` is identical to the CMake object. Compared
with a C++ unit at the openmp-release flags (g++ 16.2), 2026-10-16.

- (v) Both objects: `memset` of `TheOut` when `NPA > 0`, then for each `IP` with
  `ACTIVESEC(IP) == 1` a scalar accumulator started from `TheOut(IP)`, `TheIn(JP)*ASPAR(J)`
  (`mulss`, `TheIn` the first operand in both), `acc + product` (`addss`, the accumulator first in
  both), in `J` order; the Fortran object stores `TheOut(IP)` once per row, the C++ object after
  every `J` (it cannot prove `theout` does not alias `thein`), the same float each time; then the
  exchange call, also when `NPA <= 0`. Neither vectorised; no FMA, no libm call.
- (v) Fresh review 2026-10-16: no arithmetic defect. The module recompiled from `ref3/build_setup`'s
  `flags.make` is identical to the CMake object; its assembly read against the C++ unit's: `memset`
  under `NPA > 0`, the `ACTIVESEC(IP) == 1` test, the row bounds `PDLIB_IA(IP)..PDLIB_IA(IP+1)-1`
  (`PDLIB_IA(npa+1)`, `PDLIB_JA(PDLIB_NNZ)`, `PDLIB/yowpdlibmain.F90:1581`), `mulss` with `TheIn(JP)`
  first and `addss` with the accumulator first, and the tail call of
  `PDLIB_exchange1Dreal`. Fixed in this report: the C++ object stores the row sum after every `J`,
  not once per row (same bits).
- ⚠ CUDA not compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
