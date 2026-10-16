# UDSOLC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Solves `U x = y` for a non-unit upper triangular `U` stored in modified sparse column form (`au`,
`jau`) with its diagonal already inverted in `au(1:n)`, by column-oriented backward elimination:
copy `y` into `x`, then for each column `k` from `n` down to 1 scale `x(k)` by `au(k)` and
subtract `x(k)*au(j)` from every `x(jau(j))` above it. A SPARSKIT routine kept in `w3profsmd.F90`.

## What was ported

| | |
|---|---|
| WW3 routine | `udsolc`, `model/src/w3profsmd.F90:3626-3671` at NOAA-EMC/WW3 `03adda91`, 46 lines; no `#ifdef` inside it |
| Callers | none: `git -C WW3-noaa grep -n -i udsolc -- model` finds only its definition, the `PRIVATE` list at `:61-65` and a comment at `:2852`, so nothing outside the module can call it either. Translated for completeness; it can never be exercised |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:115-126`), so by nearly every switch file, but no regtest can reach a routine nothing calls |
| Kokkos code | [`../src/ww_kokkos/profs_udsolc.hpp`](../src/ww_kokkos/profs_udsolc.hpp), `ww::profs::udsolc`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based holding the Fortran 1-based indices |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) The diagonal step (`:3661`) is one multiply in both objects, `x(k)` loaded and multiplied by
  `au(k)` (`mulsd` in the Fortran object, `vmulsd` in the C++ one from `-march=x86-64-v3`), stored
  back and kept in a register as `t` (`:3662`).
- (v) The elimination loop (`:3663-3665`) emits the same sequence per `j` in both: load `au(j)`,
  multiply by `t`, load `x(jau(j))`, subtract the product, store. `t` is not reloaded inside the
  column, so a `jau(j)` equal to `k` does not change it in either object.
- (v) The copy loop (`:3657-3659`) is vectorised in both objects (16-byte vectors in the Fortran,
  32-byte with an aliasing check in the C++); it moves data only, so the widths cannot change bits.
- (v) No FMA, no reciprocal, no libm or libmvec call; the elimination loop is not vectorised in
  either object.
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3profsmd.F90` under `switch_lab_shrd` (identical to the CMake object; no `#ifdef` in the routine) and the C++ unit with the openmp-release flags (warning-free), and compared the two disassemblies instruction by instruction: same copy, same `x(k)*au(k)` scaling held as `t`, loop bounds `jau(k)`, `jau(k+1)-1` read once before the store, same `x(jau(j)) - t*au(j)` per `j`, no FMA. Fixed the Fortran line cites, off by two in the header and here, and the `switches.json` range.
- ⚠ CUDA not compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing in WW3 calls `udsolc`, so the WW3 regtests cannot exercise it.
