# LSOL_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Solves `L x = y` with `L` unit lower triangular in compressed sparse row (CSR) storage (`al`,
`jal`, `ial`, diagonal implied and not stored) by forward elimination:
`x(1) = y(1)`, then `x(k) = y(k) - sum_j al(j)*x(jal(j))` over `j = ial(k), ial(k+1)-1`, the
sum taken left to right in a scalar `t`. A SPARSKIT routine kept in `w3profsmd.F90`.

## What was ported

| | |
|---|---|
| WW3 routine | `lsol`, `model/src/w3profsmd.F90:3258-3300` at NOAA-EMC/WW3 `03adda91`, 43 lines; no `#ifdef` inside it |
| Callers | none: `lsol` is `PRIVATE` to `w3profsmd` (`:61-65`) and nothing in `model/src` calls it (`git -C WW3-noaa grep -nw lsol -- model/src` finds only the declaration, the `PRIVATE` list and a comment). Translated for completeness; it can never be exercised |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:117-125`); gfortran keeps the dead routine in the object (`__w3profsmd_MOD_lsol`) |
| Kokkos code | [`../src/ww_kokkos/profs_lsol.hpp`](../src/ww_kokkos/profs_lsol.hpp), `ww::profs::lsol`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based holding the Fortran 1-based indices |
| C shim | none (no caller) |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) Same GIMPLE shape in both: `x(1) = y(1)` stored before the `n <= 1` test; per row `t`
  loaded from `y(k)`, kept in a register through `t = t - al(j)*x(jal(j))`, stored once to `x(k)`.
  The double-precision sequence of the inner loop is the same in both objects, `load x(jal(j));
  mul al(j); sub from t` (`movsd mulsd subsd` in the Fortran object, `vmovsd vmulsd vsubsd` in the
  C++ one from `-march=x86-64-v3`, the same IEEE operations), and the row ends with one store.
- (v) No FMA, no libm or libmvec call; neither object vectorised the routine
  (`-fopt-info-vec-optimized` reports nothing at `:3258-3300`).
- (v) The integer index arithmetic differs (the Fortran object carries `ial(k+1)` to the next
  row, the C++ reloads `ial(k)`); it moves no floating-point operation.
- ⚠ `x(1) = y(1)` is executed for any `n`, including `n < 1`, as in the Fortran; the C++ keeps it.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. The `w3profsmd` object was recompiled from `build_lab`'s
  `flags.make` and found identical to the CMake object; its `__w3profsmd_MOD_lsol` and the C++
  `ww::profs::lsol` were read instruction by instruction: the same `x(1)` store before the `n <= 1`
  test, the same per-row load of `y(k)`, inner `x(jal(j))*al(j)` then `t - product` in the same
  order, one store of `x(k)` per row, no FMA, no call, no vectorisation in either. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing calls `lsol`, so no WW3 regtest can exercise it.
