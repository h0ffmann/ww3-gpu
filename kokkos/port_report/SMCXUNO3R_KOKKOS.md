# SMCXUNO3R_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Mid-flux values on the U-faces (faces normal to x) of a single-resolution Spherical
Multiple-Cell (SMC) grid, by the third-order UNO3 scheme. For each U-face `i`, with the
upstream, central and downstream cells `K, L, M, N = IJKUFc(4:7,i)`:

- `CNST2 = FLOAT(IJKCel3(L))`, `CNST3 = FLOAT(IJKCel3(M))` (cell x-sizes),
  `CNST5 = CF(M)-CF(L)`, `CNST6 = 0.5*(UC(L)+UC(M))`, `CNST0 = 2/(CLATS(ij)*CLATS(ij))` with
  `ij = MAX(L,M)`;
- `CNST6 >= 0`: `UFLX(i) = UC(L)` when `M <= 0`, else `CNST6`; `CNST4 = CF(L)-CF(K)`,
  `CNST8 = CF(M)-CF(K)`, `CNST9 = CNST5-CNST4`; when `|CNST9| <= 0.6*|CNST8|`,
  `CNST = 0.5*CNST5 - (1+UFLX(i)/CNST2)*CNST9/6`, else
  `CNST = SIGN(1 or 0.5, CNST5)*MIN(|CNST4|,|CNST5|)` (1 when `DBLE(CNST4)*DBLE(CNST5) > 0`);
  `FU(i) = CF(L) + CNST*(1-UFLX(i)/CNST2)`;
- otherwise: `UFLX(i) = UC(M)` when `L <= 0`, else `CNST6`; `CNST4 = CF(N)-CF(M)`,
  `CNST8 = CF(N)-CF(L)`, `CNST9 = CNST4-CNST5`; the same limiter with
  `CNST = 0.5*CNST5 + (1-UFLX(i)/CNST3)*CNST9/6`; `FU(i) = CF(M) - CNST*(1+UFLX(i)/CNST3)`;
- `FX(i) = AKDif*CNST0*CNST5/(CNST2+CNST3)` (diffusion flux).

## What was ported

| | |
|---|---|
| WW3 routine | `SMCxUNO3r`, `model/src/w3psmcmd.F90:2080-2194` at NOAA-EMC/WW3 `03adda91`, author Jian-Guo Li. Called only by `W3PSMC` (`w3psmcmd.F90:490`), when `NRLv == 1` (`:487`, single resolution; `NRLv = LvSMC`, `w3gridmd.F90:2687`) and `FUNO3` is true (`:488`; `FUNO3 = UNO3`, `w3gridmd.F90:2674`). The only `#ifdef` is `W3_OMPG` (`:2102-2106`, `:2186-2189`): an OpenMP `DO` over independent faces, no arithmetic change. Module `W3PSMCMD` is compiled under switch `SMC` (`switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, regtests `ww3_tp2.10`, `ww3_tp2.14`, `ww3_tp2.16`, `mww3_test_09`; `git grep -lw SMC -- '*switch*'`), by no NCEP or NWS switch |
| Kokkos code | [`../src/ww_kokkos/psmc_smcxuno3r.hpp`](../src/ww_kokkos/psmc_smcxuno3r.hpp): `ww::psmc::smcxuno3r_point` (one face) and `ww::psmc::smcxuno3r` (`i = NUA..NUB`, the Fortran order). Module state `IJKUFc`, `IJKCel3`, `CLATS` (`w3gdatmd`) are arguments, with the same bases as in [`SMCXUNO2R_KOKKOS.md`](SMCXUNO2R_KOKKOS.md) |
| Work arrays | none |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3psmcmd.F90` under `model/bin/switch_SMCMlt` (WW3's CMake
Release flags, gfortran 16.2), recompiled and checked identical to the CMake object, 2026-10-16;
the C++ unit compiled warning-free with the openmp-release flags (g++ 16.2). Compared by reading
the two assemblies operation by operation.

- (v) The Fortran loop is scalar (not vectorised); the C++ object is scalar too.
- (v) `FLOAT(IJKCel3(.))` is one `cvtsi2ss` in both objects; `MAX(L, M)` is `cmovge` in both.
- (v) Limiter test `ABS(CNST9) <= 0.6*ABS(CNST8)`: `comiss` of `|CNST9|` against
  `|CNST8|*0.6`, `jb` to the second-order branch, in both objects, so a NaN takes the
  `MIN` branch in both.
- (v) `MIN(ABS(CNST4), ABS(CNST5))`, two operand orders, the same as `SMCyUNO3r`: in the
  positive branch (`:2146`, `:2149`) the Fortran object has `minss |CNST4|,|CNST5|` (keeps
  `|CNST5|` only when strictly less), in the negative branch (`:2170`, `:2173`)
  `minss |CNST5|,|CNST4|`; the header writes `a5 < a4 ? a5 : a4` and `a4 < a5 ? a4 : a5`, and the
  C++ object has `vminss` with the same destinations.
- (v) `DBLE(CNST4)*DBLE(CNST5) .GT. 0.d0`: two `cvtss2sd`, `mulsd`, `comisd`/`jbe` in both
  (the product of two floats is exact in double, so the operand order does not matter).
- (v) `SIGN(1.0, CNST5)*MIN(...)` is an `xor` of the sign bit of `CNST5` in both objects;
  `SIGN(0.5, CNST5)*MIN(...)` is a mask/`or` then `mulss` in both.
- (v) Operation order, identical in both objects up to commutation of a product or a sum:
  `(UC(L)+UC(M))*0.5`; `2/(CLATS*CLATS)`; `0.5*CNST5 - ((1+UFLX/CNST2)*CNST9)/6` and
  `0.5*CNST5 + ((1-UFLX/CNST3)*CNST9)/6` (`divss` by 6, not a multiply);
  `CF(L) + CNST*(1-UFLX/CNST2)`; `CF(M) - CNST*(1+UFLX/CNST3)`;
  `((AKDif*CNST0)*CNST5)/(CNST2+CNST3)`. No FMA.
- (v) `UFLX(i)` is stored and the same value reused for `FU(i)` in both objects.
- (v) `K` is loaded only in the positive branch and `N` only in the negative one, as in the object.
- (v) `W3_OMPG`: `w3psmcmd.F90` recompiled with `-DW3_OMPG -fopenmp` (as `switch_MPI_OMPH` of
  `ww3_tp2.10` and `ww3_tp2.16` do); the outlined `smcxuno3r._omp_fn.0` has the same float
  operations (counts of `addss`, `subss`, `mulss`, `divss`, `minss`, `mulsd`, `comiss`, `comisd`
  equal to the serial object) and the same `minss` destinations in both branches, so the header
  serves that variant too.
- (v) No reduction, no libm call.
- ⚠ One product has its operands the other way round in the C++ object: `AKDif*CNST0` is
  `mulss` with `CNST0` as first operand in the Fortran object (serial and `W3_OMPG`) and
  `vmulss` with `AKDif` first in the C++ object. x86 returns the first operand's NaN, so `FX(i)`
  can differ only in the payload or sign of a NaN when `AKDif` and `CLATS(ij)` are both NaN.
  Writing `cnst0 * akdif` in the source did not change g++'s order. Every other product and sum
  has the same first operand in both objects.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no value defect. Recompiled `w3psmcmd.F90` from `build_smc`'s
  `flags.make` (identical to the CMake object) and with `-DW3_OMPG -fopenmp`, and the header in a
  throwaway openmp-release unit (warning-free); compared GIMPLE and assembly: float operation
  counts equal, `0.6` folded the same, `/6.0` a `divss` in both, `minss` destinations per branch
  equal in the serial, `W3_OMPG` and C++ objects, the `DBLE` product, `XORSIGN`/`copysign`
  limiter, the `UFLX` read-back, the first operand of every product and sum. Added the
  `AKDif*CNST0` NaN-pair ⚠ above; the header is unchanged.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest reaches this routine: it needs `LvSMC = 1` with
`UNO3 = .TRUE.`, and the only regtest that sets `UNO3` (`ww3_tp2.10`,
`regtests/ww3_tp2.10/input/namelists_SMC0512.nml:1`) has `LvSMC=3`, which takes the
multi-resolution path (`w3psmcmd.F90:663`) ⚠ (read from the inputs, not run).
