# JULDAYT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Conversion of a calendar day, month and year to a Julian day number (Numerical Recipes, 2nd ed.,
`julday`, with day and month swapped in the argument list), in default `REAL`. Its callers are
`TIDE_READ_ANAPAR` (`w3tidemd.F90:818,821`) and `TIDE_READ_TIMESERIES` (`:879`), and nothing in
WW3 calls either (`git grep -i 'tide_read_anapar\|tide_read_timeseries'` in `WW3-noaa/model/src`
finds only their definitions and the module's header list): the routine is translated for
completeness and can never be exercised by a regtest. Its inverse is
[`CALDATT`](CALDATT_KOKKOS.md).

## What was ported

| | |
|---|---|
| WW3 routine | `JULDAYT`, `model/src/w3tidemd.F90:1257-1281` at NOAA-EMC/WW3 `03adda91`, 25 lines. `w3tidemd.F90` is compiled by every build; the `TIDE` switch is in `ww3_tp2.18` (`switch_TIDE`, `switch_TIDE_MPI`) and `model/bin/switch_Ifremer1`, `switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OAS{ACM,ICM,OCM}`, `switch_ite_pdlib` (`git grep -lw TIDE -- '*switch*'`), in no `switch_NCEP_*` or `switch_NWS_*`. `JULDAYT` has no `#ifdef` block, so `TIDE` does not change it |
| Kokkos code | [`../src/ww_kokkos/tide_juldayt.hpp`](../src/ww_kokkos/tide_juldayt.hpp), `ww::tide::juldayt(id, mm, iyyy)`, returns the day number |
| C shim | none, and no caller to give one |
| Plan | issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3tidemd.F90` from WW3's CMake Release flags, gfortran 16.2.0,
under `switch_lab_shrd` (TIDE off) and the same with `TIDE UOST FLD2 IG1 IC3 IS2 BT8` (TIDE on),
both recompiled and checked identical to the CMake objects; compared with the `-O3` GIMPLE and
object of a throwaway C++ unit (openmp-release flags, `-ffp-contract=off`, no warning), 2026-10-16.

- (v) The `juldayt` GIMPLE and assembly are the same with TIDE on and off.
- (v) Same single-precision operations in both: `float(jy)*365.25`, `float(jm)*30.6001`, and on
  the Gregorian branch `float(jy)*0.01` (the `jy` after the `mm<=2` decrement, as the Fortran) and
  `float(ja)*0.25`; every `INT()` a `cvttss2si`. No FMA, no libm call. The four `.rodata`
  constants have the same bits (`0x43b6a000`, `0x41f4cd01`, `0x3c23d70a`, `0x3e800000`); `30.6001`
  and `0.01` are `float` literals (one rounding, as gfortran) with a `static_assert` of their bits.
- (v) `IGREG` folds to 588829 in both (`>= 588829` is `> 588828` in the GIMPLE); the test uses
  the dummy `iyyy`, not `jy`, as the Fortran. Integer sums are reassociated by both compilers; the
  result is the same in two's complement.
- (v) `INT()` goes through `ww::st4::fint`, which gives `cvttss2si`'s result (`INT_MIN`) out of
  range; g++ keeps the range test at the two `INT()` of line 1275 and drops it on the Gregorian
  branch, where the operand is always in range.
- The `WRITE(6,*) 'There is no zero year !!'` at `:1267` is not in the kernel (a kernel does not
  print); it fires when `iyyy == 0` on entry and does not stop, so a caller that wants it tests
  `iyyy` before the call. The arithmetic after it is the same.
- ⚠ Integer overflow of `31*(mm+12*iyyy)` for `|iyyy|` above about 5.7 million is undefined in
  both languages; not reachable from a date.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: the module recompiled from `build_mixa` and `build_lab` flags (identical
  to both CMake objects) and a new C++ unit compared instruction by instruction: the same four
  `mulss` on the same operands (`jy` after the `mm<=2` decrement), the same four constants
  (`0x43b6a000`, `0x41f4cd01`, `0x3c23d70a`, `0x3e800000`), `cvttss2si` or `fint` (same result for
  every input, `INT_MIN` out of range), `> 588828` on `id+31*(mm+12*iyyy)`; no defect, nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest can exercise it: nothing calls its callers.
