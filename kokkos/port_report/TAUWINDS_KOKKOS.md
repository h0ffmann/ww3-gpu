# TAUWINDS_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

`TAUWINDS(SDENSIG,CINV,DSII)`, a private function of the ST6 source-term module `W3SRC6MD`: the
wave-supported (form) stress carried by the wind input,

  tau = g rho_w sum_k S_in(sigma_k) / c(sigma_k) dsigma_k,

with `SDENSIG` the direction-integrated input in m2/(rad Hz) (or its x or y component), `CINV`
`1/c` and `DSII` the band widths (`w3src6md.F90:1450-1466`). Called by `LFACTOR` (`:1034-1036`,
`:1067-1069`, the latter inside the iteration on the reduction factor, with the argument
`SDENS10Hz*LF10Hz`) and `TAU_WAVE_ATMOS` (`:1314-1315`); the call at `:1115` is `W3_T6` test
output.

## What was ported

| | |
|---|---|
| WW3 routine | `TAUWINDS` of `W3SRC6MD`, `model/src/w3src6md.F90:1440-1469` at NOAA-EMC/WW3 `03adda91`; author S. Zieger. Compiled with `ST6`: 61 switch files (`git grep -lw ST6 -- '*switch*'`; `ww3_ts1`, `ww3_ts2`, `ww3_ts3`, `ww3_tp2.15`, `mww3_test_05`, `model/bin/switch_NRL3`, `switch_UoM_*`, `switch_swin`); no NCEP or NWS operational switch. No `#ifdef` inside the function |
| Kokkos code | [`../src/ww_kokkos/srce_st6_util.hpp`](../src/ww_kokkos/srce_st6_util.hpp), `ww::srce::st6::tauwinds(sdensig, cinv, dsii, n)`, `KOKKOS_INLINE_FUNCTION`; the assumed-shape arrays become 0-based pointers and `n = SIZE(SDENSIG)`. A caller passing an expression (`SDENS10Hz*LF10Hz`) passes it evaluated element by element into a work array, as the Fortran's temporary holds it |
| C shim | none; it is called from the `LFACTOR` and `TAU_WAVE_ATMOS` ports |
| Plan | issue #42, branch `goal/42-port-st-flx-swl` |

## Arithmetic

Reference: the `-O3` object of `w3src6md.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd` with `ST6 FLX4`), checked identical in its disassembly to the CMake object;
compared with a throwaway C++ unit compiled with the `openmp-release` flags, 2026-10-16. The
`FLX5` object has the same disassembly of `__w3src6md_MOD_tauwinds`.

- (v) `SUM(SDENSIG*CINV*DSII)` (`:1467`) is a sequential sum from `0.0` in index order: gfortran
  vectorised the loop by 4 (and an epilogue by 2) but adds the lanes into the scalar sum one at a
  time in order (`BIT_FIELD_REF` lane 0, 1, 2, 3 in the GIMPLE, a chain of `addss` in the
  assembly; no `REDUC_PLUS`), so it is the scalar order. Each term is `(SDENSIG*CINV)*DSII`. The
  C++ `sum = sum + (s*c)*d` compiles to the same in-order chain (`vmulps` then `vaddss` per lane,
  AVX2 by 8 and 4).
- (v) `GRAV * DWAT` (`constants.F90:62-63`) is folded by gfortran to `9.806e+3`
  (`0x46193800` = 9806.0f); the C++ holds it as `0x1.327p+13f` with a `static_assert` that it is
  `9.806f*1000.f` in `float`, and `.rodata.cst4` of the C++ object holds the same `0x46193800`.
  One `mulss` of the sum by it.
- (v) Empty argument (`SIZE <= 0`): the object returns `0.0`; the C++ returns `0.0f*9806 = +0`.
- (v) No FMA, no libm or libmvec call, no `MIN`/`MAX`.
- (v) How gfortran inlined it, which the caller ports must reproduce: every call in
  `tau_wave_atmos` and `lfactor` is inlined (8 copies in `tau_wave_atmos`, 11 in `lfactor`, after
  loop versioning), each the same in-order sum from `0.0`, each term `(c*s)*d` (commutative, same
  bits), then `* 9.806e+3`. With `SDENS10Hz*LF10Hz` the product is formed per element before the
  multiplication by `CINV10Hz`, so `((s*lf)*c)*d`. `W3SIN6` and `W3SDS6` do not call it.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, pinned `#cuda` shell): the sum is `FMUL`, `FMUL`,
  `FADD` per term in order, no `FFMA` in it. Never run.
- (v) Fresh review 2026-10-16: no defect. The reviewer recompiled `w3src6md.F90` and `w3swldmd.F90` from the `ST6 FLX4` reference flags (disassembly identical to the CMake objects; `w3src6md` also under `ST6 FLX5`, identical) and a C++ unit including `srce_st6_util.hpp` and `srce_swl_util.hpp` together (warning-free, no clash), and compared their GIMPLE and assembly: `(s*c)*d` per term, the lane-by-lane `addss` chain from `0.0` in index order in the standalone function and in the 8 inlined copies of `tau_wave_atmos` and 11 of `lfactor`, the folded `0x46193800`, `+0` for an empty argument. ⚠ Only when both addends of one `addss` are NaN does the payload depend on operand order, which the Fortran itself varies between inlined copies; no other bit can differ.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
