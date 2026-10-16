# STOPBIS_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The stopping test of the SPARSKIT `BCGSTAB` solver inside `W3PROFSMD`: it computes the residual
norm `FPAR(5) = sqrt(DDOT(N,R,R))`, optionally the change of the solution `FPAR(6)`, counts the
flops in `FPAR(11)`, and returns `.TRUE.` when `FPAR(6)` is not above the tolerance `FPAR(4)`
or the iteration limit `IPAR(6)` is reached. It is `PRIVATE` (`w3profsmd.F90:61`) and has one
caller, `BCGSTAB` at `:2292`, which passes `MVPI = 2` and `SX = one`. `BCGSTAB` is reached
through `RUNRC` from `W3XYPFSNIMP` (`:1239`), the implicit N scheme of unstructured grids that
`W3XYPUG` selects with `FSNIMP` (`:257-258`; namelist `UNST IMPFSN = T`).

## What was ported

| | |
|---|---|
| WW3 routine | `STOPBIS`, `model/src/w3profsmd.F90:2503-2557` at NOAA-EMC/WW3 `03adda91`; `w3profsmd.F90` is built with switch `PR1`, `PR2` or `PR3`; no `#ifdef` inside the routine. The one regtest that sets `IMPFSN = T` is `ww3_ts4` with `-i input_ug` (`regtests/ww3_ts4/input_ug/ww3_grid.inp:44`), as for [`DDOT`](DDOT_KOKKOS.md) |
| Kokkos code | [`../src/ww_kokkos/profs_stopbis.hpp`](../src/ww_kokkos/profs_stopbis.hpp), `ww::profs::stopbis(n, ipar, mvpi, fpar, r, delx, sx)` returning `bool`; `ipar[16]`, `fpar[16]`, `r[n]`, `delx[n]` 0-based for `IPAR(1:16)`, `FPAR(1:16)`, `R(1:N)`, `DELX(1:N)`; it calls `ww::profs::ddot` ([`profs_ddot.hpp`](../src/ww_kokkos/profs_ddot.hpp)) |
| C shim | none yet; it would come with `BCGSTAB` |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` as WW3's CMake Release build compiles it
(gfortran 16.2.0, `switch_lab_shrd`, recompiled and checked identical to the CMake object),
its `-fdump-tree-optimized-lineno` GIMPLE and disassembly, against a throwaway C++ unit with the
`openmp-release` flags and `-ffp-contract=off`, 2026-10-16.

- (v) `DDOT` is not inlined into `STOPBIS` in the Fortran object: both `DDOT(N,R,R)` (`:2524`)
  and `DDOT(N,DELX,DELX)` (`:2530`) are calls to `__w3profsmd_MOD_ddot`. In the C++ unit
  `ddot()` is inlined; its GIMPLE keeps the adds in source order (the remainder loop's vector
  products are extracted lane by lane and added in order, the unrolled body is
  left-associative), the same sequence of rounded adds that `DDOT_KOKKOS.md` checked against
  the Fortran `DDOT`.
- (v) `sqrt` is `sqrtsd` in the Fortran object (gfortran has no `errno` for `SQRT`). The C++
  object uses `vsqrtsd` and calls libm `sqrt` only when the argument is negative
  (`if (x u>= 0.0)` in the GIMPLE); a dot product of a vector with itself is never negative,
  and a NaN takes the `vsqrtsd` path, so the bits are the same.
- (v) `FPAR(11) + 2*N` (`:2525`, `:2531`): both objects form `2*N` in 32-bit integer and convert
  it (`cvtsi2sd`) before one `addsd`; the second add is `(2N) + FPAR(11)` in the Fortran
  object, commutative, same bits.
- (v) `FPAR(4) = FPAR(1)*FPAR(3) + FPAR(2)` (`:2538`): one `mulsd` then one `addsd` in both;
  no FMA (`grep -c vfmadd` gives 0 on the C++ object).
- (v) `FPAR(6) .GT. FPAR(4)` (`:2548`) is `comisd` + `jbe` in the Fortran object: a NaN in
  `FPAR(6)` sets `STOPBIS = .TRUE.`, `IPAR(11) = 1`, as the comment at `:2544-2547` intends;
  C++ `>` is false on NaN, the same branch.
- `MVPI+MVPI+1` (`:2532`) is kept as written; the object forms `2*MVPI+1` with `lea`, the same
  integer.
- No variant: the routine has no `#ifdef`.
- CUDA not compiled (optional this batch).
- (v) Fresh review 2026-10-16: no defect. `w3profsmd.F90` recompiled under `switch_lab_shrd`
  and identical to the CMake object; GIMPLE and disassembly of `STOPBIS` compared with a fresh
  C++ unit operation by operation: the order of the `IPAR` tests (reads only, same writes), both
  `DDOT` sums in source order, `sqrtsd` on the path every non-negative or NaN sum takes,
  `2*N` in `int` then `cvtsi2sd`, `(2N) + FPAR(11)` on the second add, `FPAR(1)*FPAR(3) +
  FPAR(2)` without FMA, `SX * sqrt` kept when `N <= 0`, and the `comisd` test on `FPAR(6) >
  FPAR(4)` with its NaN branch. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
