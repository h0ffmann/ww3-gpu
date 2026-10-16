# SMCGTCRFR_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Great-circle turning and refraction on a Spherical Multiple-Cell (SMC) grid, applied as a
rotation of each frequency's directional spectrum rather than as an advection. For each
direction bin `j` of wavenumber `n`, the Courant number `CoRfr(j,n)` is split into an integer
shift `K = MOD(INT(|CoRfr|), NTH)` and a fraction `CNST1 = |CoRfr| - INT(|CoRfr|)`; the bin's
energy goes to the bins `j±K` (weight `1 - CNST1`) and `j±(K+1)` (weight `CNST1`), wrapped
around the circle, with `+` when `CoRfr > 0` and `-` otherwise.

## What was ported

| | |
|---|---|
| WW3 routine | `SMCGtCrfr`, `model/src/w3psmcmd.F90:2717-2790` at NOAA-EMC/WW3 `03adda91`, author Jian-Guo Li. Called twice by `W3KRTN` (`w3psmcmd.F90:1257`, `:1262`, when `FLCTH`, before or after `SMCkUNO2` by the parity of `ITIME`), which `W3WAVE` calls under `W3_SMC` (`w3wavemd.F90:1830`, `:2154`). No `#ifdef` inside the routine. Module `W3PSMCMD` is compiled under switch `SMC` (`switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, regtests `ww3_tp2.10`, `ww3_tp2.14`, `ww3_tp2.16`, `mww3_test_09`; `git grep -lw SMC -- '*switch*'`), by no NCEP or NWS switch |
| Kokkos code | [`../src/ww_kokkos/psmc_smcgtcrfr.hpp`](../src/ww_kokkos/psmc_smcgtcrfr.hpp), `ww::psmc::smcgtcrfr`, one cell spectrum on one thread. `NK`, `NTH` (`w3gdatmd`) are arguments; the automatic arrays `SpeGCT(NTH)`, `Spectr(NTH)` (`:2725`, two `malloc` per call in the object) are caller-provided work arrays. `DTH`, `CTMAX` are in the `USE` list and never read |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3psmcmd.F90` under `model/bin/switch_SMCMlt` (WW3's CMake
Release flags, gfortran 16.2), recompiled and checked identical to the CMake object, 2026-10-16;
the C++ unit compiled with the openmp-release flags (g++ 16.2), warning-free.

- (v) `INT(CNST5)` is one `cvttss2si` in the Fortran object, reused by `MOD` (`cltd; idiv`) and
  by `FLOAT` (`cvtsi2ss`); the C++ uses `ww::st4::fint`, which gives `cvttss2si`'s result
  (`INT_MIN` for NaN or `|CoRfr| >= 2^31`), and one `vcvttss2si`, `cltd; idiv`, `vcvtsi2ss` on
  the in-range path. For `|CoRfr| >= 2^31` the C++ subtracts the constant `-2^31`
  (`0xcf000000`) as the Fortran subtracts `cvtsi2ss(INT_MIN)`; on the NaN path g++ folded
  `CNST5 - FLOAT(INT_MIN)` into `CNST5 + 2^31`, which gives the same quiet NaN of `CNST5`, with
  `CNST5` as first operand as in the Fortran's `subss`.
- (v) `CNST2 = 1.0 - CNST1`, `ABS` as `andps` with the sign mask, `CNST6 > 0.0` as `comiss`
  (a NaN takes the negative branch) in both objects.
- (v) Each update `SpeGCT(L) + Spectr(j)*CNST2`, then `SpeGCT(M) + Spectr(j)*CNST1`, is one
  `mulss` and one `addss` with the same first operands in both objects, in source order (the `L`
  store precedes the `M` load, which matters when `L == M`, `NTH = 1`). No FMA in the C++ object,
  no vectorisation of the `j` loop in either (`-fopt-info-vec` reports nothing for `:2717-2790`).
- (v) `Spectr = SpeTHK(:,n)`, `SpeGCT = 0.0`, `SpeTHK(:,n) = SpeGCT` are `memcpy`/`memset` in the
  Fortran and element loops in the C++: moves only, no arithmetic.
- (v) No reduction, no MIN/MAX, no libm call.
- ⚠ Undefined in WW3: when `|CoRfr| >= 2^31` or `CoRfr` is NaN, `INT` gives `INT_MIN`, `K` is
  negative, and `L`, `M` can fall outside `1..NTH`; the Fortran then writes outside `SpeGCT`
  (`w3psmcmd.F90:2744`, `:2760-2774`, no bounds check at `-O3`). The C++ takes the same path and
  writes outside `spegct`. `W3KRTN` limits the current and refraction parts of `VCFLT` by `CTMAX` and
  `ALFLMT` (`:1175`, `:1188-1189`), so reaching it needs a non-finite input.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect in the header. Recompiled `w3psmcmd.F90` from `build_smc`'s
  `flags.make` (object identical to the CMake one) and a C++ unit with the openmp-release flags;
  compared the optimised GIMPLE and the assembly of the `j` loop: one `cvttss2si`, `cltd; idiv`,
  `cvtsi2ss`, `subss` with `CNST5` first, `1.0 - CNST1`, then `CNST2*Spectr(j)` and
  `CNST1*Spectr(j)` with the coefficient as first operand and `product + SpeGCT` in both objects
  (same NaN propagation), `comiss` on `CoRfr > 0` sending NaN to the negative branch, the
  `.rodata` constants `-2147483904`, `-2^31`, `2^31`, `1.0`, no FMA, no libm, no vectorised
  arithmetic. Fixed in this report: the `+ 2^31` fold is on the NaN path only.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. The SMC regtests `ww3_tp2.10` and `ww3_tp2.16` (switch `SMC`)
reach `W3KRTN` when refraction or great-circle turning is on, so its checkpoint (ADR-0011) can
exercise it once a shim and the caller's hunk exist.
