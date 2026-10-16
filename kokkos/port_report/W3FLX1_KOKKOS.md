# W3FLX1_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Friction velocity, roughness length and drag coefficient from the 10 m wind after Wu (1980)
(switch `FLX1`): `CD = 0.001*(0.8+0.065*U10)`, `Z0 = ZWND*EXP(-0.4/SQRT(CD))`,
`UST = U10*SQRT(CD)`, `USTD = U10D`. `W3SRCE` calls it once per sea point
(`w3srcemd.F90:1139`); the point post-processors call it too (`ww3_outp.F90:1984`, `:2162`,
`ww3_ounp.F90:2142`, `:2325`, `gx_outp.F90:1090`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3FLX1`, `model/src/w3flx1md.F90:88-200` at NOAA-EMC/WW3 `03adda91` (113 lines); compiled by 59 regtest switches (`mww3_test_05` 26, `ww3_ts3` 20, `ww3_ts2` 5, `ww3_tr1` 3, `ww3_ts1` 2, `mww3_test_07` 2, `ww3_tc1` 1; `git grep -lw FLX1 -- '*switch*'`) and by one operational switch, `model/bin/switch_USACE_1`, no NCEP or NWS one. Its only `#ifdef` blocks are `W3_S` tracing |
| Kokkos code | [`../src/ww_kokkos/srce_flx1.hpp`](../src/ww_kokkos/srce_flx1.hpp), `ww::srce::flx::w3flx1_point`; returns 1 where the Fortran stops in `EXTCDE(1)` (`ABS(ZWND-10.) > 0.01`, `:178-181`), with no output written |
| C shim | none yet; on 1 it writes format 1000 (`:194-196`) on `NDSE` when `IAPROC == NAPERR` and stops the run |
| Plan | issue #42, branch `goal/42-port-st-flx-swl` |

## Arithmetic

Reference: the `-O3` object of `w3flx1md.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd` with `ST1`/`FLX1`), recompiled and checked identical to the CMake object
(`objdump -d`), against a throwaway C++ unit with the `openmp-release` flags and
`-ffp-contract=off`, 2026-10-16.

- (v) Same operations in the same order in both objects: `ZWND-10`, `andps` (ABS), `comiss`
  against `0.01f`; `U10*0.065f`, `+0.8f`, `*0.001f`; one `sqrtss` of `CD` reused for `Z0` and
  `UST`; `0.4f/sqrt`, sign flip by `xorps`, `expf`, `*ZWND`; `U10*sqrt`. The six `.rodata.cst4`
  constants are byte-identical (`0x41200000`, `0x3c23d70a`, `0x3d851eb8`, `0x3f4ccccd`,
  `0x3a83126f`, `0x3ecccccd`); gfortran folded nothing beyond the literals.
- (v) One libm call, scalar `expf` (no libmvec variant) in both objects. No FMA, no reduction,
  no MIN/MAX, no integer conversion.
- (v) The C++ object adds the `-fmath-errno` slow path, a call to `sqrtf` when `CD < 0`
  (`U10 < -12.3`, not a wind speed); glibc's `sqrtf` returns the same NaN bits as `sqrtss`.
- (v) No arithmetic variant: `W3_S` only adds `STRACE`.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, the `#cuda` shell), never run. The SASS keeps the
  user arithmetic unfused (`FADD`, `FMUL`, `FADD`, `FMUL`); `sqrtf` and the division are CUDA's
  correctly rounded sequences, but `expf` is CUDA's own algorithm (`MUFU.EX2` with `FFMA`
  reduction), not glibc's, so `Z0` on a device is not shown to match.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3flx1md.F90` from `build_st1`'s
  `flags.make` (identical to the CMake object), preprocessed with and without `W3_S` (only
  `STRACE` differs), and compared it with a C++ unit of the header (`openmp-release` flags) by
  instruction: same operations, operand order of each `mulss`/`divss`, `andps` mask, `comiss`
  branch (a NaN `ZWND` passes the test in both), the same six `.rodata.cst4` words, scalar `expf`.
  Nothing changed. ⚠ One NaN-only difference, not fixable from the source (g++ emits the same
  code for `EXP(..)*ZWND`): for `Z0` with both `ZWND` and the `EXP` result NaN, the Fortran's
  `mulss` returns the `EXP` NaN and the C++ `vmulss` the `ZWND` NaN; a NaN `ZWND` is not a valid
  input (`ZWND` is the grid's wind height, 10 m under `FLX1`).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
