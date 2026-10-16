# CALCARRAY_JACOBI_SPECTRAL_1_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The spectral (direction and frequency) part of the PDLIB implicit scheme's matrix: per node, the
diagonal and off-diagonal terms from the frequency shift and refraction velocities, for the block
Gauss-Seidel solver.

## What was ported

| | |
|---|---|
| WW3 routine | `CALCARRAY_JACOBI_SPECTRAL_1`, `model/src/w3profsmd_pdlib.F90:4413-4557` at NOAA-EMC/WW3 `03adda91`, as `WW3/regtests/ww3_tp2.17/input/switch_PDLIB` compiles it; 0.23 % of the attributed time of the PDLIB profile (`docs/data/ww3_pdlib_gprof.tsv`). Its only `#ifdef` is `W3_S` (tracing), in no relevant switch |
| Kokkos code | [`../src/ww_kokkos/pdlib_jacobi_spectral.hpp`](../src/ww_kokkos/pdlib_jacobi_spectral.hpp), `calcarray_jacobi_spectral_1`, one thread per node |
| C shim | none yet. Its `Prop` adapter holds `PropParallArrays` and one work column per node and forwards to [`prop_parall.hpp`](../src/ww_kokkos/prop_parall.hpp) and [`prop_freq_shift.hpp`](../src/ww_kokkos/prop_freq_shift.hpp) (the reviewer compiled one) |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch A, issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3profsmd_pdlib.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_PDLIB`), rebuilt identical to the CMake build's, read by the translator and a fresh
reviewer, 2026-10-16.

- (v) gfortran puts the zero on different sides of `MIN`/`MAX` in the vector body and in the
  scalar tail. At `:4497-4498` the body is four wide over `4*(n/4)` elements, then a scalar tail of
  up to three (with a separate entry for `n <= 3`). At `:4551-4552` it is four wide, one two-wide
  step, then one scalar `maxss`/`minss x,0` when `n` is odd. So with a NaN or ±0 the result depends
  on the element's position in `NSPEC`. The helpers `cp_sig`, `cm_sig`, `cp_the`, `cm_the`
  reproduce the rule for every `n` (read from the object by the translator and the reviewer).
- (v) One thread per node writes only its own diagonal column `PDLIB_I_DIAG(IP)`; the row ranges
  `IA(IP)..IA(IP+1)-1` are disjoint (`yowpdlibmain.F90:1625`), so no two nodes write one entry.
  `B_SIG` fused per spectral component keeps `divss, divss, subss` then `divss, mulss, addss`.
- (v) Methods 1 and 2 and refraction (`:4503-4527`, `:4551-4554`): operation order, true divisions,
  all `REAL`; no FMA. Method 1 is unreachable at this pin (`FreqShiftMethod = 2`,
  `w3profsmd_pdlib.F90:125`).
- ⚠ CUDA (compiled for `sm_89` with `--fmad=false`, never run): nvcc emits `max.f32(x,0)` and
  `min.f32(x,0)` for the `x>0?x:0` and `x<0?x:0` forms at `:4497,4498,4521,4525,4551,4552`. NaN
  handling matches, but `min.f32(−0,+0)` may give −0 where SSE gives +0, which changes a bit when
  an `ASPAR_JAC` entry is exactly ±0. Forcing a select would remove it; not done.
- ⚠ A node with `CCON = 0` leaves `PDLIB_I_DIAG` unset (allocated uninitialised,
  `yowpdlibmain.F90:1583`), undefined in the Fortran too.
- The reviewer also ran, against the static-only instruction and in its scratch directory, the
  header on the host beside an SSE emulation of the object for `NK` 1..7 by `NTH` 1..9 with NaN,
  ±0, ±1 inputs: 0 differing bits in 365 400 values. It is not committed and is not evidence under
  ADR-0010.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing of WW3 was run.
