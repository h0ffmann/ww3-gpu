# SMCDCXY_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Current-velocity gradients on a Spherical Multiple-Cell (SMC) grid: `SMCGradn` gives the cell
gradients of `CX` and `CY`, each is limited to magnitude 0.01, rotated to map east in the Arctic
part (`ARCTC`, cells `n > NGLO`), and copied to every point of the full `NY` x `NX` grid the cell
covers (`DCXDX`, `DCXDY`, `DCYDX`, `DCYDY`). `W3WAVE` calls it once per time step on an SMC grid
(`w3wavemd.F90:1202`, under `W3_SMC`).

## What was ported

| | |
|---|---|
| WW3 routine | `SMCDCXY`, `model/src/w3psmcmd.F90:3048-3159` at NOAA-EMC/WW3 `03adda91` (112 lines); compiled by every switch with `SMC`: `model/bin/switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, `regtests/ww3_tp2.10`, `ww3_tp2.16`, `mww3_test_09`, `ww3_tp2.14/input_oasacm6` (`git grep -lw SMC -- '*switch*'`); `ww3_tp2.10` and `ww3_tp2.16` `switch_MPI_OMPH` carry `OMPG` |
| Kokkos code | [`../src/ww_kokkos/psmc_smcdcxy.hpp`](../src/ww_kokkos/psmc_smcdcxy.hpp): `smcdcxy_point` (one iteration of either `DO n` loop, `:3079-3101`, `:3125-3147`) and the host driver `smcdcxy` |
| Not ported | `SMCGradn` (`:2341-2539`), which `SMCDCXY` calls twice (`:3073`, `:3119`): the driver takes it as a caller-provided callable until it is ported. `CXCY`, `GrHx`, `GrHy` (automatic arrays, `:3058`) are caller-provided work arrays |
| `#ifdef` | `W3_OMPG` makes both `DO n` loops parallel; the arithmetic of the outlined bodies is the same (below). `W3_T` is a `PRINT` only |
| C shim | none yet |
| Plan | [`BATCH3_SHIMS_KOKKOS_PLAN_202610.md`](../../docs/BATCH3_SHIMS_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3psmcmd.F90` built with the flags of WW3's CMake Release build
under `switch_SMCMlt` (gfortran 16.2.0, pinned shell), identical to the CMake object, and the same
file with `-DW3_OMPG -fopenmp`; compared with a throwaway C++ unit compiled with the
openmp-release flags (g++ 16.2.0, warning-free), 2026-10-16.

- (v) Limiter: `ABS(x) > 0.01` then `copysignf(0.01, x)` in both GIMPLE dumps, the constant
  `9.99999977648258209228515625e-3`; a NaN fails the test and is kept, as in the Fortran.
- (v) Rotation: `CNST0 = ANGARC(n-NGLO)*DERA` with `DERA` folded to
  `1.745329238474369049072265625e-2` (`ww::prop::kDera`), one `sincosf` call for `COS` and `SIN`
  in both objects, then `cos*GrHx - sin*GrHy` and `sin*GrHx + cos*GrHy`: the same four `mulss`, one
  `subss`, one `addss` with the same operands in both; no FMA in either.
- (v) Indices: `IJKCel(r,n)/MRFct` is a truncating `idiv` in both, `MAX(1, ...)` on the sizes.
- (v) `W3_OMPG`: the outlined bodies `smcdcxy._omp_fn.0/1` have the same limiter, `sincosf` and
  rotation sequence. The parallel loop writes disjoint blocks of the full grid only when no two
  cells cover the same grid point, which an SMC cell list satisfies; the C++ driver runs the
  loop serially, `smcdcxy_point` can run per `n` in a parallel loop under the same condition.
- (v) The block fills and the `= 0.0` clears copy values only; their order does not change bits.
- ⚠ Bit parity of the whole routine also needs `SMCGradn`, which is not ported.
- ⚠ CUDA not compiled for this header.
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3psmcmd.F90` from `build_smc`'s
  `flags.make` (identical to the CMake object) and with `-DW3_OMPG -fopenmp`, and the header in a
  throwaway openmp-release unit; both objects call `sincosf` twice and nothing else from libm, and
  the four rotation products and the `subss`/`addss` have the same first operands (read from the
  `sincosf` result slots). Fixed in the header: the comment on `cx, cy` now says `CX, CY` are
  allocated `(0:NSEA)` (`w3adatmd.F90:1008`) and are passed from `CX(1)`, as `CX(1:NSEA)` at
  `:3066` reads them; the old comment read as if `CX(0)` were the first element.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. `W3WAVE` calls `SMCDCXY` only when currents are on (`IF ( FLCUR )`
before `w3wavemd.F90:1202`); whether an SMC regtest drives currents was not checked ⚠.
