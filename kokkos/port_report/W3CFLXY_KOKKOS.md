# W3CFLXY_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The largest Courant-Friedrichs-Lewy (CFL) number of spatial advection over every frequency and
direction of one sea point, `max over (k, θ) of DTG·max(ċ_p, ċ_q)` with
`ċ_p = c_x ∂p/∂x + c_y ∂p/∂y` in grid index space, `c_x = (cos θ · c_g)/cos φ (+ U/cos φ)`,
`c_y = sin θ · c_g (+ V)`. A diagnostic: it fills `CFLXYMAX(JSEA)` for the CFX output field and
changes no spectrum.

## What was ported

| | |
|---|---|
| WW3 routine | `W3CFLXY`, `model/src/w3pro3md.F90:1980-2136` at NOAA-EMC/WW3 `03adda91` (157 lines). Called only by `W3WAVE` (`w3wavemd.F90:1722`) when `FLOGRD(9,3)` is set on a grid that is not unstructured. Compiled by every `PR3` switch: 408 switch files (`git grep -lw PR3 -- '*switch*'`), among them `ww3_tp1.*`, `ww3_tp2.*`, `ww3_ts2`-`ww3_ts4`, `ww3_tic*`, `ww3_tbt*`, `ww3_ufs1.*`, `mww3_test_01`-`08`, and the operational `switch_NCEP_glwu`, `switch_NCEP_gwm`, `switch_NCEP_st2`, `switch_NCEP_st4`, `switch_NCEP_st4sbs`, `switch_NWS_rwps` |
| Kokkos code | [`../src/ww_kokkos/prop_cflxy.hpp`](../src/ww_kokkos/prop_cflxy.hpp), `ww::prop::w3cflxy<Mgp>`; reads `Params`, `Grid`, `Step` of `prop_params.hpp` |
| Variants | `W3_MGP` (moving grid, `:2113-2116`) is the template flag `Mgp`. No switch in WW3 combines `MGP` with `PR3` (`git grep -lw MGP -- '*switch*'` lists only `mww3_test_05` `PR1`/`PR2` switches), so `Mgp = true` is never compiled by WW3. `W3_S`, `W3_T` are output only. `MAPSTA`, `MAPFS` and `IXY` (`:2108`) are never read |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3pro3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd`), identical to the CMake object of the reference build, and the same file built
with `-DW3_MGP` added; compared with a throwaway C++ unit under the `openmp-release` flags plus
`-ffp-contract=off`, 2026-10-16.

- (v) gfortran vectorises the `ITH` loop (16-byte vectors, unroll factor 2) when
  `2 <= NTH < HUGE`, in both variants: four running maxima per `IK`, lanes
  `{VLCFLX(ITH), VLCFLY(ITH), VLCFLX(ITH+1), VLCFLY(ITH+1)}`, each started from `CFLXYMAX` of
  the previous `IK`, `maxps %v,%acc` (`acc > v ? acc : v`), reduced by `movhlps`/`shufps $0x55`
  as `max(max(l3,l1), max(l2,l0))` with the upper lane first, then the odd `ITH = NTH` in a scalar
  epilogue. With `NTH = 1` a scalar loop. The C++ writes the four lanes and the reduction
  explicitly; its `-O3` GIMPLE keeps every max as `a > b ? a : b` with the operands of the object,
  so the result of a NaN or of `+0`/`-0` ties is the Fortran's.
- (v) `MAX(VLCFLX,VLCFLY,CFLXYMAX)` (`:2128`) operand order differs by path, read from the
  assembly: the epilogue takes `VLCFLY` as destination without currents and `VLCFLX` with
  currents; the scalar loop likewise, except under `MGP`, where it takes `VLCFLX` in both. The
  running maximum is always the destination of the second `maxss`.
- (v) `CXTOT = (ECOS·CG)/CLATS`, then `- VGX/CLATS` and `+ CX/CLATS` as separate quotients (the
  object hoists them out of the loop; same bits), `CP = CXTOT·DPDX + CYTOT·DPDY`, `CP·DTG`, in the
  Fortran's order in every path; no FMA, no reciprocal, no libm or libmvec call in either object.
- ⚠ The C++ does not steer the operand order of the commutative `+` and `·` (g++ writes
  `CY + CYTOT` where the object adds `CYTOT + CY`, and the object's scalar loop adds
  `CYTOT·DPDY + CXTOT·DPDX`): a difference only in the payload of a NaN result when both operands
  are NaN.
- ⚠ CUDA not compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
