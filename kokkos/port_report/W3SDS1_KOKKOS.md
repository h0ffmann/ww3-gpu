# W3SDS1_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The WAM cycle 3 whitecapping dissipation of the `ST1` source-term package,
`D = SDSC1 * FMEAN * WNMEAN**3 * EMEAN**2 * K` and `S = D * A`, which `W3SRCE` calls per sea
point (`w3srcemd.F90:1293`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3SDS1`, `model/src/w3src1md.F90:432-586` at NOAA-EMC/WW3 `03adda91`; compiled by 57 regtest switches of `mww3_test_05`, `ww3_tc1`, `ww3_tr1`, `ww3_ts1`, `ww3_ts2`, `ww3_ts3` and by `model/bin/switch_USACE_1` (`git grep -lw ST1 -- '*switch*'`), by no `switch_NCEP_*` or `switch_NWS_*`. Its only `#ifdef` blocks are `W3_S`/`W3_T`/`W3_T0`/`W3_T1` output, so no switch changes its arithmetic |
| Kokkos code | [`../src/ww_kokkos/srce_st1_sds1.hpp`](../src/ww_kokkos/srce_st1_sds1.hpp), `ww::srce::st1::w3sds1_point`; `SDSC1` (W3GDATMD) passed by value, `NSPEC` from `Params` |
| C shim | none yet |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3src1md.F90` in WW3's CMake Release build under `ST1 FLX1`
(gfortran 16.2, switch derived from `switch_lab_shrd`), recompiled with `-fdump-tree-optimized`
and checked identical to the CMake object by disassembly; compared with the GIMPLE and assembly of
a C++ unit under the `openmp-release` flags, 2026-10-16.

- (v) `FACTOR` (`:549`): the object computes `((SDSC1*FMEAN)*(WNMEAN*(WNMEAN*WNMEAN)))*(EMEAN*EMEAN)`,
  `WNMEAN**3` as two `mulss` (`powmult` in the GIMPLE), `EMEAN**2` as one; the C++ writes the same
  five products with the same operands.
- (v) `D = FACTOR*K` (`:557`) and `S = D*A` (`:558`) are element-wise products; the object
  vectorises both loops (4-wide body, 2-wide and scalar epilogue), which cannot change a bit.
- (v) No FMA, no division, no libm or libmvec call, no reduction, no MIN/MAX in either object.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, the point function inside a `Kokkos::parallel_for`;
  only `FMUL` in the SASS, no `FFMA`), never run.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3src1md.F90` from `build_st1`'s
  `flags.make` (object identical to the CMake one) and the header in a throwaway unit under the
  `openmp-release` flags; compared GIMPLE and assembly: the same five `FACTOR` products with the
  same grouping, only multiplications in both loops, no FMA, call, reduction or MIN/MAX; the
  `#ifdef` blocks of `:432-586` are output only; `SDSC1`, `NSPEC` and the argument order match
  the dummies and the call at `w3srcemd.F90:1293`. ⚠ g++ and gfortran pick different operand
  order for some products (`FMEAN*SDSC1` against `SDSC1*FMEAN`); IEEE multiplication is
  commutative, so this can differ only in the payload of a NaN when both operands are NaN.
  Nothing was changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
