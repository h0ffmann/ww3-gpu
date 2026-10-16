# DLASSQ_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The LAPACK 3.1 auxiliary routine that updates a scaled sum of squares of a REAL(8) vector,
`(scl**2)*smsq = x(1)**2 + ... + x(n)**2 + (scale**2)*sumsq`, kept inside `W3PROFSMD`. It is
`PRIVATE` (`w3profsmd.F90:61-65`) and nothing calls it: `git grep -i dlassq` in `model/src`
finds only its definition, the `PRIVATE` list and two comments of `DNRM2` (`:4589`, `:4614`),
which carries the same loop inlined (`:4617-4628`). It is translated for completeness and can
never be exercised by a WW3 run.

## What was ported

| | |
|---|---|
| WW3 routine | `DLASSQ`, `model/src/w3profsmd.F90:4638-4681` at NOAA-EMC/WW3 `03adda91`; `w3profsmd.F90` is built with switch `PR1`, `PR2` or `PR3`; no `#ifdef` inside the routine, so there is no variant |
| Kokkos code | [`../src/ww_kokkos/profs_dlassq.hpp`](../src/ww_kokkos/profs_dlassq.hpp), `ww::profs::dlassq(n, x, scale, sumsq)`, `x` 0-based for `X(1:N)` (assumed size, unit stride; the Fortran has no `INCX`), `scale` and `sumsq` by reference |
| C shim | none, and none planned: no caller |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` as WW3's CMake Release build compiles it
(gfortran 16.2.0, `switch_lab_shrd`, recompiled and checked identical to the CMake object),
its `-fdump-tree-optimized-lineno` GIMPLE and disassembly, against a throwaway C++ unit with the
`openmp-release` flags and `-ffp-contract=off`, 2026-10-16.

- (v) Instruction for instruction the same sequence in both objects (SSE2 in the Fortran, the
  VEX forms in the C++): `ucomisd` against 0 with the parity branch (`:4669`), `andpd` for `ABS`
  (`:4670`), `comisd`/`ja` for `SCALE < ABSXI` (`:4671`), then `divsd`, `mulsd r,r`,
  `mulsd` by `SUMSQ`, `addsd 1.0` (`:4672`) or `divsd`, `mulsd r,r`, `addsd SUMSQ` (`:4675`),
  with `SCALE` and `SUMSQ` stored at each update.
- (v) `(...)**2` is `r*r` in the object, no `pow` call; no FMA (`grep -c fmadd` gives 0 on the
  C++ object), no libm call, no vectorisation (the loop carries `SCALE` and `SUMSQ`).
- (v) gfortran keeps the parentheses of `(SCALE/ABSXI)**2` and `(ABSXI/SCALE)**2` as
  `PAREN_EXPR`. Without them g++ removed the `fabs` from the division, since the quotient is
  squared, and divided by the signed `X(IX)`: equal bits for every finite input, but a negative
  NaN in `X` would leave a negative NaN in `SUMSQ` where the Fortran leaves a positive one. The
  header puts `__builtin_assoc_barrier` on both quotients (`dlassq_paren`, as `tide_dpythag.hpp`
  does), and the C++ GIMPLE then shows the same `((_q))` and the object the same instructions.
  ⚠ A compiler without that builtin falls back to no barrier; CUDA was not compiled.
- A NaN in `X` passes `X(IX) /= 0` and takes the `ELSE` branch (`:4675`) in both, since
  `SCALE < NaN` is false.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3profsmd.F90` from `build_lab`'s
  `flags.make` (identical to the CMake object) and a C++ unit with the `openmp-release` flags,
  with and without `__restrict` on `scale`/`sumsq`; the loop bodies match instruction for
  instruction (same `ucomisd`/parity test, `andpd`, `comisd`/`ja`, `divsd`, `mulsd r,r`,
  `mulsd`, `addsd`), the C++ GIMPLE keeps both `((q))` barriers. Without `__restrict` g++
  stores `SUMSQ` before `SCALE` on the `:4672` branch, a difference only if the two arguments
  alias, which Fortran forbids. The loop counter wraps at `N = HUGE(N)` the same way in both
  objects. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest can exercise it: nothing calls `DLASSQ`.
