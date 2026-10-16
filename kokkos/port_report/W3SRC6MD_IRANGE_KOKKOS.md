# W3SRC6MD_IRANGE_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

`IRANGE(X0,X1,DX)`, a private helper of the ST6 source-term module `W3SRC6MD` that builds an
integer index vector. `W3SIN6` (`w3src6md.F90:450`) and `W3SDS6` (`:697`) call it as
`IKN = IRANGE(1,NSPEC,NTH)`: the spectral index of the first direction of each wavenumber. `IKN`
is a module variable, `ALLOCATABLE, SAVE` (`:104`), `!$OMP THREADPRIVATE` (`:107`) only under
`OMPG`: the guard `#ifdef W3_OMPG || W3_OMPH` (`:106`) tests `W3_OMPG` alone, cpp ignores the rest
with a warning. It is built by whichever of the two runs first on each thread, and the run stops with `EXTCDE(2)` when
`SIZE(IKN)` is not `NK` (`:452-455`, `:700-703`).

## What was ported

| | |
|---|---|
| WW3 routine | `IRANGE` of `W3SRC6MD`, `model/src/w3src6md.F90:1342-1371` at NOAA-EMC/WW3 `03adda91`; author S. Zieger. Compiled with `ST6`: 61 switch files (`git grep -lw ST6 -- '*switch*'`), among them `ww3_ts1/input/switch_ST6`, `ww3_ts2`, `ww3_ts3`, `ww3_tp2.15`, `mww3_test_05`, and `model/bin/switch_NRL3`, `switch_UoM_*`, `switch_swin`; no `switch_NCEP_*` or `switch_NWS_*`. No `#ifdef` inside the function |
| Kokkos code | [`../src/ww_kokkos/srce_st6_util.hpp`](../src/ww_kokkos/srce_st6_util.hpp), `ww::srce::st6::irange(x0, x1, dx, ix)`, plus `range_n` (the Fortran's `N`) and `range_size` (`SIZE` of the result, `max(N,0)`) shared with `rrange`. The Fortran result is an `ALLOCATABLE`; here the caller sizes `ix` with `range_size` and the function fills it and returns that size. Values are the Fortran's (1-based for `X0 = 1`) |
| C shim | none yet. `IKN` is a per-thread `SAVE` table: the host builds it once and passes it to the `W3SIN6`/`W3SDS6` kernels; the `SIZE(IKN) /= NK` stop becomes a host check |
| Plan | issue #42, branch `goal/42-port-st-flx-swl` |

`W3SWLDMD` has its own `IRANGE` with the same statements (`w3swldmd.F90:456-486`,
[`W3SWLDMD_IRANGE_KOKKOS.md`](W3SWLDMD_IRANGE_KOKKOS.md)); this is not that one.

## Arithmetic

Reference: the `-O3` object of `w3src6md.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd` with `ST6 FLX4`), recompiled with `-fdump-tree-optimized-lineno` and checked
identical in its disassembly to the CMake object; compared with a throwaway C++ unit compiled with
the `openmp-release` flags, warning-free in this header, 2026-10-16. The `FLX5` object
(`ST6 FLX5`) has the same disassembly of `__w3src6md_MOD_irange`.

- (v) gfortran does not inline `IRANGE`: both callers `call __w3src6md_MOD_irange` (GIMPLE at
  `:450` in `w3sin6` and `:697` in `w3sds6`), so the standalone function is the reference.
- (v) `N = INT(REAL(X1-X0)/REAL(DX))+1` (`:1365`): integer subtract, `cvtsi2ss` of the difference
  and of `DX`, `divss`, `cvttss2si`, `+1`. The C++ does the same through `ww::st4::fint`, which
  gives `INT_MIN` for NaN or out of range (`DX = 0`) as `cvttss2si` does, where a C++ cast is
  undefined.
- (v) `IX(I) = X0+(I-1)*DX` (`:1367-1369`): integer only. gfortran vectorised it by 4, the C++ by
  8 and 4; elements are independent, so the lanes cannot change a value.
- (v) `N <= 0`: the Fortran allocates a zero-size array and skips the loop; the C++ writes nothing
  and returns 0.
- (v) No libm or libmvec call, no reduction, no `MIN`/`MAX`, no floating-point constant.
- ⚠ `N` goes through `REAL`, exact only while `X1-X0` and `DX` are below 2^24; for
  `(1, NSPEC, NTH)` that holds for any realistic grid, and the `SIZE(IKN) /= NK` stop catches the
  rest. Reproduced, not fixed. Integer overflow of `X0+(I-1)*DX` is undefined in both languages
  and unreachable for these arguments.
- ⚠ In WW3: `IKN` is built once per thread for the first grid's `NSPEC`, `NTH`; under
  `ww3_multi` with ST6 grids of different `NK` the second grid stops in `EXTCDE(2)`
  (`:449-455`). Reproduced by the host check, not fixed.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, pinned `#cuda` shell): `I2FP`, the IEEE division
  (`MUFU.RCP` with its correctly rounded `FFMA` refinement) and `F2I.TRUNC` behind the `fint` range
  test. Never run.
- (v) Fresh review 2026-10-16: no arithmetic defect. The reviewer recompiled `w3src6md.F90` and `w3swldmd.F90` from the `ST6 FLX4` reference flags (disassembly identical to the CMake objects; `w3src6md` also under `ST6 FLX5`, identical) and a C++ unit including `srce_st6_util.hpp` and `srce_swl_util.hpp` together (warning-free, no clash), and compared their GIMPLE and assembly: the `N` chain (`cvtsi2ss`, `divss`, `cvttss2si`, `+1`), the `fint` range test against `cvttss2si` at the edges (`-2^31` kept, NaN and `>= 2^31` to `INT_MIN`), the integer fill, the zero-size path, and that both callers call the standalone function. Fixed in this report only: `THREADPRIVATE` holds under `OMPG` alone.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
