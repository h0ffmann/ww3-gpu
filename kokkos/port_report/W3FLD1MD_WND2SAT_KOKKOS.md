# W3FLD1MD_WND2SAT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Saturation level of the 1-D spectral tail as a function of the 10 m neutral wind, a piecewise
polynomial fitted to the HWRF 2016 drag-coefficient curve and clipped to [0.002, 0.014]. Its only
caller is `W3FLD1` when `Tail_Choice = 1` (`w3fld1md.F90:271-275`), which `W3SRCE` calls under
switch `FLD1` (`w3srcemd.F90:2284`). `Tail_Choice` is `TAIL_ID` from the grid namelist, whose
default `TAILTYPE = 0` (`w3gridmd.F90:3203`) takes the constant tail instead; no `ww3_ts3` input
sets it, so no regtest exercises this routine ⚠ (`git grep -i tailtype -- regtests/ww3_ts3` is
empty). `W3FLD2MD` has its own `WND2SAT` with other coefficients (`w3fld2md.F90:592-687`),
translated separately ([`W3FLD2MD_WND2SAT_KOKKOS.md`](W3FLD2MD_WND2SAT_KOKKOS.md)).

## What was ported

| | |
|---|---|
| WW3 routine | `WND2SAT` of `W3FLD1MD`, `model/src/w3fld1md.F90:1377-1473` at NOAA-EMC/WW3 `03adda91`; compiled by the `FLD1` switches of `ww3_ts3` (`switch_ST4_PR1_FLD1`, `_MPI`, `switch_ST6_PR1_FLD1`, `_MPI`) (`git grep -lw FLD1 -- '*switch*'`); by no `switch_NCEP_*` or `switch_NWS_*`. Its only `#ifdef` is `W3_S` (tracing) |
| Kokkos code | [`../src/ww_kokkos/fld1_wnd2sat.hpp`](../src/ww_kokkos/fld1_wnd2sat.hpp), `ww::fld1::wnd2sat(Real wnd10, Real& sat)` |
| C shim | none yet |
| Plan | [`EASY_BATCH_KOKKOS_PLAN_202610.md`](../../docs/EASY_BATCH_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3fld1md.F90` from WW3's CMake Release build under the batch's
`build_fld1` (`ww3_ts3/input/switch_ST4_PR1_FLD1`) and `build_mixa` (`switch_lab_shrd` plus
`FLD2`) switches, recompiled with `-fdump-tree-optimized-lineno` and checked identical to each
CMake object; the two preprocessed sources are the same. Against it, a C++ unit with the
`openmp-release` flags (g++ 16.2, `-ffp-contract=off`), compiled warning-free, 2026-10-16.

- (v) The optimised GIMPLE of the two is the same operation for operation, with the same float
  constants and operand order: `WND10**2` as `W*W`, `WND10**3` as `W*(W*W)`, `WND10**4` as
  `(W*W)*(W*W)`. gfortran rewrites `-A*X2 + B*X` as `B*X - A*X2` and `-A*W + C` as `C - A*W`
  (exact in IEEE arithmetic); the C++ is written in gfortran's order.
- (v) `MIN(MAX(SAT,0.002),0.014)` (`:1472`) is `maxss 0.002,s` then `minss 0.014,m` in the
  Fortran object: `s > 0.002 ? s : 0.002`, then `m < 0.014 ? m : 0.014`. The C++ states the same
  two selects; g++ emits them as a compare and branch plus `vminss`, with the same result for
  every input, NaN included (a NaN gives 0.002).
- (v) A NaN `WND10` fails both `<` tests (`comiss`) and takes the linear branch in both objects.
- (v) No FMA, no libm or libmvec call, no reduction in either object.
- ⚠ Not compiled for CUDA.
- (v) Fresh review 2026-10-16: no defect. `w3fld1md.F90` recompiled from the `build_fld1` and `build_mixa` flags, both identical to the CMake objects, same preprocessed source; its optimised GIMPLE compared with a C++ unit including `fld1_wnd2sat.hpp` and `fld2_wnd2sat.hpp` together (warning-free): same operations, operand order and float constants in all three branches, the clamp equivalent for every input (NaN gives 0.002). The only regtest grid that sets `TAILTYPE=1` (`ww3_ufs1.2/input/ww3_grid_hafsl.inp`) builds with `FLD2`, where the `FLD1` namelist is not read (`w3gridmd.F90:3223`), so the routine stays unexercised. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
