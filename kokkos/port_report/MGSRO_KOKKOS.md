# MGSRO_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The modified Gram-Schmidt step with selective re-orthogonalisation of the SPARSKIT iterative
solvers that `W3PROFSMD` carries: column `IND` of `VEC(LDA, M)` (REAL(8)) is orthogonalised
against columns `1:IND-1` (and `IND+1:M` when `FULL`), re-orthogonalised against a column when
`FCT**2 > 0.98 * NRM0`, then normalised; `HH` gets the coefficients and the final norm, `OPS` the
operation count, `IERR` 0, -1 (zero vector), -2 (abnormal number) or -3 (linear combination). It
is `PRIVATE` (`w3profsmd.F90:61-65`) and nothing in `model/src` calls it
(`git -C WW3-noaa grep -n -i mgsro -- model/src` finds only its definition, the `PRIVATE` list
and comments of the solver documentation at `:1810-1870`): it is translated for completeness and
can never be exercised by a regtest.

## What was ported

| | |
|---|---|
| WW3 routine | `MGSRO`, `model/src/w3profsmd.F90:2701-2822` at NOAA-EMC/WW3 `03adda91`; `w3profsmd.F90` is built with switch `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:117-125`); no `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/profs_mgsro.hpp`](../src/ww_kokkos/profs_mgsro.hpp), `ww::profs::mgsro(full, lda, n, m, ind, ops, vec, hh, ierr)`; `vec` is `VEC(LDA, M)` column-major 0-based, `hh` is `HH(1:M)` 0-based, `ind` stays the Fortran 1-based index; `ops` and `ierr` by reference; `full` is `bool` (the shim converts `LOGICAL(4)`) |
| Callees | `ww::profs::ddot` ([`DDOT_KOKKOS.md`](DDOT_KOKKOS.md)); the module function `DDOT` is called, not inlined, in the Fortran object; the C++ inlines it |
| C shim | none; no caller to patch |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` as WW3's CMake Release build compiles it
(gfortran 16.2.0, `switch_lab_shrd`, `ref2/build_lab` flags, recompiled and checked identical to
the CMake object), its `-fdump-tree-optimized-lineno` GIMPLE and disassembly, against a throwaway
C++ unit with the `openmp-release` flags and `-ffp-contract=off`, 2026-10-16. The C++ unit
compiles warning-free.

- (v) `REORTH = 0.98D0` (`:2738`) appears as the same double in both GIMPLEs
  (`9.7999999999999998e-1`); `THR = NRM0 * REORTH` after every update, as the source.
- (v) The updates `VEC(k,IND) - FCT * VEC(k,I)` and the final scale `VEC(k,IND) * FCT` are
  element-wise: gfortran vectorises them two-wide (`mulpd`, `subpd`) with a scalar tail, g++
  vectorises them its own way; each element is the same rounded product then difference, so
  the bits are the same. No FMA (`grep -c vfma` gives 0 on the C++ object).
- (v) No vector reduction in the C++ unit (no `vaddpd`/`vhaddpd`, no `REDUC` in the GIMPLE):
  the inlined `ddot` keeps its scalar left-to-right sum, the order `DDOT_KOKKOS.md` verified.
- (v) `OPS = OPS + N + N`, `OPS + 4*N + 2`, `OPS + 4*N + 1`, `OPS + N + 1`: left to right,
  `4*N` as an INTEGER product converted to REAL(8), in both objects.
- (v) `NRM0 - HH(I)*HH(I)` then `IF (NRM0 < 0) NRM0 = 0`: both compilers turn the clamp into a
  select (`_3 ? 0.0 : nrm0`), NaN passing through unchanged in both. gfortran forwards `HH(I)` as
  `FCT` (or `FCT + FCT2` after re-orthogonalisation); the C++ reloads it from `hh`, the same
  value for non-aliased dummies, which the Fortran requires.
- (v) `SQRT` is `sqrtsd` inline in both; g++ adds a `sqrt` libm call only for a negative
  argument (errno), which returns the same NaN. The test chain `NRM0 <= 0`, `NRM0 > 0 .AND.
  1/NRM0 > 0`, else `-2` is kept, so NaN and `+Inf` both give `IERR = -2`.
- (v) On the dead path `N <= 0` (it returns `IERR = -1` first, since `DDOT` gives 0) gfortran
  writes `HH(I) = FCT * 2`; the C++ writes `FCT + FCT`, the same bits.
- ⚠ The NaN-payload operand order of `ddot`'s remainder adds, as in `DDOT_KOKKOS.md`.
- ⚠ With `LDA < N` the columns overlap; the Fortran is then out of bounds and its vectorised
  loop's runtime alias check decides the order of reads. Not reproduced; a conforming caller
  passes `LDA >= N`.
- ⚠ CUDA not compiled this batch.
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3profsmd.F90` from
  `ref2/build_lab` flags (identical to the CMake object) and the C++ unit, and compared the
  GIMPLE and disassembly of `MGSRO` operation by operation: `REORTH` bits, the `OPS` sums, the
  test chain, the clamp select, `sqrtsd`, the element-wise updates, and the inlined `ddot` (its
  4-wide multiply of a length-4 remainder is followed by in-order scalar adds; no `vaddpd`, no
  `vfma`). Fixed only the source-line cites in the header's comments, which were off by two to
  four lines; the C++ object's code is unchanged.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest can exercise it: nothing calls `MGSRO`.
