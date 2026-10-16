# W3FLD2MD_WND2SAT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Saturation level of the 1-D spectral tail as a function of the 10 m neutral wind, a piecewise
polynomial fitted to the HWRF 2016 drag-coefficient curve and clipped to [0.002, 0.014]. Its only
caller is `W3FLD2` when `Tail_Choice = 1` (`w3fld2md.F90:278-279`), which `W3SRCE` calls under
switch `FLD2` (`w3srcemd.F90:2293`). `W3FLD1MD` has its own `WND2SAT` (`w3fld1md.F90:1377-1473`),
translated separately.

## What was ported

| | |
|---|---|
| WW3 routine | `WND2SAT` of `W3FLD2MD`, `model/src/w3fld2md.F90:592-687` at NOAA-EMC/WW3 `03adda91`; compiled by the `FLD2` switches of `ww3_ts3` (`switch_ST4_PR1_FLD2`, `_MPI`, `switch_ST6_PR1_FLD2`, `_MPI`), `ww3_ufs1.1` (`switch_MPI`, `switch_MPI_OMPH`, `input_unstr/switch_PDLIB`, `switch_PDLIB_TRNK`), `ww3_ufs1.2` and `ww3_ufs1.3` (`switch_MPI_OMPH`), and `model/bin/switch_multi_esmf` (`git grep -lw FLD2 -- '*switch*'`); by no `switch_NCEP_*` or `switch_NWS_*`. Its only `#ifdef` is `W3_S` (tracing) |
| Kokkos code | [`../src/ww_kokkos/fld2_wnd2sat.hpp`](../src/ww_kokkos/fld2_wnd2sat.hpp), `ww::fld2::wnd2sat(Real wnd10, Real& sat)` |
| C shim | none yet |
| Plan | [`EASY_BATCH_KOKKOS_PLAN_202610.md`](../../docs/EASY_BATCH_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3fld2md.F90` from WW3's CMake Release build under the batch's
`build_mixa` switch (`switch_lab_shrd` plus `FLD2`), recompiled with `-fdump-tree-optimized-lineno`
and checked identical to the CMake object; against it, a C++ unit with the `openmp-release` flags
(g++ 16.2, `-ffp-contract=off`), compiled warning-free, 2026-10-16.

- (v) The optimised GIMPLE of the two is the same operation for operation, with the same float
  constants: `WND10**2` as `W*W`, `WND10**3` as `W*(W*W)`, `WND10**4` as `(W*W)*(W*W)`, and the
  sums in gfortran's order. gfortran rewrites `-A*X + B*Y` as `B*Y - A*X` and `-A*W + C` as
  `C - A*W` (exact in IEEE arithmetic); the C++ is written in that order so the GIMPLE matches.
- (v) `MIN(MAX(SAT,0.002),0.014)` is `maxss 0.002,s` then `minss 0.014,m` in the Fortran object:
  `s > 0.002 ? s : 0.002`, then `m < 0.014 ? m : 0.014`. The C++ states the same two selects;
  g++ emits them as a compare and branch plus `vminss`, with the same result for every input,
  NaN included (a NaN gives 0.002).
- (v) A NaN `WND10` fails both `<` tests and takes the linear branch in both objects.
- (v) No FMA, no libm or libmvec call, no reduction in either object.
- ⚠ Not compiled for CUDA.
- (v) Fresh review 2026-10-16: no defect. The Fortran object was recompiled from `build_mixa`'s flags
  and found identical to the CMake object; its `WND2SAT` is not inlined into `W3FLD2` (`:279` stays a
  call). Against a C++ unit with the `openmp-release` flags (warning-free): the same float constants
  in GIMPLE and `.rodata`, the same multiply and subtract order in each branch, and `maxss`/`minss`
  matched by the compare-and-branch plus `vminss` for every input, NaN and -0 included. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
