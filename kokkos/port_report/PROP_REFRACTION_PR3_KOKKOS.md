# PROP_REFRACTION_PR3_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The refraction velocity in direction (`CAD`, the `PR3` form) of one node of the unstructured grid, for the implicit scheme's spectral matrix.
It is called per local node by `CALCARRAY_JACOBI_SPECTRAL_1` (`w3profsmd_pdlib.F90:4515,4542`)
and `CALCARRAY_JACOBI_SPECTRAL_2` (`:4670,4699`).

## What was ported

| | |
|---|---|
| WW3 routine | `PROP_REFRACTION_PR3`, `model/src/w3parall.F90:447-589` at NOAA-EMC/WW3 `03adda91`, as `WW3/regtests/ww3_tp2.17/input/switch_PDLIB` compiles it; in the PDLIB profile (`docs/data/ww3_pdlib_gprof.tsv`) |
| Kokkos code | [`../src/ww_kokkos/prop_parall.hpp`](../src/ww_kokkos/prop_parall.hpp), shared with its sibling routine of `W3PARALL` |
| C shim | none yet |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch A, issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3parall.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_PDLIB`), rebuilt by the translator and by a fresh reviewer identical to the CMake build's,
2026-10-16.

- (v) Every product and sum in the Fortran's order (the reviewer listed each: `VCFLT`, `VELNOFILT`,
  `FRK`, `FKC`, `FKD`, `FKD0`, `CWNB`); `FACTH = 1/DTH` and `CTMAX/DTG` true divisions; all
  `REAL`; no FMA, no libm call.
- (v) `MIN`/`MAX` as the object has them: depth `DW>DMIN?DW:DMIN`, `DSDD` `t>0?t:0` (−0 and NaN
  included), the refraction limiter `|v|<C?|v|:C` then `copysign` (the C++ blends on `v` before
  `copysign`, the same bits); the `< 5.` test sends NaN to the `ELSE` in both.
- (v) Run-time branches both ways (`FLCUR`, the limiter, `LPDLIB` in `M2`); the `DSDD` code of
  `:533-540` and `:866-873` compiles to the same sequence and is shared; index formulas match the
  allocations (`w3gdatmd.F90:1736,1743,2058,2066,2068`, `w3adatmd.F90:1312,1325,1374-1392`).
- (v) Callers declare `CAD(NSPEC)`, `CWNB_M2(1-NTH:NSPEC)`, `DWNI_M2(NK)` (`w3profsmd_pdlib.F90:4479-4481`),
  which the device functions' contracts match.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, no `fma` in the PTX), never run; nvcc's `max.f32` on
  −0/+0 not compared with `maxss`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
