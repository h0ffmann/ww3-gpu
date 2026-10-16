# PROP_REFRACTION_PR1_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The refraction velocity in direction (`CAD`, the `PR1` form, without the `CTMAX` limiter) of one
sea point, indexed through `MAPSF` as on a structured grid. Nothing in WW3 calls it: both calls
are commented out in `CALCARRAY_JACOBI_SPECTRAL_1` and `_2` (`w3profsmd_pdlib.F90:4540`, `:4697`),
which call `PROP_REFRACTION_PR3` instead ([PROP_REFRACTION_PR3_KOKKOS.md](PROP_REFRACTION_PR3_KOKKOS.md)).
It is translated for completeness and cannot be exercised by any regtest.

## What was ported

| | |
|---|---|
| WW3 routine | `PROP_REFRACTION_PR1`, `model/src/w3parall.F90:285-431` at NOAA-EMC/WW3 `03adda91`; `w3parall.F90` is compiled by every switch, the routine has no caller. `#ifdef` blocks: `W3_S` (tracing only) and `W3_REFRX` (`:399-411`, 25 regtest switch files, all under `ww3_tic1.3`; no NOAA operational switch; `git grep -lw REFRX -- '*switch*'`) |
| Kokkos code | [`../src/ww_kokkos/parall_prop_refraction_pr1.hpp`](../src/ww_kokkos/parall_prop_refraction_pr1.hpp), `ww::parall::prop_refraction_pr1<Refrx>`; reads the module arrays through `ww::pdlib::PropParallArrays` and shares `dsdd_fill` and the work-buffer size with its `PR3` sibling in `prop_parall.hpp` |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3parall.F90`, WW3's CMake Release flags, gfortran 16.2, rebuilt
in the pinned shell and identical to the CMake objects of `switch_lab_shrd` (PR3),
`ww3_ts3/input/switch_ST4_PR1_FLD1` (PR1) and `ww3_tp2.6/input/switch_PDLIB` + SETUP (PDLIB, MPI),
2026-10-16. The routine's code is the same in all three (identical disassembly up to addresses).
The C++ unit was compiled with the `openmp-release` flags and `-ffp-contract=off`, warning-free.

- (v) Every product, sum and quotient in the Fortran's order, from both `-O3` GIMPLE dumps:
  `FACTH = DTG/DTH`, `FDG = FACTH*CTHG0S`, `FRK = (FACTH*DSDD)/WN`, `FRG = FDG*CG`,
  `VCFLT = FRG*ECOS + FRK*(ESIN*DDDX - ECOS*DDDY)`, the current terms
  `((VCFLT + ES2*DCYX) + ESC*DCXXYY) - EC2*DCXY` with `DCXXYY = FACTH*(DCXDX - DCYDY)`. No FMA in
  either object, no libm call, no reduction. The vectorised loops of both are element-wise.
- (v) `MAX` as the object has them: depth `DW>DMIN?DW:DMIN` (`maxss (DMIN),DW`; `vmaxss` with `DW`
  first in the C++), `DSDD` `t>0?t:0`; `DEPTH*WN < 5.` is `comiss` with NaN to the `ELSE`.
- (v) `FDDMAX` (`:385-388`) is dead (its use at `:391` is commented out) and absent from the
  object; not translated. `CAD = DBLE(VCFLT)` into a `REAL` `CAD` keeps the bits.
- (v) `W3_REFRX`: translated as `Refrx = true`, checked against an object of the same source built
  with the PR1/FLD1 flags plus `-DW3_REFRX` (no reference build has that switch):
  `FRK = ((FACTH*CG)*WN)/SIG`, the first `VCFLT` loop still stored and then overwritten, as in
  the object.
- ⚠ `W3_REFRX` reads `DCDX(ISP,1,MAPWN(ISP))` (`:408-409`): the first index runs to `NSPEC` in a
  dimension `0:NK+1`, the third takes `IK` in a dimension `NX` or `NSEAL` (`w3adatmd.F90:1377`,
  `:1386`). WW3 reads outside the array's bounds; the C++ reads the same linear element.
- ⚠ Under `LPDLIB` the `DDDX..DCYDY` arrays are `(1,NSEAL)` (`w3adatmd.F90:1384-1392`) while the
  routine indexes them `(IY,IX)` from `MAPSF`, which reads past them when `IX > NSEAL`. Reproduced.
- ⚠ CUDA not compiled for this routine.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest can exercise it: WW3 never calls it.
