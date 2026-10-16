# ASTR_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The five mean longitudes of the tidal ephemerides (sun `h`, solar perigee `pp`, moon `s`, lunar
perigee `p`, negative of the lunar ascending node `np`) in cycles, from cubic polynomials in the
time `d1` in days, and their rates in cycles per 365 days (Explanatory Supplement to the
Astronomical Ephemeris, 1961, as cited at `w3tidemd.F90:920-923`).

## What was ported

| | |
|---|---|
| WW3 routine | `ASTR`, `model/src/w3tidemd.F90:910-971` at NOAA-EMC/WW3 `03adda91` (62 lines). Called by `SETVUF` (`w3tidemd.F90:1936`), which `ww3_prnc` calls (`ww3_prnc.F90:1672`), by `W3UCUR` and `W3ULEV` inside `#ifdef W3_TIDE` (`w3updtmd.F90:368`, `w3updtmd.F90:2271`), and by the program `W3PRTIDE` (`ww3_prtide.F90:555`). No `#ifdef` in the routine; `w3tidemd` is compiled by every build (`cmake/src_list.cmake:58`, unconditional), and the callers that reach it at run time need `TIDE`: `ww3_tp2.18` (`switch_TIDE`, `switch_TIDE_MPI`, whose `info` runs `ww3_prtide`), `switch_Ifremer1`, `switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OAS*`, `switch_ite_pdlib` (`git grep -lw TIDE -- '*switch*'`); no `switch_NCEP_*`/`switch_NWS_*` |
| Kokkos code | [`../src/ww_kokkos/tide_astr.hpp`](../src/ww_kokkos/tide_astr.hpp), `ww::tide::astr` (`d1` by value, the ten outputs by reference, all `double` as the Fortran's `REAL(KIND=8)`) |
| C shim | none yet |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3tidemd.F90` (WW3's CMake Release flags, gfortran
16.2) from `build_mixa` (`TIDE` on) and `build_lab` (`TIDE` off), each recompiled and found
identical to the CMake object; the preprocessed source is the same in both and the routine's
instructions are the same, so it has one variant. Compared with a C++ unit built with the
`openmp-release` flags (warning-free), 2026-10-16.

- (v) Every constant in the C++ GIMPLE has the same decimal expansion as in the Fortran GIMPLE,
  including the products of two literals that both compilers fold (`2.d-8*.00002267d0` to
  `4.534e-13`, `3.d-12*.00000007d0` to `2.1e-19`, ...) and `f2 = 360/365`.
- (v) Same operations in the same order: 32 multiplies, 12 adds, 16 subtracts, 10 divides in
  both objects; `d2**3` is `d2*(d2*d2)` and `d1**2` is `d1*d1` in both; no FMA, no libm call,
  no reciprocal. The only differences are operand order of a commutative multiply
  (`(c*d2)*d2` vs `d2*(c*d2)`), which gives the same bits.
- (v) `DINT` is `__builtin_trunc` in the Fortran (an inline SSE2 sequence: `cvttsd2si`,
  `cvtsi2sd`, sign restored by `orpd`, skipped for `|x| >= 2**52`) and `Kokkos::trunc` in the
  C++ (`vroundsd` at `x86-64-v3`). Both are exact truncation and keep the sign of a zero, so the
  results are the same bits for every finite input, `-0` included (both give `-0`, so `x - trunc(x)` is `+0`), for `±Inf` (the guard returns `x`, `vroundsd` returns `x`, the difference is the default NaN in both) and for a NaN (both subtract a NaN with the payload of `x` from `x`; the inputs are quotients, never signalling).
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3tidemd.F90` from `build_mixa` and `build_lab` flags (identical to both CMake objects) and the C++ unit (warning-free); compared the GIMPLE operation by operation (same 70 operations, same constants), the instruction counts (`mulsd` 32, `addsd` 12, `subsd` 16, `divsd` 10 in both), and the `DINT` expansion with its constants read from `.rodata` (guard `0x4330000000000000` = 2**52, mask `0x7fffffffffffffff`) against `vroundsd $0xb` for each input class above. Fixed in this report: the missing callers in `w3updtmd.F90` and the claim that `w3tidemd` is compiled only with `TIDE`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
