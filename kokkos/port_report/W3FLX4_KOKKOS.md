# W3FLX4_KOKKOS

**Status:** ported, phase 1 (translated). Wired: WW3 at the `WW3/` pin (`357f575c`) calls it at `w3srcemd.F90:1347` behind `WW_KOKKOS_FLX4` (default off); not regtested: compiled, never run inside WW3 (the owner, 2026-10-16).

Friction velocity `UST`, drag coefficient `CD` and roughness length `Z0` from the 10 m wind
speed after Hwang (2011, J. Atmos. Oceanic Technol. 28(3) 436-443),
`CD = FLX4A0 (8.058 + 0.967 U10 - 0.016 U10^2) 1e-4`, with `UST` held at `2.026 sqrt(FLX4A0)` for
`U10 >= 50.33` m/s and `Z0 = ZWND exp(-0.4/sqrt(CD))` (`w3flx4md.F90:210-218`). Switch `FLX4`;
`W3SRCE` calls it once per sea point (`w3srcemd.F90:1149-1150`), and the point post-processors
`ww3_outp`, `ww3_ounp` and `gx_outp` call it too.

## What was ported

| | |
|---|---|
| WW3 routine | `W3FLX4`, `model/src/w3flx4md.F90:105-231` at NOAA-EMC/WW3 `03adda91`; compiled by 54 regtest switches (`ww3_ts1` 5, `ww3_ts2` 5, `ww3_ts3` 24, `mww3_test_05` 20, all ST6) and by `model/bin/switch_NRL3`, `switch_UoM_nl1`, `switch_UoM_nl3`, `switch_UoM_nl3s`, `switch_swin`; by no `switch_NCEP_*` or `switch_NWS_*` (`git grep -lw FLX4 -- '*switch*'`). Its only `#ifdef` blocks are `W3_S` (subroutine tracing); no variant changes the arithmetic |
| Kokkos code | [`../src/ww_kokkos/srce_flx4.hpp`](../src/ww_kokkos/srce_flx4.hpp), `ww::srce::flx::w3flx4_point`; `FLX4A0` of the current grid (`w3gdatmd.F90:2610`) passed by value; returns 1 where the Fortran stops in `EXTCDE(1)` (`:199-202`), outputs not written |
| C shim | none yet; it writes the message of `:200` (format `:225-227`) on `IAPROC == NAPERR` and stops the run on 1 |
| Plan | issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3flx4md.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd` with `ST6 FLX4`), recompiled with `-fdump-tree-optimized-lineno` and found
identical to the CMake object; compared with a C++ unit under the `openmp-release` flags plus
`-ffp-contract=off`, 2026-10-16.

- (v) Same operations in the same order on both branches: `|ZWND-10| > 0.01`;
  `UST = SQRT(FLX4A0)*2.026`, `CD = (UST/U10)*(UST/U10)`; else
  `((0.967*U10 + 8.058) - (U10*U10)*0.016) * FLX4A0 * 1E-4` and `UST = U10*SQRT(CD)`; then
  `-(0.4/SQRT(CD))`, `expf`, `*ZWND`. The operand swaps the object makes are commutative
  multiplications and additions of two operands, which give the same bits.
- (v) The nine single-precision constants (10, 0.01, 50.33, 2.026, 0.967, 8.058, 0.016, 1E-4, 0.4)
  have the same bits in `.rodata.cst4` of both objects, in the same order.
- (v) `U10**2` is `U10*U10` in the object, no `__powisf2`; `SQRT` is `sqrtss` and `EXP` a call of
  glibc's `expf` in both; no libmvec, no FMA, no reduction, no MIN/MAX.
- (v) The NaN paths agree: a NaN `ZWND` passes the test of `:199` and a NaN `U10` takes the
  `ELSE` branch of `:210` in both objects (`comiss` with `jbe`/`jb`). g++ keeps a branch to
  `sqrtf` for a negative argument (`-fmath-errno`), which returns the same NaN as `sqrtss`.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, `#cuda` shell), never run: the SASS holds `FFMA` from
  CUDA's own `expf`, `sqrtf` and division sequences, so the device bits depend on CUDA's math
  library, not glibc's.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3flx4md.F90` under `build_st6f4` (identical
  to the CMake object) and the header under `openmp-release` (warning-free); compared the GIMPLE
  and the assembly operation by operation on both branches, the nine `.rodata.cst4` constants
  (same bits, same order), the calls (`expf` in both; the C++ `sqrtf` calls sit only on the
  `x < 0` errno path and return the `sqrtss` NaN), the NaN and `-0` paths, the outputs written and
  the `EXTCDE` path (`w3servmd.F90:1207`, always exits); recompiled for CUDA (`sm_89`,
  `--fmad=false`), compile only. Nothing changed. The C++ reloads `CD` after storing `UST`
  because references may alias; the shim must pass distinct variables, as the Fortran assumes.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
