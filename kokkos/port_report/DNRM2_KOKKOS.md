# DNRM2_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The BLAS-1 function that returns the Euclidean norm `sqrt(x'*x)` of a REAL(8) vector, with the
loop of `DLASSQ` inlined so it scales by the largest `|X(i)|` seen and does not overflow. It is
`PRIVATE` (`w3profsmd.F90:61-65`) and its only callers are `PGMRES` (`:4403`, `:4453`), which
is itself `PRIVATE` and called by nothing: `git grep -niw pgmres` in `model/src` finds only its
definition, the `PRIVATE` list and two commented-out headers (`:4302-4303`). `DNRM2` is
translated for completeness and can never be exercised by a WW3 run.

## What was ported

| | |
|---|---|
| WW3 routine | `DNRM2`, `model/src/w3profsmd.F90:4568-4635` at NOAA-EMC/WW3 `03adda91`; `w3profsmd.F90` is built with switch `PR1`, `PR2` or `PR3`; no `#ifdef` inside the routine, so there is no variant |
| Kokkos code | [`../src/ww_kokkos/profs_dnrm2.hpp`](../src/ww_kokkos/profs_dnrm2.hpp), `ww::profs::dnrm2(n, x)`, `x` 0-based for `X(1:N)` (assumed size, unit stride; the Fortran has no `INCX`); it reuses `dlassq_paren` of [`profs_dlassq.hpp`](../src/ww_kokkos/profs_dlassq.hpp) and keeps its own loop, as the Fortran does |
| C shim | none, and none planned: no caller is ever reached |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` as WW3's CMake Release build compiles it
(gfortran 16.2.0, `switch_lab_shrd`, recompiled and checked identical to the CMake object),
its `-fdump-tree-optimized-lineno` GIMPLE and disassembly, against a throwaway C++ unit with the
`openmp-release` flags and `-ffp-contract=off`, 2026-10-16.

- (v) Same instruction sequence in both objects (SSE2 in the Fortran, VEX forms in the C++):
  `N <= 0` returns 0, `N == 1` returns `andpd` of `X(1)` (`:4605-4608`); in the loop `ucomisd`
  against 0 with the parity branch (`:4617`), `andpd` for `ABS` (`:4618`), `comisd`/`ja` for
  `SCALE < ABSXI` (`:4619`), then `divsd`, `mulsd r,r`, `mulsd` by `SSQ`, `addsd 1.0` (`:4620`)
  or `divsd`, `mulsd r,r`, `addsd` to `SSQ` (`:4623`); `sqrtsd` then `mulsd` by `SCALE` (`:4627`).
- (v) gfortran keeps the parentheses of `(SCALE/ABSXI)**2` and `(ABSXI/SCALE)**2` as
  `PAREN_EXPR` (`_23 = ((_6))`, `_20 = ((_8))`); the header puts `__builtin_assoc_barrier` on
  both quotients through `dlassq_paren`, and the C++ GIMPLE shows the same two `((_))`. Without
  it g++ drops the `fabs` before the squared quotient (the defect `profs_dlassq.hpp` records).
- (v) `(...)**2` is `r*r`, no `pow` call; no FMA (`grep -c fmadd` gives 0 on the C++ object), no
  vectorisation (the loop carries `SCALE` and `SSQ`).
- (v) `SQRT`: gfortran emits a bare `sqrtsd` (no `errno`); g++ emits `vsqrtsd` plus a call to
  libm `sqrt` when `SSQ < 0`. `SSQ` starts at 1 and only grows by non-negative terms, so that
  branch is never taken (a NaN `SSQ` fails `0 > SSQ` and also goes to `vsqrtsd`); glibc's `sqrt`
  would return the same bits anyway.
- (v) Commuted operands of two commutative operations (`SSQ*r**2` at `:4620`, `SCALE*SQRT`
  at `:4627`) change only which NaN payload `mulsd` keeps when both inputs are NaN. Neither can
  happen: `SCALE` is never NaN (it takes `ABSXI` only when `SCALE < ABSXI` is true), and in the
  `:4620` branch `r` is finite. At `:4623` both objects keep `SSQ` as the first operand.
- A NaN in `X` passes `X(IX) /= 0` and takes the `ELSE` branch (`:4623`) in both, since
  `SCALE < NaN` is false.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3profsmd.F90` from `build_lab`'s `flags.make`
  (identical to the CMake object; the routine has no `#ifdef`, so no variant) and the C++ unit with the
  `openmp-release` flags; the two optimised GIMPLE bodies match statement for statement (both `PAREN_EXPR`,
  same branch order), and the disassembly differs only in the two commuted products above and g++'s
  unreachable libm `sqrt` path. Fixed the count of commuted operations in the report (three to two).
- ⚠ CUDA was not compiled. A compiler without `__builtin_assoc_barrier` falls back to no barrier.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest can exercise it: its only caller, `PGMRES`, is never
called.
