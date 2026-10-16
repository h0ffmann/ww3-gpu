# SMCYUNO3R_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Mid-flux values on the V-faces (faces normal to y) of a single-resolution Spherical
Multiple-Cell (SMC) grid, by the third-order UNO3 scheme. For each V-face `j`, with the
upstream, central and downstream cells `K, L, M, N = IJKVFc(4:7,j)`:

- `CNST5 = CF(M)-CF(L)`, `CNST6 = 0.5*(VC(L)+VC(M))`, `CNST7 = CLATF(j)*FLOAT(IJKVFc(3,j))`;
- `CNST6 >= 0`: `VFLY(j) = VC(L)` when `M <= 0`, else `CNST6`; `CNST4 = CF(L)-CF(K)`,
  `CNST8 = CF(M)-CF(K)`, `CNST9 = CNST5-CNST4`; `CNST = 0.5*CNST5-(1+VFLY)*CNST9/6` when
  `|CNST9| <= 0.6*|CNST8|`, else `SIGN(1.0,CNST5)*MIN(|CNST4|,|CNST5|)` when
  `DBLE(CNST4)*DBLE(CNST5) > 0`, else `SIGN(0.5,CNST5)*MIN(...)`;
  `FV(j) = (CF(L) + CNST*(1-VFLY(j)))*CNST7`;
- otherwise: `VFLY(j) = VC(M)` when `L <= 0`, else `CNST6`; `CNST4 = CF(N)-CF(M)`,
  `CNST8 = CF(N)-CF(L)`, `CNST9 = CNST4-CNST5`; `CNST = 0.5*CNST5+(1-VFLY)*CNST9/6` in the
  limited zone, the same two limiter cases otherwise; `FV(j) = (CF(M) - CNST*(1+VFLY(j)))*CNST7`;
- `FY(j) = AKDif*CNST5*CNST7` (diffusion flux).

## What was ported

| | |
|---|---|
| WW3 routine | `SMCyUNO3r`, `model/src/w3psmcmd.F90:2211-2320` at NOAA-EMC/WW3 `03adda91`, author Jian-Guo Li. Called only by `W3PSMC` (`w3psmcmd.F90:583`), when `NRLv == 1` (`:487`, single resolution, `LvSMC` in the `PSMC` namelist) and `FUNO3` is true (`:581`, `UNO3` in the `PSMC` namelist). The only `#ifdef` is `W3_OMPG` (`:2229-2233`, `:2314-2317`): an OpenMP `DO` over independent faces. Module `W3PSMCMD` is compiled under switch `SMC` (`switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, regtests `ww3_tp2.10`, `ww3_tp2.14`, `ww3_tp2.16`, `mww3_test_09`; `git grep -lw SMC -- '*switch*'`), by no NCEP or NWS switch |
| Kokkos code | [`../src/ww_kokkos/psmc_smcyuno3r.hpp`](../src/ww_kokkos/psmc_smcyuno3r.hpp): `ww::psmc::smcyuno3r_point` (one face) and `ww::psmc::smcyuno3r` (`j = NVA..NVB`, the Fortran order). Module state `IJKVFc`, `CLATF` (`w3gdatmd`) are arguments, taken as contiguous, as in [`SMCYUNO2R_KOKKOS.md`](SMCYUNO2R_KOKKOS.md) |
| Work arrays | none |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3psmcmd.F90` under `model/bin/switch_SMCMlt` (WW3's CMake
Release flags, gfortran 16.2), recompiled from `build_smc`'s `flags.make` and checked identical
to the CMake object, 2026-10-16; the C++ unit compiled warning-free with the openmp-release
flags (g++ 16.2). The two loop bodies were compared instruction by instruction.

- (v) The Fortran loop is scalar (`-fopt-info-vec` reports nothing for `:2235-2312`); the C++
  object is scalar too. No FMA in either.
- (v) `MIN(ABS(CNST4), ABS(CNST5))` has a different operand order in the two branches of the
  Fortran object: for `CNST6 >= 0` (`:2272`, `:2275`) `minss` keeps `|CNST5|` only when strictly
  less, for `CNST6 < 0` (`:2299`, `:2302`) it keeps `|CNST4|` only when strictly less. The header
  writes `a5 < a4 ? a5 : a4` and `a4 < a5 ? a4 : a5`; the C++ object's `vminss` has the same
  operand order in each branch.
- (v) `DBLE(CNST4)*DBLE(CNST5) .GT. 0.d0` is `cvtss2sd`, `mulsd`, `comisd` in both objects, `CNST4`
  the first factor; a NaN takes the `SIGN(0.5,...)` branch in both.
- (v) `SIGN(1.0,CNST5)*MIN(...)` is an `xor` of the sign bit of `CNST5` into the minimum in both
  objects; `SIGN(0.5,CNST5)*MIN(...)` is `copysignf` masks then `mulss` with the minimum first.
- (v) `|CNST9| <= 0.6*|CNST8|` is `|CNST8|*0.6` then `comiss`/`jb` in both (a NaN takes the
  limiter branch). `CNST9/6.0` is a division by 6 in both, not a reciprocal multiply.
- (v) Operation order, identical in both objects: `(VC(L)+VC(M))*0.5`; `CF(M)-CF(L)`;
  `cvtsi2ss(IJKVFc(3,j))*CLATF(j)`; `0.5*CNST5 - ((1+VFLY)*CNST9)/6` (positive) and
  `((1-VFLY)*CNST9)/6 + 0.5*CNST5` (negative); `(1+VFLY)*CNST` and `CF(M)-` (negative);
  `+CF(L)`, `*CNST7`. Nothing is reassociated in the C++.
- ⚠ Two products have their operands the other way round in the C++ object, which changes a
  result only when both operands are NaN (x86 returns the first operand's payload): `CNST*(1-VFLY)`
  in the positive branch (Fortran `cnst*(1-v)`, C++ `(1-v)*cnst`), unreachable since a NaN
  `VFLY` there makes `CNST6` NaN and takes the negative branch; and `AKDif*CNST5` (Fortran
  `CNST5*AKDif`, C++ `AKDif*CNST5`), different only when `AKDif` and `CNST5` are both NaN. Writing
  `cnst5 * akdif` in the source did not change g++'s order.
- (v) `K` is loaded only in the positive branch and `N` only in the negative one, as in the object;
  `VFLY(j)` is stored, then the same value is used for `FV(j)`.
- (v) No reduction.
- (v) `W3_OMPG`: `w3psmcmd.F90` recompiled with `-DW3_OMPG -fopenmp` (no CMake object of this
  variant exists here) and the outlined `smcyuno3r._omp_fn.0` read: the per-face arithmetic and the
  `minss` operand order per branch are those of the serial object. Two products swap operands
  there (`(1-VFLY)*CNST` positive, `CNST*(1+VFLY)` negative), a NaN-pair payload difference of
  WW3's own two builds, not a change of value.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3psmcmd.F90` from `build_smc`'s
  `flags.make` (identical to the CMake object) and with `-DW3_OMPG -fopenmp`, and the header in a
  throwaway openmp-release unit; read the `minss` operands of both branches in the plain object,
  the `W3_OMPG` outlined body and the C++ object (positive `|CNST5|` first, negative `|CNST4|`
  first in all three), the `DBLE` product, the `0.6` and `/6.0` sites and every product's first
  operand; the only swap reachable with two NaNs is `AKDif*CNST5`, and that swap changes only the
  NaN payload. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest of WW3 reaches this routine ⚠ (read from the inputs, not
run): `UNO3 = .TRUE.` appears only in `ww3_tp2.10` (`regtests/ww3_tp2.10/input/namelists_SMC0512.nml:1`),
which sets `LvSMC=3` and so takes the multi-resolution path (`w3psmcmd.F90:663` on), and
`ww3_tp2.16` leaves `UNO3` false.
