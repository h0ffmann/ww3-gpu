# WND2Z0M_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Bulk momentum roughness length z0 from the 10 m neutral wind: the 2016 HWRF fit, a piecewise
polynomial in `U10` (`znot_m_v1`, `w3fld1md.F90:1351-1361`) followed by the `icoef_sf=4` drag
reduction of `znot_wind10m` (`:1366-1373`), with `U10` capped at 85 m/s (`:1348`). Callers:
`W3FLD2` calls it once per point as the first guess of z0 (`w3fld2md.F90:249`) and again as the
bulk fail-safe (`w3fld2md.F90:577`); `W3FLD1` calls it only in its fail-safe branch
(`w3fld1md.F90:767`). The routine lives in `W3FLD1MD`, which both switches compile.

## What was ported

| | |
|---|---|
| WW3 routine | `WND2Z0M` of `W3FLD1MD`, `model/src/w3fld1md.F90:1251-1375` at NOAA-EMC/WW3 `03adda91`; compiled by the `FLD1` switches of `ww3_ts3` (`switch_ST4_PR1_FLD1`, `_MPI`, `switch_ST6_PR1_FLD1`, `_MPI`) and the `FLD2` ones (`ww3_ts3/input/switch_ST4_PR1_FLD2`, `_MPI`, `switch_ST6_PR1_FLD2`, `_MPI`, `ww3_ufs1.1/input/switch_MPI`, `switch_MPI_OMPH`, `input_unstr/switch_PDLIB`, `switch_PDLIB_TRNK`, `ww3_ufs1.2/input/switch_MPI_OMPH`, `ww3_ufs1.3/input/switch_MPI_OMPH`, `model/bin/switch_multi_esmf`) (`git grep -lw FLD1\|FLD2 -- '*switch*'`); by no `switch_NCEP_*` or `switch_NWS_*`. Its only `#ifdef` is `W3_S` (tracing) |
| Kokkos code | [`../src/ww_kokkos/fld1_wnd2z0m.hpp`](../src/ww_kokkos/fld1_wnd2z0m.hpp), `ww::fld1::wnd2z0m(Real w10m, Real& znotm)` |
| C shim | none yet |
| Plan | issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3fld1md.F90` from WW3's CMake Release build under the batch's
`build_mixa` (`switch_lab_shrd` plus `FLD2`) and `build_fld1` (`ww3_ts3/input/switch_ST4_PR1_FLD1`)
switches, recompiled with `-fdump-tree-optimized-lineno` and checked identical to each CMake
object; the routine's assembly is the same in both. Against it, a C++ unit with the
`openmp-release` flags (g++ 16.2, `-ffp-contract=off`), compiled warning-free, 2026-10-16.

- (v) gfortran folds the whole `U10 > 85` path (`U10 = 85`, then polynomial, `ALOG`, `SQRT`,
  `EXP` evaluated at compile time) into the constant `3.507997025735676e-4` (`0x1.6fd714p-12`),
  a PHI operand of the optimised GIMPLE and a `.rodata` load in the object. The C++ returns that
  constant for `W10M > 85` instead of calling `logf`/`expf` at run time, whose result could
  differ from the compile-time one.
- (v) The optimised GIMPLE of the other branches is the same operation for operation, with the
  same float constants and operand order: `U10**2` as `U*U` once, `U10**3` as `U*U2`, `**4` as
  `U2*U2`, `**5` as `U3*U2`, `**6` as `U3*U3`; `0.0185/9.8`, `0.4*0.4`
  (`0x1.47ae16p-3`, not 0.16), `0.75-0.99` and `45-20` folded to one float each; the division by
  25 stays. gfortran writes `bs6 + bs5*U` as `bs5*U - |bs6|`, exact in IEEE arithmetic; g++ does
  the same.
- (v) Both objects call `logf` and `expf` from libm once per evaluated branch, no libmvec. The
  Fortran's `SQRT` is an inline `sqrtss`; the C++ is an inline `sqrtss` with a `sqrtf` call on a
  negative or NaN argument only (errno), which the argument `0.16/(TMP*AAA)` with `TMP, AAA > 0` never
  is, and which returns the same NaN.
- (v) A NaN `W10M` fails every `comiss` test, takes the `ELSE` (`cf`) polynomial and `AAA = 0.75`
  in both. No `MIN`/`MAX`, no reduction, no FMA.
- (v) Fresh review 2026-10-16: no arithmetic defect. Both objects recompiled (`build_mixa`, `build_fld1`),
  identical to the CMake objects and to each other for this routine; every branch of the optimised GIMPLE
  compared with the C++ unit's, same float constants (including the folded `U10 > 85` result), same
  power tree and operand order (`u2*u3` for `U10**5` is the commutative swap), same `logf`/`expf`
  relocations, no libmvec; only `W3_S` is an `#ifdef`. Fixed in this report: three `FLD2` switches
  missing from the list (`switch_PDLIB_TRNK`, `ww3_ufs1.2`, `ww3_ufs1.3`) and the `sqrtf` errno path
  also taken on NaN.
- ⚠ On a GPU, `logf` and `expf` are CUDA's, not glibc's. Not compiled for CUDA.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
