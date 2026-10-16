# ATMUX_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Computes `y = transpose(A) x` for a `REAL*8` matrix in compressed sparse row form: `ia(1:n+1)`
the row pointers, `a(k)`, `ja(k)` the values and column numbers. A SPARSKIT routine (Y. Saad)
kept in `w3profsmd.F90`.

## What was ported

| | |
|---|---|
| WW3 routine | `atmux`, `model/src/w3profsmd.F90:2946-2994` at NOAA-EMC/WW3 `03adda91`, 49 lines; no `#ifdef` inside it |
| Callers | one: `runrc` (`:3861`), on the branch `ipar(1).eq.2` of its reverse-communication loop. `runrc` is called only from `W3XYPFSNIMP` (`:1239`) with `bcgstab` as solver, and `bcgstab` (`:2049`) never sets `ipar(1) = 2` (it sets 1, 3, 5 and 10), so no WW3 run reaches `atmux`. Translated for completeness |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json`), so by nearly every switch file, `switch_lab_shrd` included |
| Kokkos code | [`../src/ww_kokkos/profs_atmux.hpp`](../src/ww_kokkos/profs_atmux.hpp), `ww::profs::atmux`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based holding the Fortran 1-based indices |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) Zeroing loop (`:2979-2981`): both objects vectorise it into stores of `0.0` (Fortran by 2
  with `movups`, C++ by 4 with `vmovupd`) with a scalar tail. Only stores of +0.0, so the bits match.
- (v) Product loop (`:2985-2989`): both objects load `x(i)` once per row and, for every `k`, load
  `y(ja(k))`, form one `mulsd` of `x(i)` and `a(k)`, one `addsd` onto `y(ja(k))`, and store it back,
  in the Fortran's `k` order. Not vectorised in either object, no reduction reordered.
- (v) No FMA, no libm or libmvec call.
- ⚠ The Fortran object multiplies `a(k)*x(i)` and the C++ one `x(i)*a(k)` (operand order of
  `mulsd`). IEEE multiplication is commutative, so the result differs only in which NaN payload
  propagates when both operands are NaN.
- ⚠ Only `y(1:n)` is zeroed, as in the Fortran; for a non-square `A` the entries of `y` beyond `n`
  keep what the caller left there. Reproduced, not fixed.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect that changes bits for non-NaN input. The `w3profsmd.F90`
  object was rebuilt from `build_lab`'s `flags.make` and found identical to the CMake object, and
  `__w3profsmd_MOD_atmux` was read against the C++ unit instruction by instruction (zeroing, the
  `ia` bounds, the hoisted `x(i)`, the `mulsd`/`addsd` scatter in `k` order, the `addsd` first
  source being the product in both). The one difference is the `mulsd` first source above: the
  Fortran object has `a(k)`, the C++ one `x(i)`, so when both are NaN with different payloads the
  result carries the other payload. Writing `a[k - 1] * xi` in the source gave the same
  `vmulsd` (g++ canonicalises the operands), so it stays ⚠; nothing was changed in the header.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No WW3 run reaches `atmux` (see Callers), so the WW3 regtests cannot
exercise it.
