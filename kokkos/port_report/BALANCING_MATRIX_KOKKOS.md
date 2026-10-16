# BALANCING_MATRIX_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Balancing of a square `REAL` matrix by integer powers of the radix (Numerical Recipes' `balanc`),
a similarity transformation that leaves the eigenvalues unchanged and reduces their sensitivity to
rounding. In `W3SIC5` (switch `IC5`) it is called once per root search: `FSDISP`
(`w3sic5md.F90:652`) calls `POLYROOTS(6, ...)` (`:853`), which builds the 5 by 5 companion
(Hessenberg) matrix `HESS` and calls `BALANCING_MATRIX(NPC-1, HESS)` (`:1576`) before `EIG_HQR`.
gfortran `-O3` does not inline it: `POLYROOTS` calls the out-of-line
`__w3sic5md_MOD_balancing_matrix` (a `call` with its relocation in the object of `build_mixb`, (v)
2026-10-16).

## What was ported

| | |
|---|---|
| WW3 routine | `BALANCING_MATRIX`, `model/src/w3sic5md.F90:956-1109` at NOAA-EMC/WW3 `03adda91` (arithmetic at `:1069-1104`); compiled by the `IC5` switches of `ww3_tic1.1` (`input_IC5_M1`, `_M2`, `_M3`; `git grep -lw IC5 -- '*switch*'`), by no NOAA operational switch (`model/bin/switch_*`). Only `#ifdef W3_S` (tracing, no arithmetic) |
| Kokkos code | [`../src/ww_kokkos/ic5_balancing_matrix.hpp`](../src/ww_kokkos/ic5_balancing_matrix.hpp), `ww::ic5::balancing_matrix(nmat, matrix)`, `matrix` column-major with leading dimension `nmat` |
| C shim | none yet; its only caller is `POLYROOTS` |
| Plan | issue #42, batch `goal/42-port-batch-3-shims` |

## Arithmetic

Reference: the `-O3` object of `w3sic5md.F90` (WW3's CMake Release flags, gfortran 16.2, the lab
switch with `NL4 IC5 BT9 NLS`, `build_mixb`), recompiled and checked identical to the CMake object,
compared with a throwaway C++ unit under the `openmp-release` flags (g++ 16.2, warning-free),
2026-10-16.

- (v) `RADX = RADIX(MATRIX) = 2.0`, `SQRADX = 4.0` (`:1060-1061`). Both compilers emit the same
  constants (`0.5`, `4.0`, `0.25`, `0.95f` = `0x3F733333`, `1.0`) and the same exact rewrites:
  `R/RADX` as `R*0.5`, `R*RADX` and `F*RADX` as `x+x`, `F/RADX` as `F*0.5`, `C/SQRADX` as `C*0.25`.
- (v) `SUM(ABS(MATRIX(:, I)))` and `SUM(ABS(MATRIX(I, :)))` (`:1073-1074`) are summed from `0.0`
  in index order with the accumulator as first source in both objects. gfortran vectorises the
  column load 4 wide and g++ 8 wide, but both add the lanes one by one in order (`addss`), so the
  sequence of roundings is the Fortran's. The row sum is scalar in both. The diagonal is then
  subtracted (`subss`).
- (v) `C /= 0.0 .AND. R /= 0.0` is `ucomiss` with `setp`/`cmovne` in both (a NaN counts as
  non-zero). Both radix loops, `S = C + R`, `(C+R)/F < 0.95*S` (`R + C` with `R` as first source in
  both objects, `S*0.95`, `divss` by `F`, `comiss`) and `G = 1.0/F` match instruction for instruction.
- (v) The row scaling `MATRIX(I, :) * G` (strided, scalar) comes before the column scaling
  `MATRIX(:, I) * F` (vectorised, element-wise) in both, so the diagonal is multiplied by `G`, then
  `F`. g++ puts the factor as first source of `mulss`/`mulps` where gfortran puts the matrix element;
  the factor is never NaN (`F` is `2**k`, possibly infinite; `G` is `1/F`), so with at most one NaN
  operand the product is that operand's quiet NaN in either order: same bits.
- ⚠ A `C` or `R` that is NaN or infinite never satisfies the exit tests of the radix loops
  (`:1082-1093`): WW3 loops forever. The header reproduces that path. A `C` far below `R` can drive
  `F` to infinity (more than 127 doublings) and `G` to 0; reproduced too.
- ⚠ Not compiled for CUDA. The routine uses no libm call, so nothing in it prevents bit for bit on
  a device, but no device object was read.
- (v) Fresh review 2026-10-16: no defect. The `w3sic5md` object was recompiled from `build_mixb`'s
  flags (identical to the CMake object; only `W3_S`, tracing, varies the routine) and its GIMPLE and
  assembly were compared with a C++ unit under the `openmp-release` flags: same in-order lane sums,
  same constants, same comparison sense in both radix loops, same scaling order, no libm call in
  either. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
