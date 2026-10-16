# MINLAM_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The smallest allowed lambda of a Generalized Multiple DIA (`NL3`) quadruplet with parameters
`MU` and `THETA` (degrees), a function internal to `W3SNL3MD`. `INSNL3` is its only caller: the
range test of a user quadruplet (`w3snl3md.F90:985`) and the diagnostic `WRITE` before `EXTCDE(1002)`
(`:988`). It was translated with `INSNL3` in an earlier pull request
([`INSNL3_KOKKOS.md`](INSNL3_KOKKOS.md)); this report records its own object comparison.

## What was ported

| | |
|---|---|
| WW3 routine | `MINLAM`, `model/src/w3snl3md.F90:1525-1578` at NOAA-EMC/WW3 `03adda91`, 54 lines, no `#ifdef`; `NL3` is compiled by `switch_NCEP_glwu`, `switch_UoM_nl3`, `switch_UoM_nl3s` and the `ww3_ts1` regtest switch `switch_ST4_GMD` (`git grep -lw NL3 -- '*switch*'`) |
| Kokkos code | [`../src/ww_kokkos/snl3_tables.hpp`](../src/ww_kokkos/snl3_tables.hpp), `ww::snl3::insnl3_detail::minlam`, an `inline` host function (`INSNL3` builds its tables on the host) |
| C shim | none; `INSNL3`'s table builder calls it |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3snl3md.F90` from WW3's CMake Release build, gfortran 16.2.0,
`switch_lab_shrd` with `NL1` replaced by `NL3`; the object recompiled with that build's flags is
identical to the CMake object except for its file name (v, 2026-10-16). C++: a unit calling
`minlam` from a non-inlined wrapper, built with the `openmp-release` flags (`-O3 -march=x86-64-v3
-ffp-contract=off`), warning-free.

- (v) gfortran keeps `MINLAM` as one out-of-line function, `minlam.1.isra.0`, called at `:985`
  and `:988`; no inlined copy exists in `INSNL3`.
- (v) The C++ object runs the same operations in the same order: `MIN(1.,MU)` as `minss` with
  `MU` in the destination, then `MAX(0.,.)` as `maxss` with the value in the destination (a NaN
  `MU` gives 1, then 1); `THETA*ATAN(1.)/45.` with `ATAN(1.)` folded to `0x3f490fdb`;
  `(1.+MULOC)**4` and `(1.-MULOC)**4` as the squares of the squares, `2.*(1.+MULOC)**2` as an
  `addss` of the square with itself; `((a4+b4) + ((2a2)*b2)*COS(THETAR))`; `MAX(.,0.)` before both
  `SQRT`, value in the destination. The constants `1.`, `ATAN(1.)`, `45.`, `0.5` have the same
  bits in both objects.
- (v) `COS` is a scalar call to glibc's `cosf` in both; `SQRT` is `sqrtss`. No FMA, no reciprocal,
  no libmvec call, no reduction. `THETA .LT. 0.` is `comiss`/`ja` in both, so a NaN `THETA` takes
  the computed branch in both.
- (v) No defect found; the function was not changed.
- (v) Fresh review 2026-10-16: no bit defect. The module recompiled from the `build_nl3` flags gives a disassembly identical to the CMake object; the `minlam.1.isra.0` GIMPLE and assembly were read against a new C++ unit operation by operation (`minss`/`maxss` operand order, folded `0x3f490fdb`, `45.`, `0.5`, power tree, `cosf`, `sqrtss`), all equal. Fixed in this report: the exit after `:988` is `EXTCDE(1002)` (`:989`), not 1001.
- ⚠ CUDA: host-only function; the header was compiled for CUDA (`sm_89`, `--fmad=false`) with
  `INSNL3`, not again here.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
