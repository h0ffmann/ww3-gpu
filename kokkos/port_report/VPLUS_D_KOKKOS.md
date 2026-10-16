# VPLUS_D_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Zakharov's (1967) transfer coefficient for the three-wave sum interaction of deep-water waves
without current, a function internal to `SECONDHH` (`w3iogomd.F90:4622-4863`), which builds the
second-order harmonic tables of `SKEWNESS` (`:4883`, call at `:4944`). `W3OUTG` calls `SKEWNESS` only
when output field 8-7, 8-8 or 8-9 is requested (`:2355-2357`).

## What was ported

| | |
|---|---|
| WW3 routine | `VPLUS_D`, `model/src/w3iogomd.F90:4820-4860` at NOAA-EMC/WW3 `03adda91`, with `DEL1 = 1.0E-8` and `ZCONST = 0.0281349` from its host `SECONDHH` (`:4654-4655`). `w3iogomd` is compiled by every switch; the function runs only with the skewness output fields on. No `#ifdef` inside it |
| Callers | `SECONDHH` only, once (`:4713`, as `VPLUS_D(-XK3,XK1,XK2,-X13,-X32,X12,OM3,OM1,OM2)`) |
| Kokkos code | [`../src/ww_kokkos/iogo_vplus_d.hpp`](../src/ww_kokkos/iogo_vplus_d.hpp), `ww::iogo::vplus_d` |
| C shim | none yet |

## Arithmetic

Reference: the `-O3` object of `w3iogomd.F90` recompiled with the flags of WW3's CMake Release build
under `switch_lab_shrd` (gfortran 16.2.0, pinned shell), disassembly identical to the CMake object.
gfortran emits no standalone `VPLUS_D`: it is inlined into `SECONDHH` at `:4713`. Compared with a
throwaway C++ unit (the general function, and a wrapper with the negated actuals of `:4713`)
compiled with the `openmp-release` flags, 2026-10-16.

- (v) Against the inlined copy, operation by operation the same in both `-O3` GIMPLE dumps: three
  `ABS`+`DEL1` (`ABS(-XK3)` folded to `ABS(XK3)` in both), three `+DEL1`, each ratio as
  `(OI*OJ)*RK / ((RI*OK)*RJ)` and the two permutations, `sqrt`, then `XIJ+RI*RJ` with
  `XIJ = -X13` folded to `RI*RJ - X13` (likewise `-X32`), the sum left to right and the product by
  `ZCONST` last. No FMA, no reciprocal, no libm call in either. In the Fortran `RJ`, `RK`, `OJ`,
  `OK`, `OJ*OK` and `RJ*RK` are hoisted out of the direction loops; the values are the same.
- (v) `DEL1` and `ZCONST` rounded once from the decimal literal: `9.99999993922529e-9` and
  `2.813489921391010284423828125e-2` in both dumps.
- ⚠ The general form (no negated actuals) has no Fortran object to compare with, since the only
  call site is inlined; its GIMPLE is the same as `VMIN_D`'s except the two `+`, and `VMIN_D`'s
  out-of-line clone matched (`VMIN_D_KOKKOS.md`).
- ⚠ The C++ keeps a `sqrtf` call for a negative argument (errno); gfortran emits a bare `sqrtss`.
  Both give NaN there; the arguments are ratios of positive terms whenever the `XO*` frequencies
  are positive, which `SECONDHH` builds them as (`:4706-4708`).
- CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. `w3iogomd.F90` recompiled from `build_lab`'s `flags.make` (disassembly identical to the CMake object) and a C++ unit calling `vplus_d` with the negated actuals of `:4713`; the inlined copy (`:4849-4858` in the GIMPLE) and the C++ GIMPLE agree operation by operation (operands, products, the three divisions, `sqrt`, `RI*RJ - X13`, `RI*RK - X32`, `X12 + RJ*RK`, the left-to-right sum, `ZCONST` last, same constant bits); no FMA, reciprocal or `rsqrt` in the C++ assembly. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
