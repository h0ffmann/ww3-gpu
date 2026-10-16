# W3IC5WNCG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Complex wavenumber of waves in ice for every frequency of the grid (switch `IC5`): for each
`IK` it calls `FSDISP` (the Fox-Squire dispersion relation) at the period `TPI/SIG(IK)` and stores
its `(k_r, k_i)` in `WN_R(IK)`, `WN_I(IK)`. Its only caller is `W3SIC5`
(`w3sic5md.F90:427`), which passes arrays of size `NK` (`:352`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3IC5WNCG`, `model/src/w3sic5md.F90:493-633` at NOAA-EMC/WW3 `03adda91`; compiled by the `IC5` regtest switches `ww3_tic1.1/input_IC5_M1`, `input_IC5_M2`, `input_IC5_M3` (`git grep -lw IC5 -- '*switch*'`), by no `switch_NCEP_*`/`switch_NWS_*`. Its only `#ifdef` blocks are `W3_S` tracing |
| Kokkos code | [`../src/ww_kokkos/ic5_w3ic5wncg.hpp`](../src/ww_kokkos/ic5_w3ic5wncg.hpp), `ww::ic5::w3ic5wncg`, a template on the `FSDISP` callable |
| Structural changes | the `ALLOCATABLE SIGMA` (`:600`) is a caller-provided work array of at least `max(SIZE(CG), SIZE(WN_R))` values; `EXTCDE(3)` (`:611-614`) returns 3, the shim writes format 900 and stops; `FSDISP` (`:652-943`, not translated) is a callable returning 0 or its own `EXTCDE` code, which is returned before `WN_R(IK)`, `WN_I(IK)` are stored |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3sic5md.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd` with `NL4 IC5 BT9 NLS`), recompiled and checked identical to the CMake object;
compared with a throwaway C++ unit under the `openmp-release` flags, 2026-10-16.

- (v) The only floating-point operation is `TPI/SIGMA(IK)`: one `divss` by the folded constant
  `0x40c90fdb` (`6.2831855`, `2.0*PI` in `REAL`) in both objects; the C++ unit compiles warning-free
  with one `vdivss` from the same constant.
- (v) `SIGMA = SIG(1:NK)` and `SIGMA = SIG(0:NK+1)` reallocate `SIGMA` to the section's size
  (`__builtin_realloc` in the GIMPLE, `-frealloc-lhs`), so only `SIZE(WN_R)` selects the bounds;
  `SIZE(CG)` only sizes the first allocation and its zero fill, which the copy overwrites.
- (v) `FSDISP` is called, not inlined, in the object; no libm, libmvec, reduction or MIN/MAX in the
  routine.
- ⚠ After `EXTCDE(3)` the object enters the loop with an undefined `KU` (`ku_127(D)` in the
  GIMPLE); it is never reached because `EXTCDE` stops the run. The C++ returns 3 instead.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. `w3sic5md.F90` recompiled from `build_mixb`'s flags and
  found identical to the CMake object; the single `divss` (`:620`, GIMPLE
  `6.283185482025146484375e+0 / _69`) loads `.LC46` = `0x40c90fdb`, the same bits as the C++
  unit's `vdivss` constant; argument order, bounds selection by `SIZE(WN_R)`, the realloc and the
  undefined `KU` path read again in the GIMPLE. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
