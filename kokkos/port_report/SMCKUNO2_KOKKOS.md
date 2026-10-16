# SMCKUNO2_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Refraction shift of the spectrum in wavenumber space on the Spherical Multiple-Cell (SMC) grid,
by the second-order UNO2 scheme. For each direction `n` the wave action `Spectr(-1:NK+2)` is
extended by zero below the first bin and by two factors `XFR**(-7)` above the last; the gradient
`SpeRfr(j) = (Spectr(j+1)-Spectr(j))/DKS(j)` is taken on every face, and the flux on faces
`0..NK` is the upwind value plus a limited correction:

- `CoRfr > 0`: `C0 = MIN(CTMAX*DKC(j), CoRfr)`,
  `F(j) = C0*(Spectr(j) + SIGN(0.5,SpeRfr(j))*(DKC(j)-C0)*MIN(|SpeRfr(j-1)|,|SpeRfr(j)|))`;
- otherwise: `C0 = MIN(CTMAX*DKC(j+1), -CoRfr)`,
  `F(j) = -C0*(Spectr(j+1) - SIGN(0.5,SpeRfr(j))*(DKC(j+1)-C0)*MIN(|SpeRfr(j+1)|,|SpeRfr(j)|))`;

then `SpeTHK(n,j) = MAX(0, Spectr(j) + (F(j-1)-F(j))/DKC(j))` for `j = 1..NK`.

## What was ported

| | |
|---|---|
| WW3 routine | `SMCkUNO2`, `model/src/w3psmcmd.F90:2821-2890` at NOAA-EMC/WW3 `03adda91`, author Jian-Guo Li. Called only by `W3KRTN` (`w3psmcmd.F90:1253`, `:1266`, under `FLCK`), itself called by `W3WAVE` for an SMC grid (`w3wavemd.F90:1830`, `:2154`, `#ifdef W3_SMC`). No `#ifdef` inside the routine. Module `W3PSMCMD` is compiled under switch `SMC` (`switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, regtests `ww3_tp2.10`, `ww3_tp2.14`, `ww3_tp2.16`, `mww3_test_09`; `git grep -lw SMC -- '*switch*'`), by no NCEP or NWS switch |
| Kokkos code | [`../src/ww_kokkos/psmc_smckuno2.hpp`](../src/ww_kokkos/psmc_smckuno2.hpp): `ww::psmc::smckuno2_cnst` (`XFR**(-7)`), `ww::psmc::smckuno2_point` (one direction) and `ww::psmc::smckuno2` (`n = 1..NTH`, the Fortran order). Module state `NK`, `NTH`, `XFR`, `CTMAX` (`w3gdatmd`) are arguments |
| Work arrays | the automatic arrays `Spectr`, `SpeRfr`, `SpeFlx(-1:NK+2)` (heap-allocated by gfortran per call, `__builtin_malloc` in the GIMPLE) are caller-provided, `NK+4` reals each, one set per concurrent direction |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3psmcmd.F90` under `model/bin/switch_SMCMlt` (WW3's CMake
Release flags, gfortran 16.2), recompiled and checked identical to the CMake object, 2026-10-16;
the C++ unit compiled warning-free with the openmp-release flags (g++ 16.2).

- (v) `XFR**(-7)` is no `powf` call: gfortran builds `x2=x*x`, `x3=x*x2`, `x4=x2*x2`,
  `x7=x4*x3`, `1/x7`; `smckuno2_cnst` writes that tree and the C++ object computes the same four
  products and one division.
- (v) `SIGN(0.5, SpeRfr(j))` is `copysignf` (and/andn/or masks) in both objects.
- (v) `MIN`: in all four sites the Fortran object keeps the first operand only when strictly less
  (`minss`): `CTMAX*DKC < CoRfr`, `CTMAX*DKC(j+1) < -CoRfr`, `|SpeRfr(j-1)| < |SpeRfr(j)|`,
  `|SpeRfr(j+1)| < |SpeRfr(j)|`; the header writes `a < b ? a : b` in that order and the C++
  object has the same `vminss` operand order.
- (v) `MAX(0.0, v)` keeps `v` only when `v > 0` in the vectorised body (`maxps`) and the scalar
  tail (`maxss`) of the Fortran; the C++ `v > 0 ? v : 0` gives `vmaxps`/`vmaxss` with the same
  order. A NaN becomes 0 in both.
- (v) Parentheses: `(DKC-C0)` and `(Spectr ± ...)` are `PAREN_EXPR` in the Fortran GIMPLE; the
  products are `((DKC-C0)*sign)*MIN`, then the sum, then `*C0`, then a negation (`xorps`) in the
  negative branch; the C++ object performs the same operations in the same order. No FMA.
- (v) No reduction; the update loop is elementwise, so its vectorisation changes no bit.
- (v) `SpeRfr(-1) = 0` is overwritten at `j = -1` by `(Spectr(0)-Spectr(-1))/DKS(-1)`, i.e.
  `0/DKS(-1)`, kept as written.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. `w3psmcmd.F90` recompiled from `build_smc`'s flags
  (identical to the CMake object) and a C++ unit recompiled with the openmp-release flags; GIMPLE
  and assembly compared site by site: the `XFR**(-7)` power tree, `copysignf`, the four `minss`
  operand orders, the `PAREN_EXPR` grouping and the negation of both flux branches, `maxps`/`maxss`
  (`v > 0 ? v : 0`) in the SSE body, epilogue and scalar tail against the AVX body and tail, the
  NaN branch of `CoRfr > 0`, and the index bases of `CoRfr`, `SpeTHK`, `DKC`, `DKS` and the three
  work arrays. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. `ww3_tp2.10` and `ww3_tp2.16` set `RUN%FLCK = T`
(`regtests/ww3_tp2.10/input/ww3_grid.nml:22`, `ww3_tp2.16/input/ww3_grid.nml:22`), so their SMC
runs reach this routine once it is wired ⚠ (read from the inputs, not run).
