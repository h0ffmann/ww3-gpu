# FD_WAVE_SETUP_SCALAR_PROD_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The dot product `eScal = SUM(V1*V2)` over the `NX` points of a finite-difference grid, used by the
preconditioned conjugate gradient of the wave setup Poisson solver
(`FD_WAVE_SETUP_SOLVE_POISSON_NEUMANN_DIR`, `w3wavset.F90:2824`, `:2828`, `:2836`, `:2842`).

## What was ported

| | |
|---|---|
| WW3 routine | `FD_WAVE_SETUP_SCALAR_PROD`, `model/src/w3wavset.F90:2655-2729` at NOAA-EMC/WW3 `03adda91`. Reached from `W3WAVE` (`w3wavemd.F90:1124`, under `W3_SETUP`) through `WAVE_SETUP_COMPUTATION` and `FD_WAVE_SETUP_COMPUTATION` (`w3wavset.F90:3159`, when `GTYPE /= UNGTYPE`). No regtest switch and no NCEP/NWS operational switch has `SETUP` (`git grep -lw SETUP -- '*switch*'` finds only `model/src/cmake/switches.json`), so no regtest exercises it. Its only `#ifdef` is `W3_S` (tracing) |
| Kokkos code | [`../src/ww_kokkos/wavset_fd_wave_setup_scalar_prod.hpp`](../src/ww_kokkos/wavset_fd_wave_setup_scalar_prod.hpp), `ww::wavset::fd_wave_setup_scalar_prod(v1, v2, escal, nx)`; `nx` is the module variable `W3GDATMD NX` passed in |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3wavset.F90` in WW3's CMake Release build (gfortran 16.2,
`regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP`), recompiled from the build's `flags.make`
with the disassembly identical to the CMake object, and a throwaway C++ unit with the
openmp-release flags (g++ 16.2, `-march=x86-64-v3 -ffp-contract=off`), 2026-10-16.

- (v) `REAL(rkind)` is `float`: `rkind = 4` (`PDLIB/yowdatapool.F90:48`).
- (v) Reduction order: gfortran vectorises the products (`mulps`, 4 wide) but adds them to the
  accumulator one at a time in `IP` order (`addss` chain, no partial sums); the C++ object does
  the same with 8-wide `vmulps` and a `vaddss` chain in index order. The sum starts from `+0` in
  both. Each product is one rounded multiply, so every finite or infinite result has the same bits.
- (v) No FMA, no libm or libmvec call in either object; the routine is not inlined into its caller
  in the Fortran object (four `call`s in the GIMPLE of the solver).
- ⚠ Operand order of the multiply: the Fortran object computes `V1*V2` in its vector body and
  tail, but `V2*V1` in its short-loop path (`NX <= 3`); the C++ object computes `v2*v1` in its
  vector body. The product differs only in which NaN payload survives when both operands are NaN.
- (v) Fresh review 2026-10-16: no defect beyond the NaN-payload operand order above, which
  the Fortran object itself does not keep across its own paths and which C++ source order does not
  control. The module recompiled from `ref3/build_setup`'s `flags.make` is identical to the CMake
  object; its assembly read against the C++ unit's: `+0` start (`eScal = 0` stored as `+0` when
  `NX <= 0`), one `mulss`/`mulps` lane per product, one `addss` per product in `IP` order in the
  vector body, the epilogue and the tail of both objects; 4 calls of the routine remain in the
  solver's object.
- ⚠ CUDA not compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
