# CALDATT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Conversion of a Julian day number to calendar day, month and year (Numerical Recipes, 2nd ed.,
`caldat`, with day and month swapped in the argument list), in default `REAL`. Its only caller is
`TIDE_WRITE_RESULTS` (`w3tidemd.F90:163-164`), and nothing in WW3 calls `TIDE_WRITE_RESULTS`
(`git grep -i tide_write_results` in `WW3-noaa` finds its definition only): the routine is
translated for completeness and can never be exercised by a regtest. `W3TIMEMD` has its own
`CALDAT` (`w3timemd.F90:794-836`), which the Fortran comment at `:1286` says should replace it.

## What was ported

| | |
|---|---|
| WW3 routine | `CALDATT`, `model/src/w3tidemd.F90:1284-1310` at NOAA-EMC/WW3 `03adda91`, 27 lines. `w3tidemd.F90` is compiled by every build; the `TIDE` switch is in `ww3_tp2.18` (`switch_TIDE`, `switch_TIDE_MPI`) and `model/bin/switch_Ifremer1`, `switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OAS{ACM,ICM,OCM}`, `switch_ite_pdlib` (`git grep -lw TIDE -- '*switch*'`), in no `switch_NCEP_*` or `switch_NWS_*`. `CALDATT` has no `#ifdef` block, so `TIDE` does not change it |
| Kokkos code | [`../src/ww_kokkos/tide_caldatt.hpp`](../src/ww_kokkos/tide_caldatt.hpp), `ww::tide::caldatt(julian, id, mm, iyyy)`, outputs by reference |
| C shim | none, and no caller to give one |
| Plan | issue #42, PR #159 |

## Arithmetic

Reference: the `-O3` object of `w3tidemd.F90` from WW3's CMake Release flags, gfortran 16.2.0,
under `switch_lab_shrd` (TIDE off) and the same with `TIDE UOST FLD2 IG1 IC3 IS2 BT8` (TIDE on),
both recompiled and checked identical to the CMake objects; compared with the `-O3` object of a
throwaway C++ unit (openmp-release flags, `-ffp-contract=off`), 2026-10-16.

- (v) The `caldatt` GIMPLE is the same with TIDE on and off.
- (v) Same single-precision sequence in both objects: `cvtsi2ss`, `subss 0.25`, `divss 36524.25`;
  `mulss 0.25`; `ja-2438346` (gfortran folds `jb-2439870`), `subss 122.1`, `divss 365.25`,
  `addss 6680`; `mulss 0.25`; `divss 30.6001`; `mulss 30.6001`; every `INT()` a `cvttss2si`.
  No reciprocal, no FMA, no libm call. The six `.rodata` constants have the same bits; `122.1` and
  `30.6001` are written as `float` literals (one rounding, as gfortran) with a `static_assert` of
  their bits.
- (v) `INT()` goes through `ww::st4::fint`, which gives `cvttss2si`'s result (`INT_MIN`) out of
  range; the C++ object keeps the range test only at `INT(30.6001*je)`, where the compiler could
  not drop it, and returns the same value there.
- (v) Integer branches (`mm>12`, `mm>2`, `iyyy<=0`) give the same results; gfortran turns them
  into `cmov`/`setl`, g++ into branches.
- ⚠ Integer overflow for `|julian|` near `2**31` is undefined in both languages; not reachable
  from a date.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3tidemd.F90` under `switch_lab_shrd` and the TIDE-on switch (both identical to the CMake objects, same `caldatt` GIMPLE) and the header in a throwaway unit; the GIMPLE matches operation by operation (operands, folded constants `122.1`, `30.6001`, `ja-2438346`, every `INT()` a truncation), and the object's out-of-range path at `INT(30.6001*je)` gives `cvttss2si`'s `INT_MIN` and skips the `mm>12` test only where `je` makes its result certain. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest can exercise it: nothing calls its caller.
