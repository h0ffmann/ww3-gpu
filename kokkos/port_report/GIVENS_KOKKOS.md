# GIVENS_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The Givens rotation of the SPARSKIT iterative solvers that `W3PROFSMD` carries: given `(x, y)` in
REAL(8), it returns `c`, `s` with `(c, s)` rotating `(x, y)` onto `(sqrt(x**2 + y**2), 0)`, and
leaves `|x*y|` in `x` (`:2495`). It is `PRIVATE` (`w3profsmd.F90:61`) and nothing in
`model/src` calls it (`git -C WW3-noaa grep -n -i givens -- model/src` finds only its definition
and the `PRIVATE` list): it is translated for completeness and can never be exercised by a regtest.

## What was ported

| | |
|---|---|
| WW3 routine | `GIVENS`, `model/src/w3profsmd.F90:2461-2500` at NOAA-EMC/WW3 `03adda91`; `w3profsmd.F90` is built with switch `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:117-125`), so with most regtest and NOAA operational switches; no `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/profs_givens.hpp`](../src/ww_kokkos/profs_givens.hpp), `ww::profs::givens(x, y, c, s)`, all four `double&` (INTENT unspecified in the Fortran, all written or read and written) |
| C shim | none; no caller to patch |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` as WW3's CMake Release build compiles it
(gfortran 16.2.0, `switch_lab_shrd`, recompiled and checked identical to the CMake object),
its `-fdump-tree-optimized-lineno` GIMPLE and disassembly, against a throwaway C++ unit with the
`openmp-release` flags and `-ffp-contract=off`, 2026-10-16.

- (v) Same operations in the same order on each branch: `t = x/y` (or `y/x`), `t*t`, `+1`,
  `sqrtsd` inline (no libm call in either object), `1/sqrt`, `copysign` (SIGN on REAL), `t*s`
  (or `t*c`), then `|x*y|`. No FMA, no reciprocal approximation, no vectorisation.
- (v) The `x == 0 .and. y == 0` branch: gfortran folds `ABS(x*y)` to `0.0`; the C++ computes
  `|x*y|` with both operands zero, which is `+0.0` for every sign of the zeros, so the same bits.
- (v) The C++ reloads `x`/`y` after stores because two references may alias; the Fortran forbids
  aliased dummies, so the values loaded are the ones gfortran kept in registers.
- (v) The last `ELSE` (`:2486-2493`) is reached only when `x` or `y` is a NaN; it is kept, as are
  both comparisons of `:2476` and `:2481`.
- ⚠ CUDA not compiled this batch.
- (v) Fresh review 2026-10-16: no defect. `w3profsmd.F90` recompiled from `build_lab`'s
  `flags.make` (disassembly identical to the CMake object) and compared instruction by
  instruction with the C++ unit, called through references and through `__restrict` pointers:
  same branch tests (`ucomisd` with parity on the zero test, `comisd` for `>` then `>=`, NaN
  falling to the last `ELSE`), same `divsd`/`mulsd`/`addsd`/`sqrtsd`/`divsd` sequence, first
  operands of `t*s`, `t*c` and `x*y` the same, SIGN as a sign-bit merge, `+0.0` in `x` on the
  zero and NaN branches; no `#ifdef` in `:2461-2500`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest can exercise it: nothing calls `GIVENS`.
