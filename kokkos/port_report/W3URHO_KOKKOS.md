# W3URHO_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the `W3WAVE` wiring first.

Air density at the model time by linear interpolation between the two input fields
(`RHOAIR = RA0 + RD*RAI`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3URHO`, `model/src/w3updtmd.F90:2626-2791` at NOAA-EMC/WW3 `03adda91` |
| Kokkos code | [`../src/ww_kokkos/updt_w3urho.hpp`](../src/ww_kokkos/updt_w3urho.hpp): `w3urho_rd`, `w3urho_first_point`, `w3urho_point` and the launcher `w3urho`, header-only; reuses `w3uwnd_rd` and `w3uwnd_interp` of [`updt_uwnd.hpp`](../src/ww_kokkos/updt_uwnd.hpp) |
| C shim | none yet |
| Caller | `W3WAVE`, when `FLRHOA` (`w3wavemd.F90:1277-1278`) |
| Regtests that reach it | `ww3_tp2.15` with `-i input_rho` (`work_PR3_UQ_RHO`, `work_ST4FLX5`, `work_ST6FLX5`, `work_PR3_UQ_RHO_MPI`; `matrix.base:724-726,817`), all `WNT1`; `ww3_tp2.14` `input_oasacm6` (`SMC WNT0 OASACM`) reads air density in `ww3_prnc.nml` ⚠ (not traced to a `W3URHO` call) |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of `w3updtmd.F90` (WW3's CMake Release flags, gfortran 16.2,
rebuilt byte-identical to the CMake builds of `switch_lab_shrd` and `switch_SMCMlt` inside the
pinned `ww3` shell), read as GIMPLE and assembly, 2026-10-16.

- (v) `RAI = RHN - RH0` is one `subss`; `RA0 = RH0` a copy.
- (v) `RD = DT0T / MAX(1.E-7,DT0N)` is `maxss 1.E-7,DT0N` then `divss`: a NaN `DT0N` gives
  `1.E-7`, as `w3uwnd_rd` computes it. The constant has the same bits
  (`1.0000000116860974230803549289703369140625e-7`).
- (v) `RHOAIR = RA0 + RD*RAI` is `mulss` then `addss` with the product as first operand, in
  the C++ too. ⚠ In the Fortran's `mulss` `RAI` is the first operand, in the g++ object `RD`,
  however the C++ is written: the bits differ only when both are NaN (x86 keeps the first
  operand's payload).
- (v) Neither loop of `:2626-2791` is vectorised in the Fortran object; no FMA, no libm call.
  The C++ loop may be vectorised; it is elementwise with the same operations, so the bits are
  the same.
- (v) `SMC`: with `FSWND`, `RH0` is `RH0(NSEA,1)` (`w3idatmd.F90:726-728`) and `IX = ISEA`,
  `IY = 1` (`w3updtmd.F90:2731-2733`): the `fswnd` argument. The `switch_SMCMlt` object has the
  same arithmetic as the lab's.
- `WNT0` (`RD = 0`), `WNT2` (same expression as `WNT1`) and `OASACM` (`RD = 1`, set after the
  `WNT` line, so it wins) are the template argument `UrhoTime`. Not compared against an object
  built with them ⚠; with a runtime `RD` of 0 or 1 the multiply and add give the same bits as
  the Fortran's constant, except that gfortran may fold `RAI*1.` to `RAI`, which differs only
  for a signalling NaN ⚠.
- `OMPG` is not translated: `!$OMP PARALLEL DO PRIVATE (ISEA,RA0,RAI)` (`w3updtmd.F90:2772`)
  privatises the module pointers `RA0`, `RAI`, whose association is undefined in each thread,
  so the loop reads undefined pointers ⚠ (a WW3 defect). The C++ runs the serial semantics.
- `S` and `T` are output only. `DSEC21` (`w3timemd.F90:330`) runs on the host: the caller passes
  `DT0N` and `DT0T` as the `REAL`s the Fortran stores (`w3updtmd.F90:2749-2750`).
- (v) A throwaway unit with the `openmp-release` flags compiled the header with no warning and
  no FMA. Not compiled for CUDA.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
