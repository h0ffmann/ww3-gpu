# FD_WAVE_SETUP_COMPUTE_SYSTEM_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Assembles the matrix `ASPAR` and the right-hand side `B` of the wave setup Poisson problem on
finite-difference grids, edge by edge: for each edge, the mean radiation-stress force
`(eFX, eFY)` projected on the edge's unit vector, times the edge length, goes into `B` at each of
the two sea points, and the products of the two endpoints' unit vectors go into `ASPAR`. Its only
caller is `FD_WAVE_SETUP_COMPUTATION` (`w3wavset.F90:3041`, on `IAPROC == 1`), reached from
`WAVE_SETUP_COMPUTATION` when `GTYPE /= UNGTYPE`, itself called from `W3WAVE`
(`w3wavemd.F90:1124`, under `W3_SETUP`).

## What was ported

| | |
|---|---|
| WW3 routine | `FD_WAVE_SETUP_COMPUTE_SYSTEM`, `model/src/w3wavset.F90:2539-2642` at NOAA-EMC/WW3 `03adda91`; compiled by no regtest switch and no NOAA operational switch (`git grep -lw SETUP -- '*switch*'` finds only `model/src/cmake/switches.json`), so no regtest exercises it. Its only `#ifdef` is `W3_S` (tracing); no MPI or PDLIB path |
| Kokkos code | [`../src/ww_kokkos/wavset_fd_wave_setup_compute_system.hpp`](../src/ww_kokkos/wavset_fd_wave_setup_compute_system.hpp), `ww::wavset::fd_wave_setup_compute_system`; calls `ww::wavset::fd_compute_diff` ([`FD_COMPUTE_DIFF_KOKKOS.md`](FD_COMPUTE_DIFF_KOKKOS.md)). `ASPAR(PDLIB_NNZ)`, `B(NX)`, `FX(NSEA)`, `FY(NSEA)` 0-based; `PDLIB_NNZ`, `NX`, `NBEDGE` and the arrays `FD_COMPUTE_DIFF` reads (`EDGES`, `MAPSF`, `XGRD`, `YGRD`) passed in |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3wavset.F90` in WW3's CMake Release build (gfortran 16.2,
`regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP`), recompiled from the build's `flags.make`
with the disassembly identical to the CMake object, its GIMPLE and its `-S` listing, against a
throwaway C++ unit at the openmp-release flags (g++ 16.2, `-march=x86-64-v3 -ffp-contract=off`),
2026-10-16.

- (v) `REAL(rkind)` is `float` (`rkind = 4`, `PDLIB/yowdatapool.F90:48`). No FMA, no libm call in
  either object. `ASPAR` and `B` are zeroed over `PDLIB_NNZ` and `NX` (`memset` in the Fortran).
- (v) `eFX = (FX(ISEA)+FX(JSEA))/2.0` is an `addss` then a `mulss` by `0.5` in both objects (the
  division by two is exact as a multiply); the same for `eFY`.
- ⚠ WW3 defect, `w3wavset.F90:2638`: `eFact` is never assigned. The GIMPLE reads it as the
  undefined SSA value `efact_118(D)`, and gfortran 16.2 at `-O3` materialises it as the constant
  `+0.0` (`.LC0 = .LC1+4`, a zero word of the sign-mask constant). The port uses `+0.0f`, so
  `ASPAR` gets `escal*0.0f`: `+0`, `-0` or NaN, never a finite non-zero value. Another compiler or
  other flags may give a different value: this matches this build only.
- ⚠ WW3 defect, `:2638`: the update goes to `ASPAR(J)` with `J = 1, 2`, not to the sparse position
  of the pair `(ISEAREL, JSEAREL)`, so only `ASPAR(1:2)` are ever touched. Reproduced as written.
- ⚠ `:2632` indexes `B`, dimensioned `NX`, with a sea index `ISEAREL`; reproduced as written.
- (v) `eDep` (`:2625`) and `dist2` are never used; the Fortran object never loads `DW`, so `DW` is
  not an argument of the port. `eLen`, `POS_TRICK` and the other unused locals are dropped.
- (v) The Fortran calls `FD_COMPUTE_DIFF` out of line four times per edge; g++ inlines it, sees
  that the four calls read the same edge and computes `sqrtss` and the two `divss` once per edge,
  then selects `u` or `-u` per endpoint. The values are the same operations on the same inputs,
  so the same bits. g++ also packs the two `J` updates of `ASPAR` into 2-wide `vmulps`/`vaddps`,
  lane by lane the same IEEE operations.
- ⚠ Operand order, NaN payloads only: in `B`'s `eScal` the Fortran adds `ugrad1*eFX + vgrad1*eFY`
  with the `U` term first, the C++ object with the `V` term first; in `ASPAR`'s `eScal` the Fortran
  multiplies `ugrad1*ugrad`, the C++ `ugrad*ugrad1`. Rewriting the C++ source in the other order
  left the object unchanged. Results differ only in which payload survives when both operands are
  NaN. The final `+ B(...)` and `+ ASPAR(J)` keep the product as first operand in both.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect beyond the NaN-payload operand orders above. The `eFact` claim was checked in a fresh `-S` listing of the module: the only `ASPAR` update multiplies by `.LC0`, `.set .LC0,.LC1+4`, and the second word of `.LC1` is `.long 0`, so `+0.0f`; `0.5` is `.LC2 = 1056964608` (`0x3F000000`). The C++ object keeps the multiply by a zero vector (`vmulps`), so `escal*0` keeps its sign and NaN. Object recompiled from `ref3/build_setup` flags identical to the CMake object. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
