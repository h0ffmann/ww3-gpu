# FD_WAVE_SETUP_APPLY_PRECOND_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The preconditioner of the conjugate gradient of the wave setup Poisson solver on a PDLIB grid,
switch `SETUP`. As WW3 is written it is the identity: `TheOut = TheIn`.

## What was ported

| | |
|---|---|
| WW3 routine | `FD_WAVE_SETUP_APPLY_PRECOND`, `model/src/w3wavset.F90:2039-2139` at NOAA-EMC/WW3 `03adda91`; called only by `FD_WAVE_SETUP_SOLVE_POISSON_NEUMANN_DIR` (`:2822`, `:2841`). No regtest switch and no NOAA operational switch carries `SETUP` (`git grep -lw SETUP -- '*switch*'` finds only `model/src/cmake/switches.json`), so no regtest exercises it. Its only `#ifdef` block is `W3_S` tracing |
| Kokkos code | [`../src/ww_kokkos/wavset_fd_wave_setup_apply_precond.hpp`](../src/ww_kokkos/wavset_fd_wave_setup_apply_precond.hpp), `ww::wavset::fd_wave_setup_apply_precond`; arrays 0-based, `PDLIB_IA`/`PDLIB_JA`/`PDLIB_I_DIAG` values kept 1-based; `REAL(rkind)` is `float` (`PDLIB/yowdatapool.F90:48`) |
| C shim | none yet; it passes `NSEA` and yowNodepool's `PDLIB_IA`, `PDLIB_JA`, `PDLIB_I_DIAG` |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3wavset.F90` in WW3's CMake Release build with
`regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP` (`DIST MPI PDLIB SCOTCH`), gfortran 16.2;
the module recompiled with that build's `flags.make` gives an identical disassembly. Compared
with a C++ unit at the openmp-release flags (g++ 16.2, no warning), 2026-10-16.

- (v) `ThePrecond` (`:2109`) is a local with an initialiser, so implicitly `SAVE`, and no
  statement assigns it. gfortran's optimised GIMPLE keeps only `TheOut=TheIn` (`:2114`): a
  `memcpy` of `4*NSEA` bytes when `NSEA > 0`, nothing else. The C++ is a `constexpr` 0 and
  `if constexpr`; g++ emits an element copy (`vmovups`/`vmovss`). Both are bit moves with no
  arithmetic, so the bits of `TheOut` are the bits of `TheIn`.
- ⚠ Branches 1 (`:2116-2131`, a Jacobi-like preconditioner from the CSR matrix) and 2
  (`:2132-2138`, diagonal scaling) are translated in Fortran order but are dead in both objects,
  so there is no object to compare them against. Branch 1 loops `IP=1,NSEA` over `PDLIB_IA`,
  which is indexed by local node, not by sea point; as the Fortran is dead, it was not checked.
- The `memcpy` assumes `TheIn` and `TheOut` do not overlap; both callers pass the distinct
  arrays `V_R`, `V_Z`.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. Object recompiled from `ref3/build_setup` flags identical to the CMake object; its optimised GIMPLE for this routine is only the `NSEA > 0` test and a `memcpy` of `4*NSEA` bytes, and the C++ unit is a vectorised element copy with no arithmetic. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
