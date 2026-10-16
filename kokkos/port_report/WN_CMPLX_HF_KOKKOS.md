# WN_CMPLX_HF_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16). Its callee `CMPLX_ROOT_MULLER_V1` has no header yet: the
header takes it as a callable argument.

A helper of the IC3 sea-ice source term, "V1" path (Wang and Shen 2010): the complex wavenumber
of a wave in ice for a period below 10 s, found by Muller's method from the previous frequency's
root, stepping the period from `2*PI/SIGMA_LAST` to `2*PI/SIGMA` in `NSUB = INT((TS-T)*10.)`
steps. With `HICE < 0.001` it returns the open-water wavenumber. Its only caller is
`W3IC3WNCG_V1` (`w3sic3md.F90:805-806`), for `IK > KL` and `SIGMA(IK) > TPI/10.0` (`:759`,
`:803`); `W3IC3WNCG_V1` is called by `W3SIC3` (`:549`) and `W3WAVE` (`w3wavemd.F90:1063`) when
`IC3CHENG` is false.

## What was ported

| | |
|---|---|
| WW3 routine | `WN_CMPLX_HF`, `model/src/w3sic3md.F90:984-1102` at NOAA-EMC/WW3 `03adda91`; compiled under switch `IC3`, by 46 regtest switch files (`ww3_tic1.1`, `ww3_tic1.2`, `ww3_tic1.3`, `ww3_ts1`; `git grep -lw IC3 -- '*switch*'`, less `model/src/cmake/switches.json`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`); exercised only by regtest namelists with `IC3CHENG = F`. No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/ic3_wn_cmplx_hf.hpp`](../src/ww_kokkos/ic3_wn_cmplx_hf.hpp), `template <class RootMuller> ww::ic3::wn_cmplx_hf(double sigma, double wn_o, double es, double nu, double dice, double hice, double depth, double sigma_last, const Kokkos::complex<double>& wn_last, Kokkos::complex<double>& wn, const RootMuller& root) -> int`; the Fortran result is written to `wn`, the return is 0 or the callee's `EXTCDE` code |
| Not ported | `CMPLX_ROOT_MULLER_V1` (`:1106-1276`): no header yet. `root(x0, x1, x2, judge, sigma, es, nu, dice, hice, depth, p3)` stands for it and returns 0, or the code where the Fortran stops in `EXTCDE(2)` (`:1244`, `:1268`); a nonzero code is returned at once, `wn` not written. The callable is the only structural change, and goes when that header exists |
| C shim | none yet; its caller `W3IC3WNCG_V1` is not ported |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3sic3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object; its GIMPLE (`-fdump-tree-optimized-lineno`) read against that of a
throwaway C++ unit (`wn_cmplx_hf` with an external function as `root`) compiled with the
`openmp-release` flags and `-ffp-contract=off`, 2026-10-16. The C++ unit compiles warning-free.

- (v) Constants: `DBLE(TPI)` is `6.283185482025146484375` (`TPI = 2.0*PI`, both REAL,
  `constants.F90:72-73`), and the REAL literals `0.001`, `0.90`, `1.1` are promoted to double
  (`0x1.0624dep-10`, `0x1.ccccccp-1`, `0x1.19999ap+0`); `10.` is exactly `10.0`. Each is a hex
  literal with a `static_assert` of how it is built; both GIMPLEs carry the same values.
- (v) `0.90 * WN` and `1.1 * WN` are full complex products with `(literal, 0.0)` in the object,
  not a scaling: real part `wr*c - wi*0.0`, imaginary part `wr*0.0 + wi*c`. Both are written out
  component by component, so the products with `0.0` keep NaN and the sign of zero. Before the
  loop gfortran vectorises them and the imaginary part of `X0` is `li*0.9 + lr*0.0` (the other
  operand order), that of `X2` `lr*0.0 + li*1.1`. Inside the loop the GIMPLE reads
  `wr*0.0 + wi*c`, but the object's `addsd` has `wi*c` as its first operand for both `X0` and
  `X2`, and that operand decides which NaN payload survives when both addends are NaN. The C++
  reproduces all four first operands in the `vaddsubpd` lanes of its object; inside the loop
  that needed the order `wi*c + wr*0.0` and a PAREN barrier on the products of `X2`
  (`slp_barrier`).
- (v) `CMPLX(WN_O, 0.)` has the default REAL kind: `WN_O` is rounded to float and back (`(float)`
  then `(double)` in both GIMPLEs). The caller passes `DBLE(K_OCEAN)`, a REAL, so the rounding
  is exact there.
- (v) `NSUB = INT((TS-T)*10.)` truncates a double: `fint_d` reproduces `cvttsd2si` (NaN and out
  of range give `INT_MIN`, then the loop does not run). `REAL(NSUB)` and `REAL(I)` are REAL,
  rounded to float and promoted; `(TS-T)/REAL(NSUB)` is hoisted out of the loop in both
  objects, then `TS - q*REAL(I)` and `DBLE(TPI)/TT`, in the Fortran's order.
- (v) No reduction, no MIN/MAX, no libm or libmvec call of its own; the two calls of the callee
  take the same arguments in the same order (`JUDGE = 1`).
- (v) Fresh review 2026-10-16: Fortran object recompiled and checked identical to `build_mixa`'s,
  GIMPLE and disassembly of both read site by site (constants in `.rodata`, `cvttsd2si`, the
  float round trips, the hoisted quotient, the operand order of every complex product). Found
  that the loop's imaginary parts of `X0` and `X2` added `wr*0.0` first in `vaddsubpd` where the
  Fortran's `addsd` adds `wi*c` first: a NaN payload or sign when both addends are NaN, nothing
  else. Fixed in the header and read again in the object; the `EXTCDE` line cited as `:1271`
  is `:1268`, corrected.
- ⚠ The result is whatever `root` returns; its parity is that of a future
  `CMPLX_ROOT_MULLER_V1` port. CUDA not compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
