# SMCXUNO2R_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Mid-flux values on the U-faces (faces normal to x) of a single-resolution Spherical
Multiple-Cell (SMC) grid, by the second-order UNO2 scheme. For each U-face `i`, with the
upstream, central and downstream cells `K, L, M, N = IJKUFc(4:7,i)`:

- `CNST2 = FLOAT(IJKCel3(L))`, `CNST3 = FLOAT(IJKCel3(M))` (cell x-sizes),
  `CNST5 = CF(M)-CF(L)`, `CNST6 = 0.5*(UC(L)+UC(M))`, `CNST0 = 2/(CLATS(ij)*CLATS(ij))` with
  `ij = MAX(L,M)`;
- `CNST6 >= 0`: `UFLX(i) = UC(L)` when `M <= 0`, else `CNST6`;
  `FU(i) = CF(L) + SIGN(0.5,CNST5)*MIN(|CF(L)-CF(K)|,|CNST5|)*(1-UFLX(i)/CNST2)`;
- otherwise: `UFLX(i) = UC(M)` when `L <= 0`, else `CNST6`;
  `FU(i) = CF(M) - SIGN(0.5,CNST5)*MIN(|CF(N)-CF(M)|,|CNST5|)*(1+UFLX(i)/CNST3)`;
- `FX(i) = AKDif*CNST0*CNST5/(CNST2+CNST3)` (diffusion flux).

## What was ported

| | |
|---|---|
| WW3 routine | `SMCxUNO2r`, `model/src/w3psmcmd.F90:1551-1644` at NOAA-EMC/WW3 `03adda91`, author Jian-Guo Li. Called only by `W3PSMC` (`w3psmcmd.F90:493`), when `NRLv == 1` (`:487`, single resolution) and `FUNO3` is false (`:488`). The only `#ifdef` is `W3_OMPG` (`:1573-1577`, `:1638-1641`): an OpenMP `DO` over independent faces, no arithmetic change. Module `W3PSMCMD` is compiled under switch `SMC` (`switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, regtests `ww3_tp2.10`, `ww3_tp2.14`, `ww3_tp2.16`, `mww3_test_09`; `git grep -lw SMC -- '*switch*'`), by no NCEP or NWS switch |
| Kokkos code | [`../src/ww_kokkos/psmc_smcxuno2r.hpp`](../src/ww_kokkos/psmc_smcxuno2r.hpp): `ww::psmc::smcxuno2r_point` (one face) and `ww::psmc::smcxuno2r` (`i = NUA..NUB`, the Fortran order). Module state `IJKUFc`, `IJKCel3`, `CLATS` (`w3gdatmd`) are arguments, taken as contiguous with their allocated bases (`IJKUFc(7,MUFc)`, `IJKCel3(-9:MCel)`, `CLATS(0:MSEA)`, `w3gdatmd.F90:1771`, `:1779`, `:1741`; the pointer assignments at `:2473`, `:2480`, `:2445` keep those bases) |
| Work arrays | none |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3psmcmd.F90` under `model/bin/switch_SMCMlt` (WW3's CMake
Release flags, gfortran 16.2), recompiled and checked identical to the CMake object, 2026-10-16;
the C++ unit compiled warning-free with the openmp-release flags (g++ 16.2).

- (v) The Fortran loop is scalar (not vectorised); the C++ object is scalar too.
- (v) `FLOAT(IJKCel3(.))` is one `cvtsi2ss` in both objects.
- (v) `MIN(ABS(CNST4), ABS(CNST5))`: in both branches the Fortran object keeps `|CNST4|` only
  when strictly less (`minss %xmm8(|CNST5|),%xmm0(|CNST4|)`); the header writes
  `a4 < a5 ? a4 : a5` and the C++ object has `vminss` with the same operand order.
- (v) `SIGN(0.5, CNST5)` is `copysignf` (and/or masks) in both objects.
- (v) `MAX(L, M)` is an integer `MAX_EXPR` (`cmovge`) in both; no rounding involved.
- (v) Operation order, identical in both objects up to commutation of one product:
  `(UC(L)+UC(M))*0.5`; `CF(M)-CF(L)`; `2/(CLATS*CLATS)`; `(sign*min)*(1-UFLX/CNST2)`, `CF(L)+`
  (positive); `(sign*min)*(1+UFLX/CNST3)`, `CF(M)-` (negative);
  `((AKDif*CNST0)*CNST5)/(CNST2+CNST3)`. The parentheses are `PAREN_EXPR` in the Fortran GIMPLE;
  nothing is reassociated in the C++. No FMA.
- (v) `UFLX(i)` is stored and then read back for `FU(i)`; the Fortran GIMPLE uses the stored
  value (`cstore` PHI), the header uses the same local value.
- (v) `K` is loaded only in the positive branch and `N` only in the negative one, as in the object.
- (v) No reduction.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3psmcmd.F90` under `switch_SMCMlt`
  (identical to the CMake object) and the C++ unit (warning-free); compared the `-O3` GIMPLE and
  the assembly operation by operation: loads and branches, `cvtsi2ss`, `MAX_EXPR` as `cmovge`,
  `copysignf`, `minss`/`vminss` with `|CNST4|` as destination in both branches, the `FU` and
  `FX` operation order (commutations only), the `CNST6 >= 0` branch (`comiss`/`jae`, NaN to the
  negative branch in both). Also compiled with `-DW3_OMPG -fopenmp` (as `switch_MPI_OMPH` of
  `ww3_tp2.10` and `ww3_tp2.16` do): the outlined `smcxuno2r._omp_fn.0` has the same operations
  and the same `minss` operand order, so the header serves that variant too. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. `ww3_tp2.16` leaves `LvSMC` and `UNO3` at their defaults (1 and
false; `regtests/ww3_tp2.16/input/namelists_A50SMC.nml:1`), so its SMC run reaches this routine
once it is wired ⚠ (read from the inputs, not run, as recorded for `SMCyUNO2r`). `ww3_tp2.10`
(`LvSMC=3`, `UNO3=.TRUE.`) and `mww3_test_09` (`LvSMC=4`) do not.
