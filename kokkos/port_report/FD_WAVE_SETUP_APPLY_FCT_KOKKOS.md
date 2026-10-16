# FD_WAVE_SETUP_APPLY_FCT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The matrix-vector product `TheOut = ASPAR * TheIn` of the wave setup Poisson solver on a PDLIB
grid, in compressed sparse row form (`PDLIB_IA`, `PDLIB_JA`), switch `SETUP`.

## What was ported

| | |
|---|---|
| WW3 routine | `FD_WAVE_SETUP_APPLY_FCT`, `model/src/w3wavset.F90:1945-2026` at NOAA-EMC/WW3 `03adda91`; called only by `FD_WAVE_SETUP_SOLVE_POISSON_NEUMANN_DIR` (`:2827`). No regtest switch and no NOAA operational switch carries `SETUP` (`git grep -lw SETUP -- '*switch*'` finds only `model/src/cmake/switches.json`), so no regtest exercises it. Its only `#ifdef` block is `W3_S` tracing |
| Kokkos code | [`../src/ww_kokkos/wavset_fd_wave_setup_apply_fct.hpp`](../src/ww_kokkos/wavset_fd_wave_setup_apply_fct.hpp), `ww::wavset::fd_wave_setup_apply_fct`; arrays 0-based, `PDLIB_IA`/`PDLIB_JA` values kept 1-based; `REAL(rkind)` is `float` (`PDLIB/yowdatapool.F90:48`) |
| C shim | none yet; it passes `NSEA` and yowNodepool's `PDLIB_IA`, `PDLIB_JA` |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3wavset.F90` in WW3's CMake Release build with
`regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP` (`DIST MPI PDLIB SCOTCH`), gfortran 16.2;
the module recompiled with that build's `flags.make` gives the same instructions for this
routine. Compared with a C++ unit at the openmp-release flags (g++ 16.2), 2026-10-16.

- (v) Both objects: `memset` of `TheOut` when `NSEA > 0`, then per `IP` a scalar accumulator
  started from `TheOut(IP)`, `TheIn(JP)*ASPAR(J)` (`mulss`, `TheIn` the first operand in both),
  `acc + product` (`addss`), in `J` order. Neither vectorised; no FMA, no libm call.
- The C++ stores the accumulator every iteration (aliasing), the Fortran once per row; the values
  stored are the same.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. The module recompiled from `build_setup`'s
  `flags.make` gives the same instructions as the CMake object; the C++ unit's loop is the same
  `mulss` (`TheIn(JP)` first, then `ASPAR(J)`) and `addss` into a per-row accumulator in `J` order,
  no vectorisation, no FMA, no branch on a float; the only `#ifdef` in `:1945-2026` is `W3_S`.
  Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
