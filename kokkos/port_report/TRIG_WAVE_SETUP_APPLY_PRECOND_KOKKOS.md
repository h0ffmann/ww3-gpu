# TRIG_WAVE_SETUP_APPLY_PRECOND_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The preconditioner of the conjugate gradient of the wave setup Poisson solver on a triangular
PDLIB grid, followed by the halo exchange `PDLIB_exchange1Dreal`; switch `SETUP`. As WW3 is
written it is a diagonal scaling: `TheOut(IP) = TheIn(IP)/ASPAR(PDLIB_I_DIAG(IP))` where
`ACTIVESEC(IP) = 1`, `TheOut(IP) = TheIn(IP)` elsewhere.

## What was ported

| | |
|---|---|
| WW3 routine | `TRIG_WAVE_SETUP_APPLY_PRECOND`, `model/src/w3wavset.F90:892-1005` at NOAA-EMC/WW3 `03adda91`; called only by `TRIG_WAVE_SETUP_SOLVE_POISSON_NEUMANN_DIR` (`:1307`, `:1349`). No regtest switch and no NOAA operational switch carries `SETUP` (`git grep -lw SETUP -- '*switch*'` finds only `model/src/cmake/switches.json`), so no regtest exercises it. Its only `#ifdef` block is `W3_S` tracing |
| Kokkos code | [`../src/ww_kokkos/wavset_trig_wave_setup_apply_precond.hpp`](../src/ww_kokkos/wavset_trig_wave_setup_apply_precond.hpp), `ww::wavset::trig_wave_setup_apply_precond`; arrays 0-based, `PDLIB_IA`/`PDLIB_JA`/`PDLIB_I_DIAG` values kept 1-based; `REAL(rkind)` is `float` (`PDLIB/yowdatapool.F90:48`) |
| Not translated | `PDLIB_exchange1Dreal` (`:1004`), an MPI halo exchange: the C++ takes it as a caller-provided callable `exchange(theout)`, called at the same point |
| C shim | none yet; it passes `NPA` and yowNodepool's `PDLIB_IA`, `PDLIB_JA`, `PDLIB_I_DIAG`, and wraps the exchange |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3wavset.F90` in WW3's CMake Release build with
`regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP` (`DIST MPI PDLIB SCOTCH`), gfortran 16.2;
the module recompiled with that build's `flags.make` is identical to the CMake object. Compared
with a C++ unit at the openmp-release flags (g++ 16.2, no warning), 2026-10-16.

- (v) `ThePrecond` (`:967`) is a local with an initialiser 2, so implicitly `SAVE`, and no
  statement assigns it. gfortran's optimised GIMPLE keeps only branch 2 (`:994-1003`). Both
  objects: per `IP` in order, load `TheIn(IP)`, test `ACTIVESEC(IP) == 1`, then either one
  `divss` of `TheIn(IP)` by `ASPAR(PDLIB_I_DIAG(IP))` (dividend and divisor the same in both) or
  a plain store of `TheIn(IP)`; then the exchange as a tail call. Neither vectorised; no FMA, no
  libm call.
- ⚠ Branches 0 (`:971-973`, identity) and 1 (`:974-993`, a Jacobi-like preconditioner from the
  CSR matrix over `ACTIVE`/`ACTIVESEC` rows) are translated in Fortran order but are dead in both
  objects, so there is no object to compare them against.
- `ACTIVE`, `PDLIB_IA` and `PDLIB_JA` are read only by the dead branch 1; they stay in the
  signature.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. The Fortran object recompiled from the `build_setup`
  flags (identical to the CMake object) and a new C++ unit have the same GIMPLE: branch 2 only,
  per `IP` load `TheIn(IP)`, test `ACTIVESEC(IP) == 1`, one division `TheIn(IP)/ASPAR(J)` or a
  copy, then the exchange as a tail call. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
