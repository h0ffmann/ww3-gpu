# SMCYUNO2_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Mid-flux values on the V-faces (faces normal to y) of a multi-resolution Spherical
Multiple-Cell (SMC) grid, by the second-order UNO2 scheme. For each V-face `j`, with the
upstream, central and downstream cells `K, L, M, N = IJKVFc(4:7,j)` and the cell y-sizes
`CNST1, CNST2, CNST3 = FLOAT(IJKCel4(K or N, L, M))`:

- `CNST0 = AKDif*FTS*FTS*2.0`; `CNST5 = (CF(M)-CF(L))/(CNST2+CNST3)`;
  `CNST6 = 0.5*(VC(L)+VC(M))*FTS`; `CNST8 = CLATF(j)*FLOAT(IJKVFc(3,j))`;
- `CNST6 >= 0`: `VFLY(j) = VC(L)*FTS` when `M <= 0`, else `CNST6`;
  `CNST4 = (CF(L)-CF(K))/(CNST2+CNST1)`;
  `FV(j) = (CF(L) + SIGN(1.0,CNST5)*MIN(|CNST4|,|CNST5|)*(CNST2-VFLY(j)))*CNST8`;
- otherwise: `VFLY(j) = VC(M)*FTS` when `L <= 0`, else `CNST6`;
  `CNST4 = (CF(N)-CF(M))/(CNST1+CNST3)`;
  `FV(j) = (CF(M) - SIGN(1.0,CNST5)*MIN(|CNST4|,|CNST5|)*(CNST3+VFLY(j)))*CNST8`;
- `FY(j) = CNST0*CNST5*CNST8` (diffusion flux).

## What was ported

| | |
|---|---|
| WW3 routine | `SMCyUNO2`, `model/src/w3psmcmd.F90:1424-1534` at NOAA-EMC/WW3 `03adda91`, author Jian-Guo Li. Called only by `W3PSMC` (`w3psmcmd.F90:780`), in the multi-resolution branch (`NRLv > 1`, the `ELSE` of `:487`; `LvSMC` in the `PSMC` namelist) when `FUNO3` is false (`:776`), once per refinement level with `FTS = FMR`. The only `#ifdef` is `W3_OMPG` (`:1449-1453`, `:1528-1531`): an OpenMP `DO` over independent faces. Module `W3PSMCMD` is compiled under switch `SMC` (`switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, regtests `ww3_tp2.10`, `ww3_tp2.14`, `ww3_tp2.16`, `mww3_test_09`; `git grep -lw SMC -- '*switch*'`), by no NCEP or NWS switch |
| Kokkos code | [`../src/ww_kokkos/psmc_smcyuno2.hpp`](../src/ww_kokkos/psmc_smcyuno2.hpp): `ww::psmc::smcyuno2_point` (one face) and `ww::psmc::smcyuno2` (`j = NVA..NVB`, the Fortran order). Module state `IJKVFc`, `CLATF`, `IJKCel4` (`w3gdatmd`) are arguments, taken as contiguous (they point at `GRIDS(IMOD)` arrays allocated `IJKVFc(7,MVFc)`, `CLATF(MVFc)`, `IJKCel4(-9:MCel)`, `w3gdatmd.F90:1772`, `:1775`, `:1780`). `CNST0`, loop invariant in the Fortran (`:1445`), is computed in the point function from `AKDif` and `FTS` with the same operations, so every face gets the same bits |
| Work arrays | none |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3psmcmd.F90` under `model/bin/switch_SMCMlt` (WW3's CMake
Release flags, gfortran 16.2), recompiled from `build_smc`'s `flags.make` and checked identical
to the CMake object, 2026-10-16; a second object with `-DW3_OMPG -fopenmp` for the OpenMP
variant; the C++ unit compiled warning-free with the openmp-release flags (g++ 16.2).

- (v) The Fortran loop is scalar (not vectorised); the C++ object is scalar too. No FMA in
  either.
- (v) `SIGN(1.0, CNST5)*MIN(...)` is `.XORSIGN(min, CNST5)` in both GIMPLEs, the same
  `andps`/`xorps` sign-mask sequence in both objects.
- (v) `MIN(ABS(CNST4), ABS(CNST5))`: in both branches, plain and `W3_OMPG`, the Fortran object
  keeps `|CNST4|` only when strictly less (`minss |CNST5|,|CNST4|`); the header writes
  `a4 < a5 ? a4 : a5` and the C++ object has `vminss` with the same operand order.
- (v) Operation order, identical in both GIMPLEs: `CNST0 = ((AKDif*FTS)*FTS)*2.0` (an `addss`
  of itself in both objects); `(CF(M)-CF(L))/(CNST2+CNST3)`; `((VC(L)+VC(M))*0.5)*FTS`;
  `CLATF*cvtsi2ss(IJKVFc(3,j))`; positive branch `(CF(L)-CF(K))/(CNST2+CNST1)`,
  `CF(L) + CNST*(CNST2-VFLY)`, `*CNST8`; negative branch `(CF(N)-CF(M))/(CNST1+CNST3)`,
  `CF(M) - CNST*(CNST3+VFLY)`, `*CNST8`; `(CNST0*CNST5)*CNST8`. The Fortran parentheses are
  `PAREN_EXPR`s; nothing is reassociated in the C++. `CNST6 >= 0` is `comiss`/`jae` in both, so
  a NaN `CNST6` takes the negative branch in both.
- (v) The Fortran's `CNST3 = CNST2` (`:1486`) and `CNST2 = CNST3` (`:1506`) are dead: the
  GIMPLE keeps them only as debug binds. The header omits them, with a comment.
- (v) `VFLY(j)` is stored as `CNST6`, overwritten at the boundary, then read back for `FV(j)`;
  the Fortran GIMPLE uses the stored value (a `prephitmp` PHI), the header the same local value.
- (v) The `W3_OMPG` variant: the loop body is outlined into `smcyuno2._omp_fn.0` with the same
  operations and order (GIMPLE and object read); `CNST0` is computed before the parallel region.
  The faces are independent, so splitting `j` over threads gives the same bits.
- ⚠ Which operand of a commutative `mulss` is the first source is chosen by register allocation
  and differs from the plain Fortran object at five sites: `VC(L)*FTS` and `VC(M)*FTS` at the
  boundary (Fortran `VC` first, C++ `FTS` first), `CNST*(CNST2-VFLY)` and `CNST*(CNST3+VFLY)`
  (Fortran `CNST` first, C++ the bracket first), `CNST0*CNST5` (Fortran `CNST5` first).
  The product is the same bits except when both operands are NaN, where x86 returns the first
  source's payload and sign. The Fortran itself does not keep one choice: its plain and `W3_OMPG`
  objects put the operands of `CNST*(CNST3+VFLY)` in opposite orders. Writing the C++ operands in
  either order gives the same C++ object.
- (v) No reduction, no libm call.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3psmcmd.F90` from `build_smc`'s
  `flags.make` (identical to the CMake object) and with `-DW3_OMPG -fopenmp`, and the header in a
  throwaway openmp-release unit; compared GIMPLE and assembly operation by operation (`minss`
  order in both branches and both variants, `CNST0*2` as `addss`, the dead `CNST3`/`CNST2`
  stores, `CLATF*FLOAT` order, `VFLY` read-back). Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. `mww3_test_09` (`LvSMC=4`, `UNO3` at its default false,
`regtests/mww3_test_09/input/namelists_Huron.nml:1`) and the OASIS-coupled `ww3_tp2.14`
(`LvSMC=2`, `regtests/ww3_tp2.14/input_oasacm6/namelists_BREST.nml:1`) reach this routine once
it is wired ⚠ (read from the inputs, not run). `ww3_tp2.10` (`LvSMC=3`, `UNO3=.TRUE.`) and
`ww3_tp2.16` (`LvSMC` default 1) do not.
