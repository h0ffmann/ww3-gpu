# MAXLAM_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The largest allowed `lambda` of a Generalized Multiple DIA (switch `NL3`) quadruplet, given its
`mu` and its angle `theta` in degrees. An internal function of `INSNL3`, which checks every
quadruplet of the namelist against it once per grid (`w3snl3md.F90:983-990`).

## What was ported

| | |
|---|---|
| WW3 routine | `MAXLAM`, `model/src/w3snl3md.F90:1593-1645` at NOAA-EMC/WW3 `03adda91`, 53 lines, contained in `INSNL3`; called only there, at `:986` (the check) and `:988` (the `WRITE` before `EXTCDE(1002)`). `NL3` is compiled by `switch_NCEP_glwu`, `switch_UoM_nl3`, `switch_UoM_nl3s` and the `ww3_ts1` regtest switch `switch_ST4_GMD` (`git grep -lw NL3 -- '*switch*'`). It has no `#ifdef` block |
| Kokkos code | [`../src/ww_kokkos/snl3_tables.hpp`](../src/ww_kokkos/snl3_tables.hpp), `ww::snl3::insnl3_detail::maxlam`, host `inline`, written with `INSNL3` ([`INSNL3_KOKKOS.md`](INSNL3_KOKKOS.md)); this report checks it on its own. Not changed: no bit defect found |
| C shim | none of its own; `INSNL3`'s shim is its caller, and the `WRITE` of `:987-988` is the shim's job on `kInsnl3Quad` |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3snl3md.F90` built by WW3's CMake Release flags with gfortran
16.2.0, `switch_lab_shrd` with `NL1` replaced by `NL3`; recompiled with `-fdump-tree-optimized-lineno`,
its disassembly identical to the CMake object's. gfortran inlines both calls into `INSNL3` (no
`maxlam` symbol), and the two copies were read against a throwaway C++ unit calling `maxlam`
(the openmp-release flags, `-ffp-contract=off`, warning-free), 2026-10-16.

- (v) `THETA .LT. 0.` is `comiss`/`ja` in the object and `vcomiss`/`ja` in the C++: a NaN `THETA` takes
  the `ELSE` branch in both.
- (v) `MAX(0., MIN(1., MU))` (`:1633`): `minss` with `MU` in the destination and 1 as source, then `maxss`
  with 0 as source, in both copies and in the C++ (`minss(mu, 1)`, `maxss(., 0)`): a NaN `MU` gives
  `MULOC = 1`. `MAX(BB, 0.)` (`:1637`) the same: a NaN `BB` gives 0.
- (v) `THETAR = THETA * ATAN(1.) / 45.`: `ATAN(1.)` folded to `0x1.921fb6p-1` (`insnl3_detail::kAtan1`),
  a multiply then a divide, as in the C++.
- (v) `(1.+MULOC)**4`, `(1.-MULOC)**4`, `(1.+MULOC)**2`, `(1.-MULOC)**2`: the power tree squares each base
  once and squares the square, the C++'s `a2*a2`, `b2*b2`. `2.*A2*B2*COS(THETAR)` is `(A2+A2)*B2*cos` in
  both objects, left to right. One `cosf` call per copy, no libmvec, no FMA.
- (v) The object adds the two fourth powers as `B4 + A4` in both copies; the C++ adds `A4 + B4`. With
  `MULOC` clamped to `[0, 1]` both are finite, and an IEEE sum of two finite floats is commutative, so
  the bits are the same. The last sum and the final `0.25 * BB` match operand for operand.
- (v) Fresh review 2026-10-16: no defect. The two inlined copies in `__w3snl3md_MOD_insnl3` (`:1630-1638`, GIMPLE and assembly) were read against a new C++ unit: same operations, `minss`/`maxss` operand order, constants `0x3f490fdb`, `45.`, `0.25`, `0.5`. In the GIMPLE the first copy sums `A4 + B4` and the second `B4 + A4`; both finite, so the bits agree. Nothing changed.
- ⚠ CUDA not compiled (optional this batch); the function is host-only, as `INSNL3` is.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
