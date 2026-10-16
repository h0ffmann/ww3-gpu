# SMCXUNO2_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Mid-flux values on the U-faces (faces normal to x) of a multi-resolution Spherical
Multiple-Cell (SMC) grid, by the second-order UNO2 scheme, at the sub-time-step fraction `FTS`.
For each U-face `i`, with the upstream, central and downstream cells `K, L, M, N = IJKUFc(4:7,i)`:

- `CNST0 = AKDif*FTS*FTS` (once per call), `CNST2 = FLOAT(IJKCel3(L))`,
  `CNST3 = FLOAT(IJKCel3(M))`, `CNST5 = (CF(M)-CF(L))/(CNST2+CNST3)`,
  `CNST6 = 0.5*(UC(L)+UC(M))*FTS`, `CNST8 = FLOAT(IJKUFc(3,i))` (face factor),
  `CNST9 = 2/(CLATS(ij)*CLATS(ij))` with `ij = MAX(L,M)`;
- `CNST6 >= 0`: `UFLX(i) = UC(L)*FTS` when `M <= 0`, else `CNST6`;
  `CNST4 = (CF(L)-CF(K))/(CNST2+FLOAT(IJKCel3(K)))`;
  `FU(i) = (CF(L) + SIGN(1,CNST5)*MIN(|CNST4|,|CNST5|)*(CNST2-UFLX(i)))*CNST8`;
- otherwise: `UFLX(i) = UC(M)*FTS` when `L <= 0`, else `CNST6`;
  `CNST4 = (CF(N)-CF(M))/(FLOAT(IJKCel3(N))+CNST3)`;
  `FU(i) = (CF(M) - SIGN(1,CNST5)*MIN(|CNST4|,|CNST5|)*(CNST3+UFLX(i)))*CNST8`;
- `FX(i) = CNST0*CNST5*CNST8*CNST9` (diffusion flux).

## What was ported

| | |
|---|---|
| WW3 routine | `SMCxUNO2`, `model/src/w3psmcmd.F90:1297-1406` at NOAA-EMC/WW3 `03adda91`, author Jian-Guo Li. Called only by `W3PSMC` (`w3psmcmd.F90:691`), in the multi-resolution branch (`NRLv > 1`, `:487`, `:660-669`) when `FUNO3` is false (`:687`), once per refinement level and sub-step. The only `#ifdef` is `W3_OMPG` (`:1321-1332`, `:1400-1403`): an OpenMP `DO` over independent faces, no arithmetic change. Module `W3PSMCMD` is compiled under switch `SMC` (`switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, regtests `ww3_tp2.10`, `ww3_tp2.14`, `ww3_tp2.16`, `mww3_test_09`; `git grep -lw SMC -- '*switch*'`), by no NCEP or NWS switch |
| Kokkos code | [`../src/ww_kokkos/psmc_smcxuno2.hpp`](../src/ww_kokkos/psmc_smcxuno2.hpp): `ww::psmc::smcxuno2_point` (one face), `ww::psmc::smcxuno2_cnst0` (`:1317`) and `ww::psmc::smcxuno2` (`i = NUA..NUB`, the Fortran order). Module state `IJKUFc`, `IJKCel3`, `CLATS` (`w3gdatmd`) are arguments, with the layout and bases of the sibling [`SMCXUNO2R_KOKKOS.md`](SMCXUNO2R_KOKKOS.md). No existing `psmc_*` helper applies (the sibling has no shared function) |
| Work arrays | none |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3psmcmd.F90` under `model/bin/switch_SMCMlt` (WW3's CMake
Release flags, gfortran 16.2), recompiled and checked identical to the CMake object, 2026-10-16;
the C++ unit compiled warning-free with the openmp-release flags (g++ 16.2).

- (v) Both loops are scalar (no vectorisation in either object).
- (v) `FLOAT(IJKCel3(.))` and `FLOAT(IJKUFc(3,i))` are one `cvtsi2ss` each in both objects.
- (v) `Sign(1.0, CNST5)*min(...)` is folded to `.XORSIGN(min, CNST5)` (`andps` sign mask,
  `xorps`) in the Fortran GIMPLE; the header's `copysign(1,CNST5)*min` folds to the same
  `.XORSIGN` in the C++ GIMPLE and object.
- (v) `MIN(|CNST4|,|CNST5|)`: in both branches the Fortran object keeps `|CNST4|` only when
  strictly less (`minss |CNST5|,|CNST4|`); the header writes `a4 < a5 ? a4 : a5` and the C++
  object has `vminss` with `|CNST4|` as first operand.
- (v) `MAX(L, M)` is an integer `MAX_EXPR` (`cmovge`) in both.
- (v) Operation order, identical in both GIMPLE dumps up to commutation of products and sums:
  `CNST0 = FTS*(AKDif*FTS)`; `CNST5 = (CF(M)-CF(L))/(CNST2+CNST3)`; `CNST6 = FTS*((UC(L)+UC(M))*0.5)`;
  `CNST9 = 2/(CLATS*CLATS)`; `CNST4` with its own division; `FU = ((CNST2-UFLX)*CNST + CF(L))*CNST8`
  and `((CNST3+UFLX)*CNST` subtracted from `CF(M))*CNST8`; `FX = ((CNST0*CNST5)*CNST8)*CNST9`.
  The parentheses are `PAREN_EXPR` in the Fortran GIMPLE; nothing is reassociated in the C++.
  No FMA.
- (v) `UFLX(i)` is stored and read back for `FU(i)`: the Fortran GIMPLE uses the stored value
  through a PHI (`prephitmp`), the header the same local value.
- (v) The `CNST6 >= 0` branch is `comiss`/`jae` in both, so a NaN `CNST6` takes the negative branch.
- (v) No reduction, no libm call.
- (v) `W3_OMPG`: `w3psmcmd.F90` recompiled with `-DW3_OMPG -fopenmp` (as `switch_MPI_OMPH` of
  `ww3_tp2.10` and `ww3_tp2.16`): `CNST0` is computed before the parallel region and passed in;
  the outlined `smcxuno2._omp_fn.0` has the same GIMPLE operations and the same `minss` operand
  order in both branches. The header serves that variant; the caller chooses the face partition.
- ⚠ CUDA not compiled.
- ⚠ Which operand of a commutative `mulss` is the first source differs between the objects at
  three reachable sites: `UC(M)*FTS` at the `L <= 0` boundary (Fortran `UC` first, C++ `FTS`
  first), `CNST*(CNST3+UFLX)` (Fortran `CNST` first, C++ the bracket first) and `CNST0*CNST5`
  (Fortran `CNST5` first, C++ `CNST0` first). The product is the same bits except when both
  operands are NaN, where x86 returns the first source's payload and sign. The positive-branch
  pairs (`UC(L)*FTS`, `CNST*(CNST2-UFLX)`) cannot both be NaN there, since a NaN velocity makes
  `CNST6` NaN and takes the negative branch.
- (v) Fresh review 2026-10-16: no defect in the header. Recompiled `w3psmcmd.F90` from
  `build_smc`'s `flags.make` (identical to the CMake object) and with `-DW3_OMPG -fopenmp`, and the
  header in a throwaway openmp-release unit; compared GIMPLE and assembly operation by operation:
  `minss` operand order in both branches (plain and `W3_OMPG`), `.XORSIGN`, `PAREN_EXPR` order,
  `comiss`/`jae` on `CNST6`, the `UFLX` read-back. Added the NaN-pair ⚠ above and `ww3_tp2.14` below.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. `mww3_test_09` sets `LvSMC=4` and leaves `UNO3` at its default
(false; `regtests/mww3_test_09/input/namelists_Michi.nml:1`, `w3gridmd.F90:2627`), so its SMC
grids reach this routine once it is wired ⚠ (read from the inputs, not run), as do those of the
OASIS-coupled `ww3_tp2.14` (`LvSMC=2`, `UNO3` not set, `regtests/ww3_tp2.14/input_oasacm6/namelists_BREST.nml:1`) ⚠. `ww3_tp2.10`
(`LvSMC=3`, `UNO3=.TRUE.`) takes `SMCxUNO3` instead, and `ww3_tp2.16` (`LvSMC=1`) the
single-resolution `SMCxUNO2r`.
