# VBRMV_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Computes `b = A x` for a `REAL*8` matrix in variable block row (VBR) form: `kvstr(1:nr+1)` and
`kvstc(1:nc+1)` give the first row and column of each block row and block column, `ia(1:nr+1)`
points into the block column numbers `ja`, and `a` holds the dense blocks column by column, read
in storage order through the running counter `k`. A SPARSKIT routine (Y. Saad) kept in
`w3profsmd.F90`.

## What was ported

| | |
|---|---|
| WW3 routine | `vbrmv`, `model/src/w3profsmd.F90:3203-3251` at NOAA-EMC/WW3 `03adda91`, 49 lines; no `#ifdef` inside it |
| Callers | none: `git grep -wi vbrmv` in `model/src` finds only its definition, the module's `PRIVATE` list at `:63` and the index comment at `:2840`. It is translated for completeness and can never be exercised |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json`), so by nearly every switch file, `switch_lab_shrd` included |
| Kokkos code | [`../src/ww_kokkos/profs_vbrmv.hpp`](../src/ww_kokkos/profs_vbrmv.hpp), `ww::profs::vbrmv`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based holding the Fortran 1-based indices; `ka` is declared and never read, in the Fortran as here |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) The zeroing of `b(1:n)` (`:3231`) is vectorised in both objects (16-byte stores in the
  Fortran, 32-byte in the C++); every element gets `+0.0` either way.
- (v) The accumulation (`:3243`) is scalar in both: per element one multiply of `a(k)` and `xjj`
  (`mulsd` / `vmulsd`), then the product plus `b(ii)` (`addsd` / `vaddsd`, product as first
  operand in both) stored back to `b(ii)`, in the Fortran's order of `i`, `j`, `jj`, `ii`. `xjj`
  is loaded once per `jj` in both.
- (v) The bounds `ia(i+1)-1`, `kvstc(ja(j)+1)-1` and `kvstr(i+1)-1` are read once per loop entry,
  as the Fortran `DO` defines.
- (v) No FMA, no libm or libmvec call, no reduction.
- ⚠ Operand order of the multiply: the Fortran object computes `a(k) * xjj` (`a(k)` in the
  destination register), the C++ object `xjj * a(k)`; g++ emits the same instruction whichever
  order the source writes. The product's bits differ only when both operands are NaN with
  different payloads (x86 returns the first source's NaN).
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. The Fortran object, recompiled from the `build_lab`
  `flags.make`, is identical to the CMake object; its `vbrmv` GIMPLE and assembly were compared
  with the C++ unit's operation by operation (zeroing, `a(k)*xjj` then product plus `b(ii)`, one
  `xjj` load per `jj`, one `a(k)` and `b(ii)` load per `ii`, so the memory order matches even if
  a caller aliased `b` with `x` or `a`). The multiply's operand order stays ⚠: g++ canonicalises
  it to `xjj * a(k)` whether the source writes `a[k - 1] * xjj` or loads `a(k)` into a local
  first (both tried), so it differs from the Fortran only when both operands are NaN.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing in WW3 calls `vbrmv`, so no regtest can reach it.
