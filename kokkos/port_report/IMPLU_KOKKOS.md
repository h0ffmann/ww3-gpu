# IMPLU_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Performs implicitly one step of the LU factorisation of a banded Hessenberg matrix: applies the
previous row interchanges and eliminations (`permut`, `ypiv`) to the new column `u`, then decides
the pivot of the next step from `beta` against `|u(np)|` and, when the band is `full`, shifts the
pivot history up by one. A SPARSKIT routine kept in `w3profsmd.F90`, in `REAL*8`.

## What was ported

| | |
|---|---|
| WW3 routine | `implu`, `model/src/w3profsmd.F90:2372-2416` at NOAA-EMC/WW3 `03adda91`, 45 lines; no `#ifdef` inside it |
| Callers | none: `implu` is declared `PRIVATE` (`w3profsmd.F90:61`) and no `call implu` exists in `model/src` (`git -C WW3-noaa grep -in "call implu" -- model/src` is empty). It is translated for completeness and can never be exercised |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:116-125`), so by nearly every switch file; gfortran keeps the routine in the object (`__w3profsmd_MOD_implu`) although nothing calls it |
| Kokkos code | [`../src/ww_kokkos/profs_implu.hpp`](../src/ww_kokkos/profs_implu.hpp), `ww::profs::implu`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, `permut` as `int` (`LOGICAL(4)`, written 0/1), arrays 0-based |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) Elimination loop (`:2386-2393`): both objects load `u(k)` and `u(k+1)` before the `permut(k)`
  test and compute `u(k+1) = a - ypiv(k)*b` with `(a, b)` = `(u(k), u(k+1))` when permuted and
  `(u(k+1), u(k))` otherwise: one `mulsd`, one `subsd`, in that order. The C++ object also stores
  the swapped `u(k+1)` before overwriting it, a dead store that changes no value.
- (v) Pivot (`:2398-2407`): `beta > |umm|` as `andpd` with the sign mask then `comisd`/`seta` in
  both; `umm/beta` or `beta/umm` as one `divsd`. No FMA, no libm call; neither object vectorised
  the routine.
- (v) Shift (`:2410-2413`): the Fortran object calls `memmove(ypiv, ypiv+1, 8*npm1)` and
  `memmove(permut, permut+4, 4*npm1)`; the C++ object calls the same two `memmove` when `ypiv` and
  `permut` do not overlap and a fused element loop otherwise (C++ cannot assume distinct dummies).
  For distinct arrays, the only case the Fortran allows, the bytes are the same.
- ⚠ `npm1` is assigned only when `np > 1` (`:2382`) but read by the shift loop at `:2410` whenever
  `full` is true. For `np <= 1` and `full` the Fortran reads an undefined integer: the object
  passes whatever `%r10` holds to `memmove` as the size. That path cannot be reproduced; the C++
  sets `npm1 = 0` there, so it shifts nothing. Dead code either way (no caller).
- ⚠ The C++ assumes `ypiv`, `u` and `permut` are distinct arrays, as Fortran requires.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3profsmd.F90` from `build_lab`'s flags (object identical to the CMake one) and a C++ unit with the openmp-release flags, and compared the two disassemblies of `implu` instruction by instruction: the elimination loop's loads, `mulsd`/`subsd` operands per branch, the `comisd`/`seta` pivot test (NaN `beta` or `umm` takes the `else` branch in both), the two `divsd` operand orders, the 0/1 `permut` store and the two `memmove` sizes for `np > 1`. Setting `npm1 = 0` for `np <= 1` is accepted: the Fortran passes an uninitialised register to `memmove` there, which no translation can reproduce. Fixed the `switches.json` line range.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing calls `implu`, so no WW3 regtest can exercise it.
