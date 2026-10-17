# W3FLX3_KOKKOS

**Status:** ported, phase 1 (translated). Wired: WW3 at the `WW3/` pin (`357f575c`) calls it at `w3srcemd.F90:1333, :2301` behind `WW_KOKKOS_FLX3` (default off); not regtested: compiled, never run inside WW3 (the owner, 2026-10-16).

Friction velocity `UST`, roughness length `Z0` and drag coefficient `CD` after Tolman and
Chalikov (1996), iterated `NITTIN` times from the peak phase speed, with a cap on `CD` (switch
`FLX3`, used with `ST2`). `W3SRCE` calls it per sea point (`w3srcemd.F90:1146`, `:1895`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3FLX3`, `model/src/w3flx3md.F90:95-248` at NOAA-EMC/WW3 `03adda91`; compiled by the NOAA operational `model/bin/switch_NCEP_st2` and by the 20 `ww3_ts3/input/switch_ST2_*` regtest switches and that test's default `switch` (`git grep -lw FLX3 -- '*switch*'`). Its only `#ifdef` blocks are `W3_S` (tracing) |
| Kokkos code | [`../src/ww_kokkos/srce_flx3.hpp`](../src/ww_kokkos/srce_flx3.hpp), `ww::srce::flx::w3flx3_point`; both caps of `CAP_ID` (`MIN`, `:233`, and `TANH`, `:235`) as a run-time argument, as in the Fortran (`w3gridmd.F90:1652-1660` leaves it 0 or 1) |
| Module state | `NITTIN`, `CINXSI`, `CD_MAX`, `CAP_ID` of `W3GDATMD` by value; `DSIE`, `N1MAX` by value and `EWN1(0:NAR1D)` as a pointer to element 0, the layout [`disp_distab.hpp`](../src/ww_kokkos/disp_distab.hpp) builds |
| C shim | none yet |
| Plan | issue #42, batch of `ST`/`FLX`/`SWL` units |

## Arithmetic

Reference: the `-O3` object of `w3flx3md.F90` recompiled with the flags of the WW3 CMake Release
build under `switch_NCEP_st2`'s physics (`ST2 STAB2 FLX3`, gfortran 16.2), byte-identical in
disassembly to the CMake object; compared with the C++ unit at `openmp-release` flags
(`-O3 -march=x86-64-v3 -ffp-contract=off`), operation by operation, 2026-10-16.

- (v) Every statement maps one to one: `TPI` folded to `0x40c90fdb` (`static_assert` against
  `2*kPi`), `INT(SIX/DSIE)` is `cvttss2si` (`fint`), the interpolation `(1-R1)*EWN1(I1) +
  R1*EWN1(I1+1)` then `/DEPTH`, `UNZ**2` as `UNZ*UNZ`, `CINXSI*SQRT(ALPHA)*UNZ**2` in that order;
  products and sums differ only by commutation, which is exact.
- (v) libm: the Fortran object calls `powf` twice (`**1.5`, `**1.23`; neither became `sqrt`),
  `logf`, `tanhf` (`CAP_ID /= 0`) and `expf`; the C++ object calls the same scalar functions. No
  libmvec, nothing vectorised in either object. `SQRT` is `sqrtss` in both.
- (v) MIN/MAX, destination first: `maxss(0.001, FP)` `:200`, `maxss(U, 0.01)` `:218`,
  `maxss(UST, 1E-4)` `:222`, `maxss(LOG(..), 0)` `:223`, `minss(CD_MAX, CD)` `:233`; the C++ object
  emits `vmaxss`/`vminss` with the same operand order at each site.
- (v) `-0.4/SQRT(CD)` parses as `-(0.4/SQRT(CD))`: the Fortran object divides `0.4`
  (`0x3ecccccd`) and flips the sign (`xorps`); the C++ writes `-(0.4/x)` and its object does the
  same (`vdivss`, then `vxorps` with the sign mask). Dividing `-0.4` instead gives the same bits
  except the sign of a NaN `Z0` (`CD` NaN or negative, reachable with `NITTIN <= 0` or a NaN
  `CDMAX`).
- (v) No FMA and no reciprocal multiply in either object.
- ⚠ The Fortran object drops the dead `Z0` (and the `UST` store) of the loop body (`:226-227`);
  the C++ computes and stores them each iteration and overwrites them at `:238-239`. Outputs are
  the same bits; only the work differs.
- ⚠ g++ (`-fmath-errno`) calls `sqrtf` on a negative argument instead of `sqrtss`; glibc's
  `sqrtf` returns `sqrtss`'s NaN there, read from glibc's wrapper, not run.
- ⚠ Undefined in WW3: with `NITTIN <= 0` the Fortran reads `CD`, `INTENT(OUT)`, at `:233`/`:235`;
  the C++ reads the incoming `cd` the same way. With `SIX` NaN (`FP` NaN or `DEPTH < 0`) or `SIX/DSIE >= 2**31`,
  `cvttss2si` gives `I1 = INT_MIN`, which passes `I1 <= N1MAX` (`:205`) and reads `EWN1(INT_MIN)` out of bounds; the C++
  reproduces the read. With `DEPTH = 0`, `WNP` is `0/0`. `W3SRCE` is not expected to pass these.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, Kokkos 5.2.0 CUDA), never run. The SASS has `FFMA`
  instructions; they are taken to come from CUDA's `powf`/`logf`/`expf`/`tanhf`, not attributed
  one by one. Those functions are not glibc's (`AGENTS.md`, "Bit for bit with the Fortran").
- (v) Fresh review 2026-10-16: one defect found and fixed. The header divided `-0.4` by
  `SQRT(CD)` at `:227` and `:239` where the object computes `-(0.4/SQRT(CD))`, so a NaN `Z0`
  carried the opposite sign bit; it now writes `-(0.4/x)`, and the recompiled C++ object divides
  `0.4` and flips the sign with `vxorps` at every site. Compared again, operation by operation:
  the `-O3` GIMPLE and disassembly of `w3flx3md.F90` (recompiled under `build_st2`'s flags,
  disassembly identical to the CMake object) with the C++ unit at `openmp-release` flags (no
  warning): every `maxss`/`minss` operand order (`:200`, `:218`, `:222`, `:223`, `:233`), the
  `.rodata` constants, `powf`/`logf`/`expf`/`tanhf` calls, `cvttss2si` and the `EWN1`
  interpolation; both `CAP_ID` branches; `DSIE`, `N1MAX` and `EWN1(0:NAR1D)` as
  `disp_distab.hpp` returns and lays them out (pointer to element 0, no shift). `CDMAX` is
  clamped to `MAX(0., CDMAX)` before it becomes `CD_MAX` (`w3gridmd.F90:1651`, `:1660`). ⚠ The
  CUDA compile was not repeated after the fix: `nvcc_wrapper` fails on host GCC 16.2 here.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. The regtests that would exercise it are the `ww3_ts3` `ST2_*`
entries of `matrix.base`, the only ones whose switch has `FLX3`: serial `:971-975`, MPI
`:1065-1109`, hybrid `:1361-1405` and OpenMP `:1651-1695` (the `ww3_ts3` lines only; the `ST2`
switches of `ww3_ts1`, `ww3_ts2` and `mww3_test_05` do not compile `FLX3`).
