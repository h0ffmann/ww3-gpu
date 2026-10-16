# W3SIN1_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The WAM cycle 3 wind input of the `ST1` source-term package (Snyder et al. 1981, Komen et al.
1984), `D = SINC1 * SIG2 * MAX(0, USTAR*cos(theta - USDIR)*K/SIG2 - 0.035714)` and `S = D * A`,
which `W3SRCE` calls per sea point (`w3srcemd.F90:1227`); the point post-processors `ww3_outp`
(`:2228`), `ww3_ounp` (`:2391`) and `gx_outp` (`:1157`) call it too.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SIN1`, `model/src/w3src1md.F90:255-417` at NOAA-EMC/WW3 `03adda91`; compiled by 57 regtest switches of `mww3_test_05`, `ww3_tc1`, `ww3_tr1`, `ww3_ts1`, `ww3_ts2`, `ww3_ts3` and by `model/bin/switch_USACE_1` (`git grep -lw ST1 -- '*switch*'`), by no `switch_NCEP_*` or `switch_NWS_*`. Its only `#ifdef` blocks are `W3_S`/`W3_T`/`W3_T0`/`W3_T1` output, so no switch changes its arithmetic |
| Kokkos code | [`../src/ww_kokkos/srce_st1_sin1.hpp`](../src/ww_kokkos/srce_st1_sin1.hpp), `ww::srce::st1::w3sin1_point`; `SINC1` (W3GDATMD) by value, `SIG2`, `ECOS`, `ESIN` (W3GDATMD, 1-based in Fortran; `SIG2` is `NSPEC` long, `ECOS` and `ESIN` `NSPEC+NTH`, `w3gdatmd.F90:2061-2067`, of which the first `NSPEC` are read) as 0-based pointers at element 1, `NSPEC` from `Params` |
| C shim | none yet |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3src1md.F90` in WW3's CMake Release build under `ST1 FLX1`
(gfortran 16.2, switch derived from `switch_lab_shrd`), recompiled with `-fdump-tree-optimized`
and checked identical to the CMake object by disassembly; compared with the GIMPLE and assembly of
a C++ unit under the `openmp-release` flags, 2026-10-16.

- (v) `COSU`, `SINU` (`:378-379`): the object calls `sincosf` once (`__builtin_cexpif` in the
  GIMPLE); the C++ object folds `Kokkos::cos` and `Kokkos::sin` of the same argument into the same
  single `sincosf` call.
- (v) `D` (`:383-387`): both objects compute
  `(SINC1*SIG2) * max(((USTAR*(ECOS*COSU + ESIN*SINU))*K)/SIG2 - 0.035714f, 0)` with the same
  operands per operation (products commuted only); `0.035714` is a default `REAL` literal, stored
  as `0x3d1248d8` in both, added negated in the vector body and subtracted in the scalar tail,
  which gives the same bits.
- (v) `MAX(0., x)`: `maxps`/`maxss` with `x` the destination and `0` the source in the Fortran
  vector body and scalar tail, `vmaxps`/`vmaxss` with the same operand roles in the C++ (a NaN
  argument gives 0 in both).
- (v) `S = D*A` (`:389`) is an element-wise product. Both objects vectorise both loops (SSE 4-wide
  in the Fortran, AVX 8-wide in the C++), which cannot change a bit: no reduction, no FMA
  (`vfmadd` absent from the C++ object), one correctly rounded division per element.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, the point function inside a `Kokkos::parallel_for`),
  never run. The SASS has `FFMA`, from CUDA's own `cosf`/`sinf` and its IEEE division sequence;
  CUDA's `cosf`/`sinf` are not glibc's `sincosf` and bit parity on the GPU is not shown.
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3src1md.F90` from `build_st1`'s
  flags (disassembly identical to the CMake object) and a C++ unit under the `openmp-release` flags;
  compared per site the GIMPLE and assembly of the 4-wide and 2-wide vector bodies, the scalar tail and
  the C++ alias-fallback scalar loop: one `sincosf` call in both, the same operands per operation, the
  constant `0x3d1248d8` (negated `0xbd1248d8` in the vector bodies) in both, `max` with the argument as
  first operand and `+0` as second at every site (NaN and `-0` give `+0`), no `vfmadd`. Fixed only the
  documented lengths of `ECOS` and `ESIN` (`NSPEC+NTH`, not `NSPEC`) in the header comment and above.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
