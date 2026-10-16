# AMUX_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Computes `y = A x` for a `REAL*8` matrix in compressed sparse row (CSR) form: `ia(1:n+1)` are the
row pointers, `a(k)` and `ja(k)` the values and column numbers. A SPARSKIT routine (Y. Saad) kept
in `w3profsmd.F90`.

## What was ported

| | |
|---|---|
| WW3 routine | `amux`, `model/src/w3profsmd.F90:2856-2901` at NOAA-EMC/WW3 `03adda91`, 46 lines; no `#ifdef` inside it |
| Callers | `runrc` (`:3858`), when the solver returns `ipar(1) = 1`; `runrc` is called by `W3XYPFSNIMP` (`:1239`), the implicit N-scheme for unstructured grids, chosen when `FSNIMP` is true (`IMPFSN` in the grid namelist, `w3gridmd.F90:3725`). Also `pgmres` (`:4388`, `:4425`), which nothing in WW3 calls. `regtests/ww3_ts4/input_ug` sets `IMPFSN = T` ⚠ (not run) |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json`), so by nearly every switch file, `switch_lab_shrd` included |
| Kokkos code | [`../src/ww_kokkos/profs_amux.hpp`](../src/ww_kokkos/profs_amux.hpp), `ww::profs::amux`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based holding the Fortran 1-based indices |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) Neither object vectorises the routine. Both keep `t` in a register, start it at `+0.0`,
  and per entry load `x(ja(k))`, multiply it by `a(k)` (`mulsd` / `vmulsd`) and add the product
  to `t` (`addsd` / `vaddsd`), in the Fortran's order of `k`, then store `y(i)` once.
- (v) The bound `ia(i+1)-1` is read once per row in both objects, as the Fortran `DO` defines.
- (v) No FMA, no libm or libmvec call, no reduction reordered.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. The routine has no `#ifdef`; `w3profsmd.F90` was
  recompiled from `build_lab`'s `flags.make` and found identical to the CMake object; its
  optimised GIMPLE and assembly were compared with a fresh C++ unit: same `t = a(k)*x(ja(k)) + t`
  chain in `k` order from `+0.0`, one scalar `mulsd`/`addsd` per entry (operands of the product
  swapped, which is exact), bound read once per row, no vectorisation, FMA or call. Callers,
  lines and switches in this report re-read with `git grep` at `03adda91`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Only the implicit unstructured-grid path (`FSNIMP`) reaches it.
