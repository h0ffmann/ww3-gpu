# W3SIN6_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Observation-based wind input of the ST6 package (switch `ST6`), after Donelan et al. (2006, JPO
36(8) 1672-1689) as implemented by Rogers et al. (2012, JTECH 29(9) 1329-1346), both as the
Fortran header cites them (`:323-324`): `S_in = D N` with the growth rate
`D = (rho_a/rho_w) sigma (2.8 - (1 + tanh(10 sqrt(B_n) W - 11))) sqrt(B_n) W`, where
`W = max(0, U/c cos(theta - theta_u) - 1)^2`, `U = SIN6WS u*` and `B_n` the normalised spectral
saturation. `LFACTOR` then reduces `D` so that the wave-supported stress stays within the total
stress, and when `SIN6A0 > 0` the adverse-wind term (`min` in place of `max`, scaled by
`SIN6A0`) is subtracted and `TAU_WAVE_ATMOS` gives its stress. `W3SRCE` calls it per sea point
(`w3srcemd.F90:1252`); the point post-processors `ww3_outp` (`:2247`), `ww3_ounp` (`:2419`) and
`gx_outp` (`:1176`) call it too.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SIN6`, `model/src/w3src6md.F90:298-539` (242 lines) at NOAA-EMC/WW3 `03adda91`; authors S. Zieger, Q. Liu; its module `SAVE` array `IKN` (`:104`). Compiled by every switch with `ST6` (`git grep -lw ST6 -- '*switch*'`): `ww3_ts1`, `ww3_ts2`, `ww3_ts3`, `ww3_tp2.15`, `mww3_test_05`, and `model/bin/switch_NRL3`, `switch_swin`, `switch_UoM_nl1`, `_nl3`, `_nl3s`; by no `switch_NCEP_*` or `switch_NWS_*` |
| Kokkos code | [`../src/ww_kokkos/srce_st6_sin6.hpp`](../src/ww_kokkos/srce_st6_sin6.hpp): `ww::srce::st6::w3sin6_point(...)`, `KOKKOS_INLINE_FUNCTION`, the Fortran arguments in order, then `Params` (`NK`, `NTH`, `NSPEC`), `DTH`, `XFR`, `SIG2`, `DDEN2`, `ECOS`, `ESIN`, the seven `SIN6*` scalars, `IKN`, the 10 Hz tables and `work`; `w3sin6_sig_dsii()` (`SIG`, `DSII` of `:457-459`, used by the kernel and by the shim for `lfactor_tables()`); `w3sin6_work_size()`. It calls `lfactor()` ([`srce_st6_lfactor.hpp`](../src/ww_kokkos/srce_st6_lfactor.hpp)) and `tau_wave_atmos()`, `tau_wave_atmos_nk10hz()` ([`srce_st6_tau_wave_atmos.hpp`](../src/ww_kokkos/srce_st6_tau_wave_atmos.hpp)), unchanged |
| `IKN` | The first-call block of `W3SIN6` (`:449-456`) is the same statement as that of `W3SDS6` (`:696-704`): `IKN = IRANGE(1,NSPEC,NTH)`, then `EXTCDE(2)` when `SIZE(IKN) /= NK`; only the `FORMAT` (`:601` of each) names the routine. So the host table of `w3sds6_ikn()` ([`srce_st6_sds6.hpp`](../src/ww_kokkos/srce_st6_sds6.hpp)) is reused: one table per process, built by the shim before the first call, its return 2 printed with `W3SIN6`'s format when `W3SIN6` is the first ST6 call. The kernel does not test `SIZE(IKN)`, as the Fortran does not once `IKN` is allocated |
| Structural changes | The automatic arrays of `:412-417` become `work`: `K`, `SDENSIG`, `CG2`, `SQRTBN2`, and one `NSPEC` array for `W1` then `W2` (`W1` is dead after `:498`), plus `DSII`, `SIG`, `WN`, `ADENSIG`, `KMAX`, `ANAR`, `SQRTBN`, `LFACT`, `CINV` (`NK` each). `ECOS2`, `ESIN2` (`:446-447`) are read in place from `ECOS(1)`, `ESIN(1)`; `CINV2 = WN2/SIG2` (`:461`) and `DSII2 = DDEN2/DTH/SIG2` (`:457`) are evaluated per element where they are read, the same division per element. The callees' `ALLOCATE`s share one region after W3SIN6's: `LFACTOR` needs `6*NK10Hz`, `TAU_WAVE_ATMOS` after it `4*NK10Hz`. Total `5*NSPEC + 9*NK + 6*NK10Hz` reals, `NK10Hz = lfactor_nk10hz(NK, SIG2(IKN(1)), XFR)` |
| Return code | `(LFACTOR's) \| (TAU_WAVE_ATMOS's << 4)`: `kLfactorItermax` (1, the Fortran writes `LFACTOR`'s warning and goes on), `kLfactorExtcde3` (3, the Fortran stops in `LFACTOR` at `:983-986`; the kernel returns at once), `3 << 4` (`EXTCDE(3)` in `TAU_WAVE_ATMOS`, `:1274-1277`). The shim writes the warning and calls `EXTCDE` where the Fortran does |
| Dropped | `CD` is a dummy argument the Fortran never reads; kept in the signature, unused. `W3_S` (`STRACE`) |
| C shim | none yet. It builds `IKN` (`w3sds6_ikn`) and the 10 Hz tables (`lfactor_tables` with `w3sin6_sig_dsii` of the first grid) once per process, passes `work`, and acts on the return code |
| Plan | issue #42, branch `goal/42-port-st-flx-swl` |

## Arithmetic

Reference: the `-O3` object of `w3src6md.F90` (WW3's CMake Release flags, gfortran 16.2, pinned
`ww3` shell) recompiled from `build_st6f4` (`ST6 FLX4`) and `build_st6f5` (`ST6 FLX5`) with
`-fdump-tree-optimized-lineno`, each identical in its disassembly to the CMake object, and
identical to each other. Compared, operation by operation in the optimised GIMPLE and the
assembly, with a throwaway C++ unit compiled with the `openmp-release` flags plus `-g`
(`-ffp-contract=off`, warning-free; `-g` leaves the code unchanged, checked), 2026-10-16.

- (v) Libmvec: `TANH` in `:498` and `:525` is vectorised by gfortran through `tanhf.simdclone`,
  `_ZGVbN4v_tanhf` in groups of four from the first element when `NSPEC >= 4`, then scalar
  `tanhf` for the last `NSPEC mod 4` (all elements when `NSPEC < 4`; the loop guard is
  `niters - 1 <= 2`). The C++ reproduces it with `sin6_detail::mvec_tanhf` (the
  `st4_libmvec.hpp` pattern, declared in the header because `st4_libmvec.hpp` has no `tanhf`);
  its object calls `_ZGVbN4v_tanhf` at both sites and `tanhf` in the tails, and no other vector
  variant (`_ZGVdN8v_tanhf` absent). glibc picks the variant from the CPU at run time, for both.
- (v) Other libm calls: `COS`/`SIN(USDIR)` (`:424-425`) are one `sincosf` in the object; in the
  C++ object one `sincosf` serves both `W3SIN6` and the inlined `LFACTOR` (the same argument,
  the same results). `SQRT` (`:486`) is `sqrtps`/`sqrtss` inline in the object; the C++ has
  `vsqrtss` plus a `sqrtf` call taken only for a negative argument (g++'s `errno` path); glibc's
  `sqrtf` returns the `sqrtss` result there ⚠ (argued from glibc's wrapper, not disassembled).
- (v) Reductions: `SUM(K,1)` (`:471`, `:484`) and `SUM(LFACT)` (`:510`) add in index order from
  `0.0`: the vector loops extract the four lanes (`BIT_FIELD_REF` 0, 32, 64, 96) and add them one
  by one, then a scalar tail of at most three. In the C++ object `:471` and `:484` are scalar
  loops and `:510` an in-order lane chain (8 lanes, then 4, then scalar): the same sums. `:510` tests `REAL(NK) > sum` (`comiss`), the C++
  `sum < (float)NK`: false for a NaN sum in both.
- (v) `MAXVAL(K,1)` (`:474`) is a call of `_gfortran_maxval_r4`; the C++ reproduces libgfortran
  16.2's loop as `srce_swl6.hpp` does (`-HUGE` for an empty column, skip to the first element
  `>= -Inf`, then `x > r ? x : r`, quiet NaN when all are NaN). `KMAX < 1.0E-34` (`:476`) is a
  `comiss` that sends a NaN `KMAX` to the division branch, as the C++ `<` does.
- (v) `MIN`/`MAX` operand order: `MAX(0., x)` (`:495`) is `maxps`/`maxss` with `x` in the
  destination and `0.0` (`pxor`) as source at all three sites (4-wide body, 2-wide pair, scalar
  tail): `x > 0 ? x : 0`, so a NaN or `-0` gives `+0`. `MIN(0., x)` (`:523`) is `minps`/`minss`
  the same way. The C++ `maxss(x, 0)`, `minss(x, 0)` compile to `vmaxss`/`vmaxps` with `x` as
  first source and to `vcmpnltss`+`vandnps` (`x < 0 ? x : +0`), the same function.
- (v) Operation order per element, from the object: `ADENSIG = (SUM*SIG)*DTH`;
  `ANAR = 1.0/(SUM*DTH)`; `SQRTBN = SQRT((ANAR*ADENSIG)*((WN*WN)*WN))` (`WN**3` as two
  `powmult`); `W = MAX(0.,(UPROXY*(WN2/SIG2))*(ECOS*COSU+ESIN*SINU)-1.)**2` (the square one
  `mulss`); `D = (((DAIR/1000.)*SIG2)*(2.8-(1.+TANH((10.*SQRTBN2)*W-11.))))*SQRTBN2)*W` with
  `DAIR/DWAT` one division, `2.8` as `0x40333333`; `:525` the same with `*SIN6A0` after
  `*SIG2`, subtracted from `D`; `DSII = (DDEN2/DTH)/SIG2`; `SDENSIG = (S*SIG2)/CG2`;
  `S = D*A`; `D(IKN+ITH-1) = D(IKN+ITH-1)*LFACT`. The vector bodies write `x - 1.`, `x - 11.`
  as `x + (-1.)`, `x + (-11.)`, the same bits. No FMA in the C++ object.
- (v) Index vectors: `IKN+(ITH-1)` (`:464`, `:488`, `:512`) is an integer temporary per `ITH`;
  the scatters and gathers through `IKN` are element copies or one product per element, in
  `ITH`-outer, `IK`-inner order in both.
- (v) Variants: `W3_S` is output only, the routine has no `W3_T*` block. `W3_OMPG` makes `IKN`
  `THREADPRIVATE` (`:106-107`): recompiled with `-fopenmp -DW3_OMPG` (`build_st6f4` flags),
  `W3SIN6` has the same counts of every floating-point instruction and the same library calls;
  the one addition is the TLS access of `IKN`. `FLX4` and `FLX5` do not reach the routine.
- ⚠ Where an addition or multiplication has its operands commuted between the two objects
  (`ECOS*COSU + ESIN*SINU`, `SIG*sum`, `1.+TANH`, `D*A`), the result differs only when both
  operands are NaN (which payload survives).
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, pinned `#cuda` shell, whose `nvcc_wrapper` is
  `kokkos-cuda-5.2.2`), never run. On a device there is no libmvec, and `tanhf`, `sincosf`,
  `expf`, `logf` are CUDA's (the SASS has 764 `FFMA*` from them and from the IEEE division
  sequences); not bit for bit there.
- (v) Fresh review 2026-10-16: no defect found, nothing changed in the header. Recompiled
  `w3src6md.F90` from `build_st6f4` and `build_st6f5` (each identical to its CMake object, and
  to each other) and with `-fopenmp -DW3_OMPG` (same floating-point instruction counts and calls
  as without, plus the TLS access of `IKN`); compared with a fresh `-O3` C++ unit (warning-free):
  the `tanhf` loops of `:498` and `:525` (`niters - 1 <= 2` guard, no peeling, `_ZGVbN4v_tanhf`
  from the first element, at most three scalar `tanhf`) against `mvec_tanhf`; the `maxps`/`maxss`
  and `minps`/`minss` destination at all six Fortran sites against the C++ vector body, epilogue,
  scalar tail and alias-check fallback; `_gfortran_maxval_r4` against libgfortran's loop; the
  in-order `SUM` reductions of `:471`, `:484`, `:510` (no `vaddps` reduction in the C++ object);
  the folded `1.0E-34` (`0x0704ec3d`, the same from `static_cast<Real>(1.0E-34)`); the `IKN`
  gathers and scatters; the `LFACTOR` and `TAU_WAVE_ATMOS` arguments against the `lfactor (...)`,
  `tau_wave_atmos (...)` calls in the GIMPLE; the work partition (W3SIN6's `5*NSPEC + 9*NK`
  disjoint from `cwork`; `TAU_WAVE_ATMOS` writes its `4*NK10Hz` only after `MK10Hz == NK10Hz`,
  the same `NK10Hz` that `LFACTOR` checked, so inside `6*NK10Hz`); the return-code packing.
  glibc 2.42's `sqrtf` wrapper was disassembled: a negative argument goes to
  `__kernel_standard_f` (type 126), whose `0/0` result is argued from source ⚠.
  `mvec_tanhf` has the shape of `st4_libmvec.hpp`'s `mvec_expf` and belongs there; it stays local
  because that header was not to be edited in this unit.

## What it found in WW3

- (v) `IKN` is built only by the first `W3SIN6` or `W3SDS6` call of a process (per thread under
  `OMPG`) and `SIZE(IKN) /= NK` is tested only then (`:449-456`). In a `ww3_multi` run whose ST6
  grids differ in `NK` or `NTH`, a later grid indexes `SIG2`, `DDEN2`, `WN2`, `CG2`, `SQRTBN2` and
  `D` through the first grid's `IKN`: out-of-bounds reads and writes, or a silently wrong
  gather, and `CG2`, `SQRTBN2` elements left unset (they are `malloc`'d). The kernel reproduces
  this only with the first-built table, and an out-of-bounds index is undefined in both ⚠.
  (v) `mww3_test_05`, the only `ww3_multi` regtest with `ST6`, has the same `NK`, `NTH` on every
  grid (`W3SDS6_KOKKOS.md`).
- (v) `#ifdef W3_OMPG || W3_OMPH` (`:106`) tests `W3_OMPG` alone (`LFACTOR_KOKKOS.md`): under
  `OMPH` without `OMPG`, the first-call build of `IKN` races. No regtest switch has that.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
