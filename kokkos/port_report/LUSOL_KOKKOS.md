# LUSOL_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Solves `(LU) x = y` with incomplete LU factors stored in modified sparse row form (`alu`, `jlu`,
`ju`, diagonal stored inverted): a forward substitution over the strict lower part, then a
backward one over the strict upper part, each row ending with `x(i) = alu(i)*x(i)`. A SPARSKIT
routine kept in `w3profsmd.F90` as the preconditioner of the implicit N scheme on unstructured
grids.

## What was ported

| | |
|---|---|
| WW3 routine | `lusol`, `model/src/w3profsmd.F90:3673-3699` at NOAA-EMC/WW3 `03adda91`, 27 lines; no `#ifdef` inside it |
| Callers | `runrc` (`w3profsmd.F90:3864`) when the solver asks for the right preconditioner (`ipar(1)` 3 or 5); `runrc` is called only by `W3XYPFSNIMP` (`:1239`, `IPAR(2) = 1`, right preconditioning, `:1205`), selected by `FSNIMP` in `W3XYPUG` (`:257`), called from `W3WAVE` (`w3wavemd.F90:1970-1976`). `pgmres` (`:4424`, `:4518`) also calls it under `lblas`, but nothing calls `pgmres` |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:117-125`), so by nearly every switch file; no regtest namelist sets `FSNIMP = T` (`git -C WW3-noaa grep -il "FSNIMP *= *T" -- regtests` is empty), so no regtest exercises it |
| Kokkos code | [`../src/ww_kokkos/profs_lusol.hpp`](../src/ww_kokkos/profs_lusol.hpp), `ww::profs::lusol`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based holding the Fortran 1-based indices |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) Same GIMPLE shape in both: forward loop `x(i) - alu(k)*x(jlu(k))` with `x(i)` carried in a
  register and stored every iteration, backward loop with `x(i)` loaded once per row, then
  `alu(i)*x(i)`. The double-precision operation sequence of the two objects is the same,
  `mov mov mov mul sub mov | mov mov mov mul sub mov mul mov` (SSE in the Fortran object, VEX in the
  C++ one from `-march=x86-64-v3`, the same IEEE operations).
- (v) No FMA, no reciprocal, no libm or libmvec call; neither object vectorised the routine
  (`-fopt-info-vec-optimized` reports nothing at `:3673-3699`).
- ⚠ `pgmres` (`:4518`) passes `rhs` as both `y` and `x`, an aliasing of a modified dummy that
  Fortran forbids. The object reads `y(i)` before it writes `x(i)` and never reads `y` again, so
  the result equals the in-place solve; the C++ keeps that order and takes no `restrict`. The call
  is dead (nothing calls `pgmres`).
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3profsmd.F90` under `switch_lab_shrd` (object
  identical to the CMake one) and the header in a throwaway unit; the optimised GIMPLE and the
  assembly match operation by operation: `x(jlu(k))*alu(k)` with `x(jlu(k))` as the first operand,
  `x(i) - product`, `alu(i)*x(i)`, loop bounds read once per row, no vectorisation, no FMA. Callers
  (`:3864`, `:4424`, `:4518`) and switches re-read with `git grep` at `03adda91`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest sets `FSNIMP`, so the WW3 regtests cannot exercise it.
