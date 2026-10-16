# FD_COMPUTE_LH_STRESS_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Wave forcing of the wave setup on a regular or curvilinear grid (switch `SETUP`): minus the
differences of the radiation stress between the four `NEIGH` neighbours of each sea point. Its only
caller is `FD_WAVE_SETUP_COMPUTATION` (`w3wavset.F90:3037`, on `IAPROC = 1`), reached from
`WAVE_SETUP_COMPUTATION` (`:3159`) when `GTYPE` is not `UNGTYPE`, itself called from `W3WAVE`
(`w3wavemd.F90:1124`).

## What was ported

| | |
|---|---|
| WW3 routine | `FD_COMPUTE_LH_STRESS`, `model/src/w3wavset.F90:2281-2418` at NOAA-EMC/WW3 `03adda91`; compiled by no regtest switch and no NOAA operational switch (`git grep -lw SETUP -- '*switch*'` finds only `model/src/cmake/switches.json`). Its only `#ifdef` block is `W3_S` tracing; no MPI or PDLIB path inside it |
| Kokkos code | [`../src/ww_kokkos/wavset_fd_compute_lh_stress.hpp`](../src/ww_kokkos/wavset_fd_compute_lh_stress.hpp), `ww::wavset::fd_compute_lh_stress_point` (one `ISEA`, 1-based) and the host loop `ww::wavset::fd_compute_lh_stress`; `NEIGH(NSEA,4)` column-major with its leading dimension; module `SXX`, `SXY`, `SYY` and the never-assigned locals `dist_X`, `dist_Y` passed as inputs |
| C shim | none yet; to match the object it passes `dist_x = dist_y = 0.0f` |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3wavset.F90` (WW3's CMake Release flags, gfortran 16.2,
`regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP`, `ref3/build_setup`), recompiled and found
identical to the CMake object, against a throwaway C++ unit at the openmp-release flags (no
warning), 2026-10-16.

- (v) Same control flow and operations in the GIMPLE of both: two `memset` for `FX = 0`, `FY = 0`;
  per point, the same three-way `JSEA1`/`JSEA2` choice and two-way `JSEA3`/`JSEA4` choice, one
  `subss` and one `divss` per surviving assignment of `SXX_X` and `SXY_X`, then two sign flips
  (`xorps` with the sign mask) for `eFX = -SXX_X - SXY_Y` and `eFY = -SYY_Y - SXY_X`, which both
  compilers fold to `-SXX_X` and `-SXY_X` since `SXY_Y = SYY_Y = 0`.
- (v) Dead code dropped identically by both: every `SYY_X` (never used) and the whole block of
  `:2398-2402`, which `:2408-2412` overwrites under the same condition.
- ⚠ `dist_X`, `dist_Y` are declared (`:2357`) and never assigned. gfortran materialises both as
  `+0.0` (`pxor %xmm2,%xmm2` before each block) and divides by that register directly, so
  `2*dist_X` is `+0.0` too. Every derivative is `±Inf` or the default NaN, and `FX`, `FY` are
  that value negated. The C++ takes `dist_x`, `dist_y` as inputs (reading an uninitialised `float`
  is undefined in C++); with `0.0f` it computes `(a-b)/(0.0f+0.0f)` and `(a-b)/0.0f`, the same
  bits as the object, including the NaN sign after the flip.
- ⚠ WW3 defects reproduced as written: `SYY_X` subtracts `SYY` from `SXY` (`:2386`, ...); the
  `y` differences are stored in `SXX_X`, `SXY_X`, `SYY_X` (`:2398-2412`), so `SXY_Y`, `SYY_Y` stay
  0; the condition of `:2408` repeats `:2398` where `JSEA4 .eq. 0` was meant, so `JSEA3 > 0,
  JSEA4 = 0` keeps the `x` value; the dummies `SXX_t`, `SXY_t`, `SYY_t`, gathered over all ranks
  by `FD_COLLECT_SXX_XY_YY` (`:3035`), are never read, and the module `SXX`, `SXY` (allocated
  `NSEALM`, `w3adatmd.F90:1187`, holding the local points) are indexed by the global `ISEA`: with
  more than one rank this reads other points and past the end of the array.
- (v) No FMA, no reduction, no `MIN`/`MAX`, no libm or libmvec call; neither object vectorises
  the loop.
- ⚠ CUDA not compiled this batch.
- (v) Fresh review 2026-10-16: no arithmetic defect. Read in the recompiled object (identical to
  the CMake object): `dist_X`, `dist_Y` are `dist_x_307(D)`, `dist_y_314(D)` (undefined) in the
  GIMPLE and a `pxor %xmm2,%xmm2` before every block in the assembly, `2*dist_X` included, so the
  divisor is `+0.0`; the C++ with `0.0f` divides by `+0.0` (`vaddss` of zero for the centred case),
  same branches and same operands (`:2408` overwrites `:2398`), `-SXX_X`, `-SXY_X` by `xorps` in
  both. Fixed the declaration line of `dist_X` from `:2358` to `:2357` here and in the header
  comment. The `+0.0` is a property of this compile of an undefined read, not of the source.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest switch compiles `SETUP`, so no regtest can exercise it.
