# W3SWLDMD_IRANGE_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

`IRANGE(X0,X1,DX)`, the private helper of the swell dissipation module `W3SWLDMD` that builds an
integer index vector. `W3SWL6` (`w3swldmd.F90:371`) and `W3SWL4` (`:192`) call it once each, as
`IKN = IRANGE(1,NSPEC,NTH)` into an `ALLOCATABLE, SAVE` array, the spectral index of the first
direction of each wavenumber, and stop with `EXTCDE(2)` when `SIZE(IKN)` is not `NK` (`:194-197`,
`:374-377`). `W3SRCE` calls only `W3SWL6` (`w3srcemd.F90:1332`, under `SWL6S6`); nothing in
`model/src` calls `W3SWL4`.

## What was ported

| | |
|---|---|
| WW3 routine | `IRANGE` of `W3SWLDMD`, `model/src/w3swldmd.F90:456-486` at NOAA-EMC/WW3 `03adda91`. The module is compiled with `ST6` (`model/src/cmake/switches.json:258`): 61 switch files (`git grep -lw ST6 -- '*switch*'`), among them `ww3_ts1/input/switch_ST6`, `ww3_ts2`, `ww3_ts3`, `ww3_tp2.15` and `mww3_test_05`, and `model/bin/switch_NRL3`, `switch_UoM_*`, `switch_swin`; no `switch_NCEP_*` or `switch_NWS_*`. No `#ifdef` inside the function |
| Kokkos code | [`../src/ww_kokkos/srce_swl_util.hpp`](../src/ww_kokkos/srce_swl_util.hpp), `ww::srce::swl::irange(x0, x1, dx, ix)`. The Fortran result is an `ALLOCATABLE`; here the caller provides `ix` and the function returns `SIZE(IX)`, `max(N,0)`. With `ix == nullptr` it only counts, so the host sizes the table and then fills it. The values are the Fortran's (1-based for `X0 = 1`) |
| C shim | none yet. `IKN` is a `SAVE` table: the host builds it once and passes it to the `W3SWL6`/`W3SWL4` kernels, which check the count against `NK` where the Fortran calls `EXTCDE(2)` |
| Plan | issue #42, branch `goal/42-port-st-flx-swl` |

⚠ In WW3 itself: `IKN` is `ALLOCATABLE, SAVE` and allocated by the first call inside the
`OMPG` parallel region over points, a data race (`w3swldmd.F90:359`, `:370-372`; `AGENTS.md`,
"Bit for bit"). It aborts a run, it does not change bits; a host-built table removes it on the
port side without changing a value.

`W3SRC6MD` has its own private `IRANGE` (`w3src6md.F90:1342-1371`) whose executable statements are
the same text; it has its own port report and is not this one.

## Arithmetic

Reference: the `-O3` object of `w3swldmd.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd` with `ST6 FLX4`), recompiled with `-fdump-tree-optimized-lineno` and checked
byte-identical in its disassembly to the CMake object; compared with a throwaway C++ unit compiled
with the `openmp-release` flags, 2026-10-16.

- (v) `N = INT(REAL(X1-X0)/REAL(DX))+1` (`:480`): the object does an integer subtract,
  `cvtsi2ss` of the difference and of `DX`, `divss`, `cvttss2si`, `+1`. The C++ does the same
  `vcvtsi2ss`, `vcvtsi2ss`, `vdivss`, and `ww::st4::fint`, which guards the `vcvttss2si` with the
  range test so that out-of-range or NaN (`DX = 0`) gives `INT_MIN` as `cvttss2si` does, where a
  C++ cast would be undefined.
- (v) `IX(I) = X0+(I-1)*DX` (`:482-484`): integer only, no rounding. gfortran vectorised it by 4
  (SSE2), the C++ by 8 and 4 (AVX2); each element is the same integer expression, so the order of
  the lanes cannot change a value.
- (v) `N <= 0` (including `DX = 0`): the Fortran allocates a zero-size `IX` (`size.0 = 0` in the
  GIMPLE) and the loop does not run; the C++ writes nothing and returns 0.
- (v) No libm or libmvec call, no reduction, no `MIN`/`MAX`, no floating-point constant.
- ⚠ `N` goes through `REAL`, so it is exact only while `X1-X0` and `DX` are below 2^24 and the
  quotient does not round up to the next integer; for `(1, NSPEC, NTH)` that holds for every
  realistic grid, and the `SIZE(IKN) /= NK` stop catches the rest. Reproduced, not fixed.
- ⚠ Integer overflow of `X0+(I-1)*DX` is undefined in both languages; unreachable for
  `(1, NSPEC, NTH)`.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, pinned `#cuda` shell): the PTX has `cvt.rn.f32.s32`,
  `div.rn.f32` (IEEE division; the `FFMA` in the SASS are its correctly rounded expansion) and
  `cvt.rzi.s32.f32` behind the `fint` range test, no `fma`. Never run.
- (v) Fresh review 2026-10-16: no defect. The reviewer recompiled `w3src6md.F90` and `w3swldmd.F90` from the `ST6 FLX4` reference flags (disassembly identical to the CMake objects; `w3src6md` also under `ST6 FLX5`, identical) and a C++ unit including `srce_st6_util.hpp` and `srce_swl_util.hpp` together (warning-free, no clash), and compared their GIMPLE and assembly: the same `N` chain and `fint` edges as the `W3SRC6MD` `IRANGE`, the integer fill, the zero-size path and the `ix == nullptr` count; the two headers coexist in one translation unit.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
