# VMIN_D_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Zakharov's (1967) transfer coefficient for the three-wave difference interaction of deep-water waves
without current, a function internal to `SECONDHH` (`w3iogomd.F90:4622-4863`), which builds the
second-order harmonic tables of `SKEWNESS` (`:4944`). `W3OUTG` calls `SKEWNESS` only when output
field 8-7, 8-8 or 8-9 is requested (`:2355-2357`).

## What was ported

| | |
|---|---|
| WW3 routine | `VMIN_D`, `model/src/w3iogomd.F90:4777-4816` at NOAA-EMC/WW3 `03adda91`, with `DEL1 = 1.0E-8` and `ZCONST = 0.0281349` from its host `SECONDHH` (`:4654-4655`). `w3iogomd` is compiled by every switch; the function runs only with the skewness output fields on. No `#ifdef` inside it |
| Callers | `SECONDHH` only (`:4712`, `:4742`, `:4743`); `VPLUS_D` (`:4820-4860`) is a separate routine |
| Kokkos code | [`../src/ww_kokkos/iogo_vmin_d.hpp`](../src/ww_kokkos/iogo_vmin_d.hpp), `ww::iogo::vmin_d` |
| C shim | none yet |

## Arithmetic

Reference: the `-O3` object of `w3iogomd.F90` recompiled with the flags of WW3's CMake Release build
under `switch_lab_shrd` (gfortran 16.2.0, pinned shell), disassembly identical to the CMake object;
gfortran emits `VMIN_D` as the clone `vmin_d.1.isra.0`. Compared with a throwaway C++ unit
compiled with the `openmp-release` flags, 2026-10-16.

- (v) Operation by operation the same in both `-O3` GIMPLE dumps: three `ABS`+`DEL1`, three `+DEL1`,
  each ratio as `(OI*OJ)*RK / ((RI*OK)*RJ)` (and the two permutations), `sqrt`, then the sum
  left to right and the product by `ZCONST` last. No FMA, no reciprocal, no libm call in either.
- (v) `DEL1` and `ZCONST` rounded once from the decimal literal: `9.99999993922529e-9`
  (`0x322bcc77` in the Fortran object's `.rodata`) and `2.813489921391010284423828125e-2` in both dumps.
- ⚠ The C++ keeps a `sqrtf` call for a negative argument (errno); gfortran emits a bare `sqrtss`.
  Both give NaN there; the argument is a ratio of positive terms whenever the `XO*` frequencies are
  positive, which `SECONDHH` builds them as (`:4738`), so that branch is not taken.
- CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. Object of `w3iogomd.F90` recompiled from `build_lab`'s flags (disassembly identical to the CMake object); `vmin_d.1.isra.0` GIMPLE and assembly compared with the C++ unit operation by operation (same 9 adds, 18 products with the same pairing, 3 divisions, 3 square roots, sum order, `ZCONST` last, same constants); all three calls in `SECONDHH` (`:4712`, `:4742`, `:4743`) go to the clone, none inlined; no `#ifdef` between `:4622` and `:4863`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
