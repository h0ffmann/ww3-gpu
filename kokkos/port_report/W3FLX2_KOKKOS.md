# W3FLX2_KOKKOS

**Status:** ported, phase 1 (translated). Wired: WW3 at the `WW3/` pin (`357f575c`) calls it at `w3srcemd.F90:1319, :2287` behind `WW_KOKKOS_FLX2` (default off); not regtested: compiled, never run inside WW3 (the owner, 2026-10-16).

Friction velocity `UST`, roughness length `Z0` and drag coefficient `CD` after Tolman and
Chalikov (1996) (switch `FLX2`), iterated `NITTIN` times on the peak phase velocity `CP`, which
comes from an inlined, reduced `WAVNU1` on the `DISTAB` table. `W3SRCE` calls it per sea point
(`w3srcemd.F90:1142`, `:1891`); the point post-processors call it too (`ww3_outp.F90:1988`,
`ww3_ounp.F90:2146`, `gx_outp.F90:1094` and their siblings).

## What was ported

| | |
|---|---|
| WW3 routine | `W3FLX2`, `model/src/w3flx2md.F90:90-230` at NOAA-EMC/WW3 `03adda91` (141 lines); compiled by 380 regtest switch files in 36 regtests (`ww3_tp1.*`, `ww3_tp2.*`, `ww3_ts1`, `ww3_ts2`, `ww3_ta1`, `ww3_tic*`, `mww3_test_01`..`06`, `09`; `git grep -lw FLX2 -- 'regtests/*switch*'`, `unittests/data/switch.io` excluded), by no `switch_NCEP_*` or `switch_NWS_*`. Its only `#ifdef` blocks are `W3_S` (`STRACE`), output only, not ported |
| Kokkos code | [`../src/ww_kokkos/srce_flx2.hpp`](../src/ww_kokkos/srce_flx2.hpp), `ww::srce::flx::w3flx2_point` |
| Module state | `NITTIN`, `CINXSI` of the current grid (W3GDATMD pointers into `MPARS(IMOD)%SFLPS`, `w3gdatmd.F90:2600-2601`) by value; `DSIE`, `N1MAX` by value and `EWN1(0:NAR1D)` as a pointer, element `i` = `EWN1(i)`, the layout `ww::disp::distab()` fills (`disp_distab.hpp`) |
| C shim | none yet |
| Plan | issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3flx2md.F90` under `switch_st0` (`ST0 FLX2 LN0`, from
`switch_lab_shrd`), WW3's CMake Release flags, gfortran 16.2.0, recompiled in scratch and
identical to the CMake object; compared operation by operation with the `-O3` object of a
throwaway C++ unit built with the `openmp-release` flags (g++ 16.2.0, warning-free), 2026-10-16.

- (v) `MAX`: four `maxss` sites, destination per site as in the object: `MAX(FP,0.001)` has
  `0.001` as destination (a NaN `FP` passes through), `MAX(0.01,U)`, `MAX(UST,0.0001)` and
  `MAX(0.,LOG(..))` have the variable as destination (a NaN gives the constant); the C++
  `vmaxss` operands match at each site.
- (v) `INT(SIX/DSIE)` is `cvttss2si` on the one quotient that `R1` reuses (`:197`, `:199`);
  `fint` reproduces it, including `INT_MIN` for NaN or out of range.
- (v) `**1.5` and `**1.23` are calls of glibc's scalar `powf` in both objects (not `sqrt`, not
  `__powisf2`); `LOG` and `EXP` are scalar `logf` and `expf`; no libmvec, nothing vectorised
  (`-fopt-info-vec` empty for the Fortran; for the C++ it lists only two Kokkos `stl_tree.h` blocks, nothing of the header).
- (v) `UNZ**2` is `unz*unz`; `-0.4/SQRT(CD)` is `0.4/sqrt` then a sign flip; one `sqrtss` of `CD`
  serves `UST` and `Z0`; the remaining products and sums are in the object's order or commuted
  (same bits). `TPI` folded to `0x40c90fdb` (`static_assert` in the header), `GRAV` `0x411ce560`;
  every literal of the C++ `.rodata` matches the Fortran's.
- (v) gfortran computes `Z0` once after the loop from the last `SQRT(CD)`; the C++ computes it in
  every iteration: the last value has the same bits. With `NITTIN <= 0` both write only `USTD`.
- (v) `SQRT` is `sqrtss`; g++ adds a call of `sqrtf` for a negative argument (errno), and glibc's
  `sqrtf` on x86-64 is `sqrtss`, so the NaN is the same.
- ⚠ The Fortran checks only `I1 <= N1MAX` (`:198`): with `DEPTH < 0`, NaN, or `SIX/DSIE >= 2**31`,
  `I1 = INT_MIN` and `EWN1(I1)` is read outside its bounds. The port takes the same path and the
  same address; the value read is undefined in both. With `DEPTH > 0` and `FP` finite it cannot
  happen.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3flx2md.F90` under `switch_st0` (identical
  to the CMake object) and the C++ unit with the `openmp-release` flags (warning-free), compared the
  GIMPLE and the assembly site by site: the four `maxss` operand orders, `cvttss2si` and the
  `INT_MIN` path (same `EWN1` address, `(I1+1)*4` sign-extended), every `.rodata` literal, the
  `powf`/`logf`/`expf` calls, the products and divisions in order, and the stores with `NITTIN <= 0`.
  The only difference left is register allocation: gfortran emits `:200`'s sum as
  `R1*EWN1(I1+1) + (1-R1)*EWN1(I1)`, g++ in source order; the GIMPLE of both is `_10 + _14`, and the
  NaN chosen differs only when both products are NaN with different payloads, which a finite
  `DISTAB` table and the `INT_MIN` path (both products carry `R1`'s NaN) cannot produce. EWN1, DSIE
  and N1MAX checked against `disp_distab.hpp` (`ewn1[i]` = `EWN1(i)`, 122 entries, `N1MAX` = 120,
  so `EWN1(I1+1)` stays in bounds for `0 <= I1 <= N1MAX`). Fixed in this report: the `W3GDATMD`
  line cite and the `-fopt-info-vec` wording; CUDA compiled again (compile only).
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, the `#cuda` shell), never run. The `FFMA` in the SASS
  come from CUDA's `powf`, `logf`, `expf` and division sequences, not glibc's algorithms.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
