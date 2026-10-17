# W3SRC3MD_CALC_USTAR_KOKKOS

**Status:** ported, phase 1 (translated). Wired: WW3 at the `WW3/` pin (`357f575c`) runs it inside the Kokkos `W3SPR3`, behind `WW_KOKKOS_SPR3` (default off); not regtested: compiled, never run inside WW3 (the owner, 2026-10-16).

Friction velocity `u*`, air-side roughness length `z0` and Charnock coefficient of the ST3 (WAM4+)
package, by bilinear interpolation in the table `TAUT(TAUW, U10) = sqrt(tau)` that `TABU_STRESS`
builds. `W3SPR3` calls it once per sea point (`w3src3md.F90:325`). Not the ST4 routine of the same
name ([`CALC_USTAR_KOKKOS.md`](CALC_USTAR_KOKKOS.md)): ST3's has no Newton branch, no `CAPCHNK`
and no `ZZ0MAX`.

## What was ported

| | |
|---|---|
| WW3 routine | `CALC_USTAR` of `W3SRC3MD`, `model/src/w3src3md.F90:1135-1227` at NOAA-EMC/WW3 `03adda91` (module constants `ITAUMAX`, `JUMAX`, `TAUWMAX` at `:95-101`); compiled by every `ST3` switch: 47 regtest switch files (`mww3_test_05` 20, `ww3_ts3` 20, `ww3_ts2` 5, `ww3_ts1`, `ww3_tnc1`; `git grep -lw ST3 -- '*switch*'`), by no `switch_NCEP_*`/`switch_NWS_*` in `model/bin`. The routine has no `#ifdef` |
| Kokkos code | [`../src/ww_kokkos/srce_st3_calc_ustar.hpp`](../src/ww_kokkos/srce_st3_calc_ustar.hpp), `ww::srce::st3::calc_ustar(windspeed, tauw, ustar, z0, charn, taut, deltauw, delu, zzwnd, aalpha)`: the Fortran dummies, then `TAUT` as a flat column-major `const Real*` (`TAUT(i,j)` at `i + 201*j`), `DELTAUW` and `DELU` of `TABU_STRESS` (`:906-907`), and `ZZWND`, `AALPHA` of `W3GDATMD` |
| C shim | none yet; `W3SPR3` (a later unit) calls it from C++ |
| Plan | issue #42, branch `goal/42-port-st-flx-swl` |

## Arithmetic

Reference: the `-O3` objects of `w3src3md.F90` under the reference builds `build_st3` (ST3 STAB0
FLX0) and `build_st3s` (ST3 STAB3 FLX0), WW3's CMake Release flags, gfortran 16.2, recompiled in
the pinned shell and checked identical to the CMake objects; C++ from a throwaway unit with the
`openmp-release` flags (g++ 16.2, `-ffp-contract=off`), 2026-10-16.

- (v) The optimised GIMPLE of `calc_ustar` is the same under STAB0 and STAB3 except for SSA
  names: STAB3 changes `W3SIN3` only (its `#ifdef W3_STAB3` blocks at `:513-516`, `:557-560`, `:563-567`, `:642-649`, all inside `W3SIN3` at `:384-717`), so there is one variant.
- (v) The C++ GIMPLE and object follow the Fortran's operation by operation: `MAX(MIN(TAUW,
  TAUWMAX),0.)`, `sqrtf`, the divisions by `DELTAUW` and `DELU`, `INT` as `cvttss2si` then
  `MIN(_,199)` by `cmovg`, the four products and three sums of the interpolation in the Fortran's
  grouping (`((T00*DELI2+T10*DELI1)*DELJ2) + ((T01*DELI2+T11*DELI1)*DELJ1)`; operands of a
  product or sum swapped only where IEEE gives the same bits), `WINDSPEED/USTAR`,
  `-(SQRTCDM1*0.4)` (`mulss` then sign flip in both), `ZZWND*expf(_)`, `(Z0*GRAV)/(USTAR*USTAR)`.
- (v) MIN/MAX: every `minss`/`maxss` has the variable as destination and the constant as source in
  both objects, so a NaN gives the constant. In particular `WINDSPEED = 0` reads `TAUT(:,0) = 0`,
  `USTAR = 0`, `0/0 = NaN`, and `MIN(NaN,100.) = 100.` (`:1218`); `CHARN = AALPHA` then.
- (v) One libm call, `expf` (glibc, not a `_ZGV*` variant), in both objects; no reduction, no
  vectorised code, no FMA on the host.
- (v) The table offsets are formed in 64 bits as gfortran does (`(int64)J*201 + IND`), so the
  out-of-range path below has no signed overflow in C++.
- ⚠ WW3 does not clamp `J` from below (`:1210`): a `WINDSPEED <= -DELU`, or NaN (`INT` gives
  `-2147483648`), reads `TAUT` out of bounds in the Fortran and in the port alike. Wind speeds
  reaching `W3SPR3` are non-negative in practice; not checked.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, Kokkos 5.2.2 CUDA and nvcc 12.9 of the `cuda-release`
  build), never run. The SASS has 16 `FMUL` and 12 `FADD`, enough for the routine's own 10
  products and 7 sums unfused, and 41 `FFMA`, taken to be CUDA's `expf`, division and `sqrtf`
  sequences (not traced instruction by instruction); the device bits need not match glibc's.
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3src3md.F90` under `build_st3` and
  `build_st3s` flags (identical to the CMake objects) and the header in a throwaway unit with the
  `openmp-release` flags (warning-free); compared GIMPLE and assembly operation by operation: the
  `.rodata` constants (`2.2361`, `1`, `100`, `0.4`, `0.001`, `9.806`), the `minss`/`maxss` operand
  order at all five sites (variable first, constant on NaN, `+0` for `TAUW = -0`), `sqrtss` without
  an errno call, `INT` out of range or NaN giving `-2147483648` and the same 64-bit offsets
  (`-2147483648*201` folded in the C++ branch), the interpolation grouping, `expf`, and the
  `USTAR > 0.001` branch taking `AALPHA` on NaN in both. `CALC_USTAR` is called, not inlined, by
  `W3SPR3` in the object. Fixed only the STAB3 line citation of the first bullet.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
