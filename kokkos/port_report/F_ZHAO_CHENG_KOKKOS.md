# F_ZHAO_CHENG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A dispatcher of the IC3 sea-ice source term, "Cheng" path: it returns the residual of the
ice-covered dispersion relation at a complex wavenumber, from `DRFUN_DBLE_CHENG` when
`JUDGE > 0` and from `DRFUN_QUAD_CHENG` when `JUDGE == 0` (`w3sic3md.F90:2803-2807`). Its only
caller is `CMPLX_ROOT_MULLER_CHENG` (`:2648-2650`, `:2692`), the Muller root finder, called by
`W3IC3WNCG_CHENG` (`:1939`) and `WN_PRECALC_CHENG` (`:2284-2338`) with `JUDGE` = 1, 2 or 3 only,
so the `JUDGE == 0` branch is never reached in WW3. `W3IC3WNCG_CHENG` is called by `W3SIC3`
(`:520`) and `W3WAVE` (`w3wavemd.F90:1049`) when `IC3CHENG` is true, its default
(`w3gridmd.F90:2929`).

## What was ported

| | |
|---|---|
| WW3 routine | `F_ZHAO_CHENG`, `model/src/w3sic3md.F90:2713-2809` at NOAA-EMC/WW3 `03adda91`; compiled under switch `IC3`, by 46 switch files of the repository (`git grep -lw IC3 -- '*switch*'` lists 47 paths, one of them `model/src/cmake/switches.json`; regtests `ww3_tic1.1`, `ww3_tic1.2`, `ww3_tic1.3`, `ww3_ts1`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`); exercised by every IC3 regtest run with `IC3CHENG = T` (for example `ww3_tic1.3/input_IC3_CHENG`). No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/ic3_f_zhao_cheng.hpp`](../src/ww_kokkos/ic3_f_zhao_cheng.hpp), `ww::ic3::f_zhao_cheng(int judge, const Kokkos::complex<double>& x, double sigma, double es, double nu, double dice, double hice, double depth, const DrfunDble&, const DrfunQuad&) -> Kokkos::complex<double>`; `DOUBLE COMPLEX` is `Kokkos::complex<double>`, `DOUBLE PRECISION` is `double` |
| Callees | `DRFUN_DBLE_CHENG` (`:2812-2962`) and `DRFUN_QUAD_CHENG` (`:2967-3088`) are not translated; they are taken as callables. Despite its name, `DRFUN_QUAD_CHENG` is `COMPLEX(8)`, not quadruple precision |
| C shim | none yet; its caller `CMPLX_ROOT_MULLER_CHENG` is not ported |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3sic3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object; its GIMPLE (`-fdump-tree-optimized-lineno`) and disassembly read
against those of a throwaway C++ unit compiled with the `openmp-release` flags and
`-ffp-contract=off`, the two callees bound to external functions, 2026-10-16. The C++ unit
compiles warning-free.

- (v) No floating-point operation: the routine is two integer tests and a tail call. Both
  objects test `JUDGE > 0` first (`jg`), then `JUDGE == 0` (`je`/`jne`), and tail-call
  `drfun_dble_cheng` or `drfun_quad_cheng` with the arguments unchanged, `JUDGE` passed last to
  the first.
- ⚠ For `JUDGE < 0` the Fortran never assigns `FZHAO` (`:2803-2807`) and returns an undefined
  value. The gfortran `-O3` object returns `(+0.0, +0.0)` on that path (`pxor %xmm0`, copied to
  `%xmm1`); the header returns the same, and the C++ object does too (`vxorpd`). Another compiler
  or flag set may return anything. No caller in WW3 passes a negative `JUDGE`.
- No libm call, no reduction, no MIN/MAX, no `INT`/`NINT`, no libmvec call.
- CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect in the header. The `w3sic3md` object was recompiled from `build_mixa`'s flags and found identical to the CMake object; `__w3sic3md_MOD_f_zhao_cheng` and the C++ unit's function read instruction by instruction (branch order, tail calls, `(+0.0, +0.0)` on `JUDGE < 0`), no `#ifdef` between `:2713` and `:2809`, the callers' `JUDGE` values and the cited lines checked at `03adda91`. Fixed in this report: the switch-file count (46, not 47).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
