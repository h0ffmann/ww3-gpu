# LDSOLC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Solves `L x = y` for a non-unit lower triangular `L` stored in modified sparse column form (`al`,
`jal`, with the diagonal `al(1:n)` already inverted) by column-oriented forward elimination: copy
`y` into `x`, then for each column `k` scale `x(k)` by `al(k)` and subtract `x(k)*al(j)` from every
`x(jal(j))` below it. A SPARSKIT routine kept in `w3profsmd.F90`.

## What was ported

| | |
|---|---|
| WW3 routine | `ldsolc`, `model/src/w3profsmd.F90:3391-3437` at NOAA-EMC/WW3 `03adda91`, 47 lines; no `#ifdef` inside it |
| Callers | none: `git -C WW3-noaa grep -n -i -w ldsolc -- model` finds only its definition, the `PRIVATE` list at `:61-65` and the index comment at `:2847`, so nothing outside the module can call it either. Translated for completeness; it can never be exercised |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:116-125`), so by nearly every switch file, but no regtest can reach a routine nothing calls |
| Kokkos code | [`../src/ww_kokkos/profs_ldsolc.hpp`](../src/ww_kokkos/profs_ldsolc.hpp), `ww::profs::ldsolc`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based holding the Fortran 1-based indices |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) The GIMPLE of both has the same three floating-point operations in the same operand order:
  `x(k)*al(k)` (`:3427`), `t*al(j)` and `x(jal(j)) - (t*al(j))` (`:3430`). The objects emit them as
  `mulsd`/`subsd` (Fortran) and `vmulsd`/`vsubsd` (C++, `-march=x86-64-v3`), the same IEEE double
  operations; `t` stays in a register for the whole column in both, so a `jal(j)` equal to `k`
  does not change `t`.
- ⚠ Only for a NaN in both `t` and `al(j)` with different payloads: the Fortran object computes
  `al(j)*t` in register order (`mulsd %xmm2,%xmm1`, first source `al(j)`), the C++ one `t*al(j)`
  (`vmulsd mem,%xmm2,%xmm1`, first source `t`), and x86 returns the first source's NaN. Writing
  `al[j - 1] * t` in the C++ gives the same instruction (checked), so it is register allocation,
  not the source. Every non-NaN result, and every NaN-ness, is identical.
- (v) The copy loop (`:3423-3425`) is vectorised in both objects (16-byte vectors in the Fortran,
  32-byte with an aliasing check in the C++); it moves data only, so the widths cannot change bits.
- (v) No FMA, no reciprocal, no libm or libmvec call; the elimination loop is not vectorised in
  either object.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. `w3profsmd.F90` recompiled from `build_lab`'s `flags.make` (object identical to the CMake one; no `#ifdef` in `:3391-3437`), GIMPLE and assembly of `__w3profsmd_MOD_ldsolc` read against a fresh C++ unit operation by operation: the same two products and one difference in the same order, `jal(k+1)` read once per column in both, `t` in a register; the only difference is the NaN-payload operand order of `t*al(j)` already marked ⚠ above. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing in WW3 calls `ldsolc`, so the WW3 regtests cannot exercise it.
