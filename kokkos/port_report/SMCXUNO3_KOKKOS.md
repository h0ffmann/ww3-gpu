# SMCXUNO3_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Mid-flux values on the U-faces (faces normal to x) of a multi-resolution Spherical
Multiple-Cell (SMC) grid, by the third-order UNO3 scheme, at the sub-time-step fraction `FTS`.
For each U-face `i`, with the upstream, central and downstream cells `K, L, M, N = IJKUFc(4:7,i)`:

- `CNST0 = AKDif*FTS*FTS*2.0` (once per call), `CNST2 = FLOAT(IJKCel3(L))`,
  `CNST3 = FLOAT(IJKCel3(M))`, `CNST5 = (CF(M)-CF(L))/(CNST2+CNST3)`,
  `CNST6 = 0.5*(UC(L)+UC(M))*FTS`, `CNST8 = FLOAT(IJKUFc(3,i))` (face factor), `ij = MAX(L,M)`;
- `CNST6 >= 0`: `UFLX(i) = UC(L)*FTS` when `M <= 0`, else `CNST6`;
  `CNST1 = FLOAT(IJKCel3(K))`, `CNST4 = (CF(L)-CF(K))/(CNST2+CNST1)`, `CNST7 = CNST5-CNST4`,
  `CNST9 = 2/(CNST3+CNST2+CNST2+CNST1)`;
  `CNST = CNST5 - (CNST3+UFLX(i))*CNST7*CNST9/1.5` when `|CNST7| < 0.6*CNST9*|CF(M)-CF(K)|`,
  else `SIGN(2,CNST5)*MIN(|CNST4|,|CNST5|)` when `DBLE(CNST4)*DBLE(CNST5) > 0`,
  else `SIGN(1,CNST5)*MIN(|CNST4|,|CNST5|)`; `FU(i) = (CF(L) + CNST*(CNST2-UFLX(i)))*CNST8`;
- otherwise the mirror image about the face, with `N`, `UC(M)` when `L <= 0`,
  `CNST4 = (CF(N)-CF(M))/(CNST1+CNST3)`, `CNST7 = CNST4-CNST5`,
  `CNST9 = 2/(CNST2+CNST3+CNST3+CNST1)`, `CNST = CNST5 + (CNST2-UFLX(i))*CNST7*CNST9/1.5` on
  the third-order side, and `FU(i) = (CF(M) - CNST*(CNST3+UFLX(i)))*CNST8`;
- `FX(i) = CNST0*CNST5*CNST8/(CLATS(ij)*CLATS(ij))` (diffusion flux).

## What was ported

| | |
|---|---|
| WW3 routine | `SMCxUNO3`, `model/src/w3psmcmd.F90:1764-1900` at NOAA-EMC/WW3 `03adda91`, author Jian-Guo Li. Called only by `W3PSMC` (`w3psmcmd.F90:688`), in the multi-resolution branch when `FUNO3` is true (`:687`), once per refinement level and sub-step. The only `#ifdef` is `W3_OMPG` (`:1794-1797`, `:1894-1897`): an OpenMP `DO` over independent faces, no arithmetic change. Module `W3PSMCMD` is compiled under switch `SMC` (`switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, regtests `ww3_tp2.10`, `ww3_tp2.14`, `ww3_tp2.16`, `mww3_test_09`; `git grep -lw SMC -- '*switch*'`), by no NCEP or NWS switch |
| Kokkos code | [`../src/ww_kokkos/psmc_smcxuno3.hpp`](../src/ww_kokkos/psmc_smcxuno3.hpp): `ww::psmc::smcxuno3_point` (one face), `ww::psmc::smcxuno3_cnst0` (`:1787`) and `ww::psmc::smcxuno3` (`i = NUA..NUB`, the Fortran order). Module state `IJKUFc`, `IJKCel3`, `CLATS` (`w3gdatmd`) are arguments, with the layout and bases of the siblings [`SMCXUNO2_KOKKOS.md`](SMCXUNO2_KOKKOS.md) and [`SMCXUNO3R_KOKKOS.md`](SMCXUNO3R_KOKKOS.md). No existing `psmc_*` helper applies |
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
- (v) `FLOAT(IJKCel3(.))` and `FLOAT(IJKUFc(3,i))` are one `cvtsi2ss` each in both objects.
- (v) `CNST9`: gfortran reuses `CNST2+CNST3` of `:1810` as the first partial sum of `:1837` and
  `:1868`; the header writes the same reuse. The integer-valued sums are exact and cannot be NaN,
  so their operand order is free.
- (v) The third-order test `|CNST7| < (0.6*CNST9)*|CF(.)-CF(.)|` is `comiss`/`jbe` in both (an
  unordered compare takes the UNO2 side); `0.6` is the float `0.6f`.
- (v) `.../1.5` stays a `divss` by `1.5` in both (no reciprocal).
- (v) `Sign(1.0, CNST5)*min(...)` is `.XORSIGN(min, CNST5)` in both; `Sign(2.0, CNST5)` is a
  `copysignf` (`andps`/`orps`) multiplied with the minimum as first source in both.
- (v) `MIN(|CNST4|,|CNST5|)`: in the positive branch both objects keep `|CNST5|` only when
  strictly less (`minss` with `|CNST5|` as first source, `:1845`, `:1849`); in the negative branch
  `|CNST4|` only when strictly less (`:1876`, `:1880`). The header writes each site as its object.
- (v) `DBLE(CNST4)*DBLE(CNST5) > 0`: `cvtss2sd`, `mulsd` with `CNST4` first, `comisd` in both.
- (v) `MAX(L, M)` is an integer `MAX_EXPR` (`cmovge`) in both.
- (v) First source of every float add, subtract and multiply that can see two NaNs matches:
  `UC(L)+UC(M)`, `(.)*0.5*FTS`, `CNST5*CNST0` in `FX`, the bracket before `CNST` in `FU`,
  `product+CF(L)` in the positive `FU`, `quotient+CNST5` in the negative third-order `CNST`. The
  header writes these in the object's order. Parentheses are `PAREN_EXPR` in the Fortran GIMPLE;
  nothing is reassociated in the C++. No FMA.
- (v) `UFLX(i)` is stored and read back for `CNST` and `FU(i)`: the Fortran GIMPLE uses the
  stored value through a PHI (`prephitmp`), the header the same local value.
- (v) The `CNST6 >= 0` branch is `comiss`/`jae` in both, so a NaN `CNST6` takes the negative branch.
- (v) No reduction, no libm call.
- (v) `W3_OMPG`: `w3psmcmd.F90` recompiled with `-DW3_OMPG -fopenmp`: `CNST0` is computed
  before the parallel region and passed in (`.omp_data_o.cnst0`); the outlined
  `smcxuno3._omp_fn.0` has the same operations and the same `minss` operand order in both
  branches. The header serves that variant; the caller chooses the face partition.
- ⚠ CUDA not compiled.
- ⚠ `UC(M)*FTS` at the `L <= 0` boundary: the Fortran object has `UC(M)` as first source, the
  C++ object `FTS`, and writing `FTS*UC(M)` does not change g++'s choice. The product is the same
  bits except when both are NaN (x86 returns the first source's payload). The positive-branch
  `UC(L)*FTS` differs the same way but cannot see two NaNs, since a NaN `UC(L)` makes `CNST6`
  NaN and takes the negative branch.
- (v) Fresh review 2026-10-16: no defect. The module recompiled from `ref2/build_smc`'s flags is
  identical to the CMake object; the C++ unit (no `__restrict`) compiled warning-free; every
  arithmetic instruction, `.rodata` constant (`0.5`, `0.6f`, `1.5`, `2.0`), `minss` operand order
  and NaN branch was read against the serial object and the `W3_OMPG` outlined body. Only the
  `UC(M)*FTS` NaN-pair payload above differs. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. `ww3_tp2.10` sets `LvSMC=3` and `UNO3=.TRUE.`
(`regtests/ww3_tp2.10/input/namelists_SMC0512.nml:1`), so its SMC grid reaches this routine once
it is wired ⚠ (read from the inputs, not run). `mww3_test_09` and `ww3_tp2.14` leave `UNO3` at
its default (false) and take `SMCxUNO2`; `ww3_tp2.16` (`LvSMC=1`) the single-resolution routines.
