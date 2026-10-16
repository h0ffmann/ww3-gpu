# SMCYUNO3_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Mid-flux values on the V-faces (faces normal to y) of a multi-resolution Spherical
Multiple-Cell (SMC) grid, by the third-order UNO3 scheme, at the sub-time-step fraction `FTS`.
For each V-face `j`, with the upstream, central and downstream cells `K, L, M, N = IJKVFc(4:7,j)`:

- `CNST0 = AKDif*FTS*FTS*2.0` (once per call), `CNST2 = FLOAT(IJKCel4(L))`,
  `CNST3 = FLOAT(IJKCel4(M))`, `CNST5 = (CF(M)-CF(L))/(CNST2+CNST3)`,
  `CNST6 = 0.5*(VC(L)+VC(M))*FTS`, `CNST8 = CLATF(j)*FLOAT(IJKVFc(3,j))` (face size times the
  cosine of the face latitude);
- `CNST6 >= 0`: when `M <= 0`, `VFLY(j) = VC(L)*FTS` and `CNST3 = CNST2`, else `VFLY(j) = CNST6`;
  `CNST1 = FLOAT(IJKCel4(K))`, `CNST4 = (CF(L)-CF(K))/(CNST2+CNST1)`, `CNST7 = CNST5-CNST4`,
  `CNST9 = 2/(CNST3+CNST2+CNST2+CNST1)`;
  `CNST = CNST5 - (CNST3+VFLY(j))*CNST7*CNST9/1.5` when `|CNST7| < 0.6*CNST9*|CF(M)-CF(K)|`,
  else `SIGN(2,CNST5)*MIN(|CNST4|,|CNST5|)` when `DBLE(CNST4)*DBLE(CNST5) > 0`,
  else `SIGN(1,CNST5)*MIN(|CNST4|,|CNST5|)`; `FV(j) = (CF(L) + CNST*(CNST2-VFLY(j)))*CNST8`;
- otherwise the mirror image about the face, with `N`, and when `L <= 0` `VFLY(j) = VC(M)*FTS`
  and `CNST2 = CNST3`; `CNST4 = (CF(N)-CF(M))/(CNST1+CNST3)`, `CNST7 = CNST4-CNST5`,
  `CNST9 = 2/(CNST2+CNST3+CNST3+CNST1)`, `CNST = CNST5 + (CNST2-VFLY(j))*CNST7*CNST9/1.5` on
  the third-order side, and `FV(j) = (CF(M) - CNST*(CNST3+VFLY(j)))*CNST8`;
- `FY(j) = CNST0*CNST5*CNST8` (diffusion flux).

Unlike the x-direction sibling [`SMCXUNO3_KOKKOS.md`](SMCXUNO3_KOKKOS.md), the boundary
branches also replace the boundary cell's y-size by the central cell's (`:1983`, `:2019`), and
the diffusion flux carries the cosine in `CNST8` instead of dividing by `CLATS**2`.

## What was ported

| | |
|---|---|
| WW3 routine | `SMCyUNO3`, `model/src/w3psmcmd.F90:1919-2063` at NOAA-EMC/WW3 `03adda91`, author Jian-Guo Li. Called only by `W3PSMC` (`w3psmcmd.F90:777`), in the multi-resolution branch when `FUNO3` is true (`:776`), once per refinement level and sub-step. The only `#ifdef` is `W3_OMPG` (`:1946-1950`, `:2057-2060`): an OpenMP `DO` over independent faces, no arithmetic change. Module `W3PSMCMD` is compiled under switch `SMC` (`switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, regtests `ww3_tp2.10`, `ww3_tp2.14`, `ww3_tp2.16`, `mww3_test_09`; `git grep -lw SMC -- '*switch*'`), by no NCEP or NWS switch |
| Kokkos code | [`../src/ww_kokkos/psmc_smcyuno3.hpp`](../src/ww_kokkos/psmc_smcyuno3.hpp): `ww::psmc::smcyuno3_point` (one face), `ww::psmc::smcyuno3_cnst0` (`:1942`) and `ww::psmc::smcyuno3` (`j = NVA..NVB`, the Fortran order). Module state `IJKVFc`, `IJKCel4`, `CLATF` (`w3gdatmd`) are arguments, with the layout and bases of the siblings [`SMCYUNO2_KOKKOS.md`](SMCYUNO2_KOKKOS.md) and [`SMCYUNO3R_KOKKOS.md`](SMCYUNO3R_KOKKOS.md). No existing `psmc_*` helper applies |
| Work arrays | none |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3psmcmd.F90` under `model/bin/switch_SMCMlt` (WW3's CMake
Release flags, gfortran 16.2), recompiled and checked identical to the CMake object, 2026-10-16;
the C++ unit compiled warning-free with the openmp-release flags (g++ 16.2). Every arithmetic
instruction of the two objects was compared site by site, including which operand is the first
source.

- (v) Both loops are scalar (no vectorisation in either object).
- (v) `CNST0`: `((AKDif*FTS)*FTS)*2.0`, the `*2.0` emitted as `x+x` in both objects.
- (v) `FLOAT(IJKCel4(.))` and `FLOAT(IJKVFc(3,j))` are one `cvtsi2ss` each in both objects;
  `CNST8` is `FLOAT(IJKVFc(3,j))*CLATF(j)` with the integer-valued factor as first source in both.
- (v) `CNST9`: gfortran starts `:1992` and `:2029` from `CNST2+CNST3` of `:1963` (or from
  `CNST2+CNST2`, `CNST3+CNST3` at the boundary); the header writes the same. The integer-valued
  sums are exact and cannot be NaN, so their operand order is free.
- (v) The third-order test `|CNST7| < (0.6*CNST9)*|CF(.)-CF(.)|` is `comiss`/`jbe` in both (an
  unordered compare takes the UNO2 side); `0.6` is the float `0.6f`.
- (v) `.../1.5` stays a `divss` by `1.5` in both (no reciprocal).
- (v) `Sign(1.0, CNST5)*min(...)` is `.XORSIGN(min, CNST5)` in both; `Sign(2.0, CNST5)` is a
  `copysignf` (`andps`/`orps`) multiplied with the minimum as first source in both.
- (v) `MIN(|CNST4|,|CNST5|)`: in the positive branch both objects keep `|CNST5|` only when
  strictly less (`:2000`, `:2005`); in the negative branch `|CNST4|` only when strictly less
  (`:2037`, `:2042`). Same in the `W3_OMPG` object.
- (v) `DBLE(CNST4)*DBLE(CNST5) > 0`: `cvtss2sd`, `mulsd` with `CNST4` first, `comisd` in both.
- (v) `FY(j)`: `(CNST5*CNST0)*CNST8`, `CNST5` first, in both. g++ put `CNST0` first when the
  product was written after the branches, so the header computes `FY(j)` before them; the store
  order of `FV(j)` and `FY(j)` does not change any value.
- (v) First source of the other float adds, subtracts and multiplies that can see two NaNs
  matches: `VC(L)+VC(M)`, `(.)*0.5*FTS`, `product+CF(L)` in the positive `FV`,
  `(CNST3+VFLY)*CNST` in the negative `FV`, `quotient+CNST5` in the negative third-order `CNST`.
  Parentheses are `PAREN_EXPR` in the Fortran GIMPLE; nothing is reassociated in the C++. No FMA.
- (v) Operand order that differs but cannot change bits: the positive `FV` product is
  `CNST*(CNST2-VFLY)` in the Fortran object and `(CNST2-VFLY)*CNST` in the C++ (writing either
  order gives the same C++ object); there `VFLY` is not NaN (a NaN `CNST6` takes the negative
  branch, and a NaN `FTS` makes `CNST6` NaN), so `CNST2-VFLY` is never NaN. `0.6*CNST9` has the
  constant first in the Fortran, `CNST9` in the C++; `0.6` is never NaN.
- (v) `VFLY(j)` is stored (the Fortran stores `CNST6` first and overwrites it at the boundary)
  and read back for `CNST` and `FV(j)`; the header uses the final value, as the Fortran GIMPLE
  does through a PHI.
- (v) The `CNST6 >= 0` branch is `comiss`/`jae` in both, so a NaN `CNST6` takes the negative branch.
- (v) No reduction, no libm call.
- (v) `W3_OMPG`: `w3psmcmd.F90` recompiled with `-DW3_OMPG -fopenmp`: `CNST0` is computed
  before the parallel region and passed in; the outlined `smcyuno3._omp_fn.0` has the same
  operations (the same counts less the three of `CNST0`), the same `minss` operand order in both
  branches and `CNST5*CNST0` for `FY`. The header serves that variant; the caller chooses the
  face partition.
- ⚠ CUDA not compiled.
- ⚠ `VC(M)*FTS` at the `L <= 0` boundary (`:2018`): the Fortran object has `VC(M)` as first
  source, the C++ object `FTS`, as in `SMCxUNO3`. The product is the same bits except when both
  are NaN (x86 returns the first source's payload). The positive-branch `VC(L)*FTS` differs the
  same way but cannot see two NaNs, since a NaN `VC(L)` makes `CNST6` NaN and takes the negative
  branch.
- (v) Fresh review 2026-10-16: no defect. The module recompiled from `ref2/build_smc`'s flags is
  identical to the CMake object; the C++ unit (no `__restrict`) compiled warning-free; every
  arithmetic instruction, `.rodata` constant (`0.5`, `0.6f`, `1.5`, `2.0`), the boundary
  substitutions `CNST3 = CNST2` (`:1983`) and `CNST2 = CNST3` (`:2019`), `minss` operand order and
  NaN branch were read against the serial object and the `W3_OMPG` outlined body (whose positive
  `FV` product has `CNST2-VFLY` first, which the argument above also covers). Only the
  `VC(M)*FTS` NaN-pair payload above differs. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. `ww3_tp2.10` sets `LvSMC=3` and `UNO3=.TRUE.`
(`regtests/ww3_tp2.10/input/namelists_SMC0512.nml:1`), so its SMC grid reaches this routine once
it is wired ⚠ (read from the inputs, not run). `mww3_test_09` and `ww3_tp2.14` leave `UNO3` at
its default (false) and take `SMCyUNO2`; `ww3_tp2.16` (`LvSMC=1`) the single-resolution routines.
