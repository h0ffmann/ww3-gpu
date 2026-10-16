# SMCYUNO2R_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Mid-flux values on the V-faces (faces normal to y) of a single-resolution Spherical
Multiple-Cell (SMC) grid, by the second-order UNO2 scheme. For each V-face `j`, with the
upstream, central and downstream cells `K, L, M, N = IJKVFc(4:7,j)`:

- `CNST5 = CF(M)-CF(L)`, `CNST6 = 0.5*(VC(L)+VC(M))`, `CNST8 = CLATF(j)*FLOAT(IJKVFc(3,j))`;
- `CNST6 >= 0`: `VFLY(j) = VC(L)` when `M <= 0`, else `CNST6`;
  `FV(j) = (CF(L) + SIGN(0.5,CNST5)*MIN(|CF(L)-CF(K)|,|CNST5|)*(1-VFLY(j)))*CNST8`;
- otherwise: `VFLY(j) = VC(M)` when `L <= 0`, else `CNST6`;
  `FV(j) = (CF(M) - SIGN(0.5,CNST5)*MIN(|CF(N)-CF(M)|,|CNST5|)*(1+VFLY(j)))*CNST8`;
- `FY(j) = AKDif*CNST5*CNST8` (diffusion flux).

## What was ported

| | |
|---|---|
| WW3 routine | `SMCyUNO2r`, `model/src/w3psmcmd.F90:1661-1746` at NOAA-EMC/WW3 `03adda91`, author Jian-Guo Li. Called only by `W3PSMC` (`w3psmcmd.F90:586`), when `NRLv == 1` (`:487`, single resolution, `LvSMC` in the `PSMC` namelist, default 1, `w3gridmd.F90:2619`) and `FUNO3` is false (`:581`). The only `#ifdef` is `W3_OMPG` (`:1678-1682`, `:1740-1743`): an OpenMP `DO` over independent faces, no arithmetic change. Module `W3PSMCMD` is compiled under switch `SMC` (`switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, regtests `ww3_tp2.10`, `ww3_tp2.14`, `ww3_tp2.16`, `mww3_test_09`; `git grep -lw SMC -- '*switch*'`), by no NCEP or NWS switch |
| Kokkos code | [`../src/ww_kokkos/psmc_smcyuno2r.hpp`](../src/ww_kokkos/psmc_smcyuno2r.hpp): `ww::psmc::smcyuno2r_point` (one face) and `ww::psmc::smcyuno2r` (`j = NVA..NVB`, the Fortran order). Module state `IJKVFc`, `CLATF` (`w3gdatmd`) are arguments, taken as contiguous (they point at `GRIDS(IMOD)` arrays allocated `IJKVFc(7,MVFc)`, `CLATF(MVFc)`, `w3gdatmd.F90:1772`, `:1775`) |
| Work arrays | none |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3psmcmd.F90` under `model/bin/switch_SMCMlt` (WW3's CMake
Release flags, gfortran 16.2), recompiled and checked identical to the CMake object, 2026-10-16;
the C++ unit compiled warning-free with the openmp-release flags (g++ 16.2).

- (v) The Fortran loop is scalar (not vectorised); the C++ object is scalar too.
- (v) `MIN(ABS(CNST4), ABS(CNST5))`: in both branches the Fortran object keeps `|CNST4|` only
  when strictly less (`minss %xmm12(|CNST5|),%xmm1(|CNST4|)`); the header writes
  `a4 < a5 ? a4 : a5` and the C++ object has `vminss` with the same operand order.
- (v) `SIGN(0.5, CNST5)` is `copysignf` (and/andn/or masks) in both objects.
- (v) Operation order, identical in both objects: `(VC(L)+VC(M))*0.5`; `CF(M)-CF(L)`;
  `CLATF*cvtsi2ss(IJKVFc(3,j))`; `(sign*min)*(1-VFLY)`, `+CF(L)`, `*CNST8` (positive);
  `(sign*min)*(1+VFLY)`, `CF(M)-`, `*CNST8` (negative); `(AKDif*CNST5)*CNST8`. The
  parentheses are `PAREN_EXPR` in the Fortran GIMPLE; nothing is reassociated in the C++. No FMA.
- (v) `VFLY(j)` is stored and then read back for `FV(j)`; the Fortran GIMPLE uses the stored
  value (`cstore` PHI), the header uses the same local value.
- (v) `K` is loaded only in the positive branch and `N` only in the negative one, as in the object.
- (v) No reduction.
- ⚠ CUDA not compiled.
- ⚠ The `W3_OMPG` variant (`switch_MPI_OMPH` of `ww3_tp2.10` and `ww3_tp2.16`) was not compiled
  or compared: its `!$OMP DO` outlines the loop body, and the reasoning that the per-face
  arithmetic is unchanged was not read in an object.
- (v) Fresh review 2026-10-16: no defect. `w3psmcmd.F90` recompiled from `build_smc`'s
  `flags.make` (identical to the CMake object) and a C++ unit of the header compiled with the
  openmp-release flags; the two loop bodies compared instruction by instruction: loads of
  `K`/`N` per branch, `CNST6 >= 0` by `comiss`/`jae` (NaN takes the negative branch in both),
  `M <= 0` / `L <= 0` selects, `copysign` masks, `minss`/`vminss` operand order (`|CNST4|` kept
  only when strictly less), the order of every add, subtract and multiply, `cvtsi2ss` of
  `IJKVFc(3,j)`; no FMA, no vectorisation. Nothing changed in the header; this ⚠ on `W3_OMPG`
  was added.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. `ww3_tp2.16` leaves `LvSMC` and `UNO3` at their defaults (1 and
false; `regtests/ww3_tp2.16/input/namelists_A50SMC.nml:1`), so its SMC run reaches this routine
once it is wired ⚠ (read from the inputs, not run). `ww3_tp2.10` (`LvSMC=3`, `UNO3=.TRUE.`) and
`mww3_test_09` (`LvSMC=4`) do not.
