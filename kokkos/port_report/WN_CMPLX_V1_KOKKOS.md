# WN_CMPLX_V1_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A helper of the IC3 sea-ice source term, "V1" path (Wang and Shen 2010): the complex wavenumber
of a wave in ice, found by Muller's method (`CMPLX_ROOT_MULLER_V1`) from an open-water first
guess (`JUDGE = 0`, then `JUDGE = 1` around it). For a period `T = 2*PI/SIGMA` below `TS = 10 s`
it solves at 10 s first and steps the period down to `T` in `NSUB = INT((TS-T)*10.)` steps. With
`HICE < 0.001` it returns the open-water wavenumber. Its only caller is `W3IC3WNCG_V1`
(`w3sic3md.F90:811-812`), for the first frequency `IK = KL` or `SIGMA(IK) <= TPI/10.0` (the
`ELSE` of `:803`); `W3IC3WNCG_V1` is called by `W3SIC3` (`:549`) and `W3WAVE`
(`w3wavemd.F90:1063`) when `IC3CHENG` is false.

## What was ported

| | |
|---|---|
| WW3 routine | `WN_CMPLX_V1`, `model/src/w3sic3md.F90:839-981` at NOAA-EMC/WW3 `03adda91`; compiled under switch `IC3`, by 46 regtest switch files (`ww3_tic1.1`, `ww3_tic1.2`, `ww3_tic1.3`, `ww3_ts1`; `git grep -lw IC3 -- '*switch*'`, less `model/src/cmake/switches.json`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`); exercised only by regtest namelists with `IC3CHENG = F`. No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/ic3_wn_cmplx_v1.hpp`](../src/ww_kokkos/ic3_wn_cmplx_v1.hpp), `ww::ic3::wn_cmplx_v1(double sigma, double wn_o, double es, double nu, double dice, double hice, double depth, Kokkos::complex<double>& wn) -> int`; the Fortran result is written to `wn`, the return is 0 or the callee's `EXTCDE` code |
| Callee | `cmplx_root_muller_v1` ([`ic3_cmplx_root_muller_v1.hpp`](../src/ww_kokkos/ic3_cmplx_root_muller_v1.hpp)), called directly. Where it returns the code of the Fortran's `EXTCDE(2)` (`:1244`, `:1268`), this routine returns it at once with `wn` not written, and the shim does the `WRITE` |
| C shim | none yet; its caller `W3IC3WNCG_V1` is not ported |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3sic3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object; its GIMPLE (`-fdump-tree-optimized-lineno`) and disassembly read
against those of a throwaway C++ unit (`wn_cmplx_v1` with the real callee) compiled with the
`openmp-release` flags and `-ffp-contract=off`, 2026-10-16. The C++ unit compiles warning-free.

- (v) Constants: `DBLE(TPI)` is `6.283185482025146484375` (`TPI = 2.0*PI`, both REAL,
  `constants.F90:72-73`); gfortran folds `DBLE(TPI)/TS` to `0.6283185482025146484375` (`:954`,
  `:959`); the REAL literals `0.001`, `0.01`, `0.1`, `0.8`, `0.90`, `1.1`, `1.2` are promoted to
  double; `10.` and `1.0` are exact. Each is a hex literal with a `static_assert` of how it is
  built; the two GIMPLEs carry the same set of values.
- (v) `0.90 * WN0`, `1.1 * WN0`, `0.8 * WN0`, `1.2 * WN0` and the products with `WN` in the loop
  are full complex products with `(literal, 0.0)`: real part `wr*c - wi*0.0`, imaginary part
  with `wi*c` as the first operand of `addsd` in the Fortran object at all six sites (the GIMPLE
  writes `wr*0.0 + wi*c`), and `wi*0.0`, `wr*0.0` shared by `X0` and `X2`. That operand decides
  which NaN payload survives when both addends are NaN. g++ vectorises the two sites after the
  first root call into `vaddsubpd`; without a barrier it paired the lanes of `X2` as
  `wr*0.0 + wi*c`. With `slp_barrier` (`__builtin_assoc_barrier`, from
  `ic3_wn_cmplx_hf.hpp`) on the products with `0.0` of `X2`, every imaginary part in the C++
  object has `wi*c` first, read in both `vaddsubpd` blocks and the scalar loop.
- (v) `CMPLX(WN_O, 0.)` has the default REAL kind: `WN_O` is rounded to float and back
  (`cvtsd2ss`/`cvtss2sd` in both objects). The caller passes `DBLE(K_OCEAN)`, a REAL, so the
  rounding is exact there.
- (v) `NSUB = INT((TS-T)*10.)`: `cvttsd2si` of `10*(10-T)` in both objects, through `fint_d`
  (`ic3_wn_cmplx_hf.hpp`); it is reached only for `T < 10`, so it is positive and in range.
  `REAL(NSUB)` and `REAL(I)` are REAL, rounded to float and promoted; `(TS-T)/REAL(NSUB)` is
  hoisted out of the loop in both objects, then `TS - q*REAL(I)` and `DBLE(TPI)/TT`.
- (v) Branch tests: `HICE < 0.001` and `T < TS` compile to `comisd` with the constant as the
  left operand in both objects, so a NaN `HICE` or `T` takes the `ELSE` path in both.
- (v) No reduction, no MIN/MAX, no libm or libmvec call of its own; the five calls of the callee
  take the same arguments in the same order (`JUDGE` 0 then 1).
- ⚠ The parity of the result is also that of `CMPLX_ROOT_MULLER_V1`
  ([its report](CMPLX_ROOT_MULLER_V1_KOKKOS.md)). CUDA not compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
