# UPPDIR_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A helper of the SPARSKIT banded iterative solvers that `W3PROFSMD` carries: in REAL(8), it
subtracts from `y` the stored conjugate directions `p(:, j)` scaled by `u(ju)/usav(j)` for the
`np-1` previous directions (a ring buffer of `lbp` columns indexed by `indp`), counts `2*n` flops
per nonzero factor, then advances `indp`, saves `u(np)` in `usav(indp)` and stores `y` as the new
column `p(:, indp)`. It is `PRIVATE` (`w3profsmd.F90:61`) and nothing in `model/src` calls it
(`git -C WW3-noaa grep -n -i uppdir -- model/src` finds only its definition and the `PRIVATE`
list): it is translated for completeness and can never be exercised by a regtest.

## What was ported

| | |
|---|---|
| WW3 routine | `UPPDIR`, `model/src/w3profsmd.F90:2418-2459` at NOAA-EMC/WW3 `03adda91`; `w3profsmd.F90` is built with switch `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:117-125`), so with most regtest and NOAA operational switches; no `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/profs_uppdir.hpp`](../src/ww_kokkos/profs_uppdir.hpp), `ww::profs::uppdir(n, p, np, lbp, indp, y, u, usav, flops)`; `p` is `p(n, lbp)` column major, `y`, `u`, `usav` assumed-size, all Fortran base 1 and passed 0-based; `indp`, `flops` by reference |
| C shim | none; no caller to patch |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` as WW3's CMake Release build compiles it
(gfortran 16.2.0, `switch_lab_shrd`, recompiled and checked identical to the CMake object),
its `-fdump-tree-optimized-lineno` GIMPLE and disassembly, against a throwaway C++ unit with the
`openmp-release` flags and `-ffp-contract=off`, 2026-10-16.

- (v) Same operations: `x = u(ju)/usav(j)` (`divsd`), the test `x /= 0`, then per `k`
  `y(k) - x*p(k,j)` as a multiply then a subtract. No FMA in either object (`grep` of the
  disassembly), no libm call.
- (v) The update loop `:2440-2442` is vectorised in both objects (gfortran 2 lanes,
  `mulpd`/`subpd`; g++ `-march=x86-64-v3` 4 then 2 lanes). It is element-wise, with no
  reduction, so each `y(k)` has the bits of the scalar loop in either width.
- (v) `flops = flops + 2*n`: `2*n` in INTEGER(4), converted to REAL(8) and added, also when
  `n <= 0` (the object adds `2*n` after skipping the empty loop); the C++ does the same.
- (v) `p`'s leading dimension is `MAX(n, 0)` in the object; the C++ uses the same stride.
- (v) Index wrap kept as written: `j` drops to `lbp` when it reaches 0 inside the loop
  (`:2437`), `indp` wraps to 1 past `lbp` (`:2451`).
- ⚠ Operand order of `x*p(k,j)` when both are NaN: gfortran multiplies `p*x` in its 2-lane
  body (`mulpd %xmm2,%xmm1`, first source `p`) and in its `n == 1` path, but `x*p` in its scalar
  tail (`mulsd (mem),%xmm4`, first source `x`); g++ puts `x` first at every site whatever the
  source order (tried `p*x`: same object). x86 returns the first source's NaN, so a `y(k)` can
  carry a different NaN payload when `x` and `p(k,j)` are both NaN with different payloads. Not
  fixable from C++ source; no other input differs.
- ⚠ CUDA not compiled this batch.
- (v) Fresh review 2026-10-16: one defect left, the NaN-payload operand order above; recompiled
  `w3profsmd.F90` from `build_lab`'s `flags.make` (identical to the CMake object) and compared its
  GIMPLE and disassembly of `UPPDIR` with a throwaway C++ unit's, site by site (`divsd`, the
  `x /= 0` test, `mulpd`/`mulsd`/`subpd`/`subsd` operands, `cvtsi2sd` then `addsd` for `flops`,
  stride `MAX(n,0)`, index wrap, the copy loop); nothing changed in the header.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest can exercise it: nothing calls `UPPDIR`.
