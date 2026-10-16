# LFACTOR_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

`LFACTOR`, a private subroutine of the ST6 source-term module `W3SRC6MD`, called once per point by
`W3SIN6` (`w3src6md.F90:506`). It reduces the positive wind input `S_in(f)` by a factor
`L(f) = MIN(1, EXP((1 - U/c(f)) RTAU))` until the stress it carries, extended to 10 Hz with
`S_in ∝ f^-2`, meets the constraint `|tau_v + tau_w| <= tau_tot = rho_a u*^2`, where `tau_v` is
the viscous stress and `tau_w` the wave-supported stress (`TAUWINDS`). `RTAU` is found by an
iteration of at most 80 steps. The routine also returns the Charnock parameter
`CHKMIN / SQRT(1 - MIN(tau_w/tau_tot, 0.99))`, with the optional high-wind cap of `CHKMIN`
(`:1101-1108`). Rogers et al. (2012), doi:10.1175/JTECH-D-11-00092.1.

## What was ported

| | |
|---|---|
| WW3 routine | `LFACTOR`, `model/src/w3src6md.F90:805-1135` (331 lines) at NOAA-EMC/WW3 `03adda91`; authors S. Zieger, Q. Liu; its `SAVE` module arrays `MK10Hz`, `IK10Hz`, `SIG10Hz`, `DSII10Hz` (`:103-108`). Compiled with `ST6`: 61 switch files (`git grep -lw ST6 -- '*switch*'`: `mww3_test_05` 20, `ww3_ts3` 24, `ww3_ts1` 5, `ww3_ts2` 5, `ww3_tp2.15` 2, `model/bin/switch_NRL3`, `switch_swin`, `switch_UoM_nl1`, `_nl3`, `_nl3s`); no NCEP or NWS operational switch. Its only `#ifdef` blocks are `W3_S` and `W3_T6` (output) |
| Kokkos code | [`../src/ww_kokkos/srce_st6_lfactor.hpp`](../src/ww_kokkos/srce_st6_lfactor.hpp): `ww::srce::st6::lfactor(...)`, `KOKKOS_INLINE_FUNCTION`, the Fortran arguments in order, then `Params` (`NK`, `NTH`), `DTH`, `XFR`, `ECOS`, `ESIN`, the six `SIN6*` scalars, the tables and `work`; `lfactor_nk10hz()` for `NK10Hz` (`:952-953`); `lfactor_tables()`, an `inline` host function, for the first-call block. It calls `rrange()` and `tauwinds()` of [`srce_st6_util.hpp`](../src/ww_kokkos/srce_st6_util.hpp) and the libmvec helpers of [`st4_libmvec.hpp`](../src/ww_kokkos/st4_libmvec.hpp) |
| Structural changes | The six `ALLOCATE`s per call (`:972-977`) become `work`, `6*NK10Hz` floats; the `ECOS2`/`ESIN2` copies and the `RESHAPE` temporaries are read in place (`ECOS(ITH+(IK-1)*NTH)`); the `SDENS10Hz*LF10Hz` temporaries of `:1067-1069` are rounded per element inside the sum, no array. The `SAVE` arrays allocated and filled on the first call (`:955-969`, `:995-1001`, `:1010-1013`) become `lfactor_tables(nk, xfr, sig, dsii, mk10hz, ik10hz, sig10hz, dsii10hz)`, run once on the host with the first call's `SIG`, `DSII` (W3SIN6's `SIG2(IKN)`, `DSII2(IKN)`, `:458-459`): `ik10hz` holds `range_size(1,NK10Hz,1)`, the other two `NK10Hz`. `EXTCDE(2)` (`:958-961`) and `EXTCDE(3)` (`:983-986`) become return codes 2 and 3; the `WRITE(NDST,280)` warning at `IK >= ITERMAX` (`:1093-1094`) becomes return code 1, outputs valid |
| Dropped | `CALL STME21(TIME, IDTIME)` (`:1047`): `STME21` (`w3timemd.F90:676-747`) only writes its string argument; `IDTIME` is read at `:1061`, `:1111-1112` (`W3_T6`) and `:1093` (the warning), never in arithmetic. `TAU_NND` (`:1036`, `:1067`) and `TAU_INIT` (`:1038`) feed only `WRITE`s (`:1080-1081`, `W3_T6`; `:1093-1094`). `IK` is read only by the warning test |
| C shim | none yet. It builds the tables once per process with the first call's arguments, passes `work`, stops on 2 or 3, and on 1 writes the warning (it then needs `TAU`, `ERR`, `TAU_NND`, which the kernel does not return ⚠) |
| Plan | issue #42, branch `goal/42-port-st-flx-swl` |

## Arithmetic

Reference: the `-O3` object of `w3src6md.F90` (WW3's CMake Release flags, gfortran 16.2,
pinned `ww3` shell) under the `ST6 FLX4` and `ST6 FLX5` reference switches, each checked
identical in its disassembly to the CMake object; the two objects are identical to each other.
Compared, operation by operation in the optimised GIMPLE and the assembly, with a throwaway C++
unit compiled with the `openmp-release` flags (warning-free), 2026-10-16.

- (v) `NK10Hz` (`:952`): `SIG(1)/TPI` (folded `0x40c90fdb`), `10./that`, `logf`, `logf(XFR)`,
  a division, then gfortran's `CEILING`: `i = cvttss2si(q)`, `i+1` if `REAL(i) >= q` else `i+2`
  (the `+1` folded in). The C++ uses `fint` and the same compare; two `logf` calls in both.
- (v) Libmvec, as `st4_libmvec.hpp` reproduces it: `SIG10Hz = SIG(1)*XFR**(IK10Hz-1.0)`
  (`:996`) is `_ZGVbN4vv_powf` by four from the first element when `NK10Hz >= 4`, then scalar
  `powf`; `LF10Hz = MIN(1.0, EXP(UCINV10Hz*RTAU))` (`:1065`) is `_ZGVbN4v_expf` by four, then
  `expf`. The C++ object calls the same two libmvec functions and the same scalar tails. glibc
  picks the libmvec variant from the CPU at run time, for both.
- (v) `DRTAU**SIGN_NEW` (`:1088`): `__builtin_powif`, one call of libgcc's `__powisf2` in both
  objects. `SIGN_NEW = INT(SIGN(1.0,ERR))` is `copysignf(1,ERR)` then a conversion, ±1 always.
- (v) Other libm calls: `COS`/`SIN(USDIR)` (`:1030-1031`) are one `sincosf` in both objects;
  `TANH` (`:1105`) `tanhf`; the five `SQRT` (`:1037`, `:1042`, `:1070`, `:1074`, `:1108`) are
  inline `sqrtss`, no `sqrtf` call in either.
- (v) Reductions: `SUM(S,1)*DTH` and `SUM(MAX(0.,S)*RESHAPE(ECOS2,...),1)*DTH` (`:992-994`,
  `:1015-1017`) and every inlined `TAUWINDS` (11 copies in the object, `:1467`) add in index order
  from `0.0`: the vector loops add the lanes one at a time (`BIT_FIELD_REF` 0..3, no
  `REDUC_PLUS`). The C++ loops compile to the same in-order chains (AVX2 by 8 then 4). Each
  `TAUWINDS` term is `(s*c)*d`, with `SDENS10Hz*LF10Hz` formed first in the loop, so
  `((s*lf)*c)*d`, then one `mulss` by the folded `9.806e+3`.
- (v) `MIN`/`MAX` operand order, from `maxss`/`minss`/`maxps`/`minps` of the object (destination
  `d`, NaN or tie gives `s`): `MAX(0.,S)` (`:993-994`, `:1016-1017`, vector bodies and scalar tails
  alike), `MAX(0.0, ...)` (`:1026`), `MIN(1.0, EXP)` (`:1065`), `MAX(0.5*(1+DRTAU), 1.00010)`
  (`:1086`) and `MIN(TAU_WAV/TAU_TOT, 0.99)` (`:1108`) put the variable in `d`; `:1028` puts
  `TAU_VIS` in `d` and `0.95*TAU_TOT` in `s`. The header's `minss`/`maxss` keep each; the C++
  object's `vminps` at `:1065` has the `EXP` result as first source.
- (v) Rewrites that keep the bits: gfortran computes `-5.0E-5*U10 + 1.1E-3` as
  `1.1E-3 - U10*5.0E-5`, g++ as `U10*(-5.0E-5) + 1.1E-3` (a negation is exact); products are
  commuted (`DAIR*(M*U10**2)`, `(0.5*(A-B))*(1-T)`), which IEEE multiplication does not see.
  No FMA in either object (`-ffp-contract=off`).
- (v) Variants: `W3_S` and `W3_T6` are output only. `W3_OMPG` makes the `SAVE` arrays
  `THREADPRIVATE` (`:106-108`): recompiled with `-fopenmp -DW3_OMPG`, `lfactor` has the same
  floating-point operations and libm calls; its four extra calls are the TLS accessors of
  `MK10Hz`, `IK10Hz`, `SIG10Hz`, `DSII10Hz`.
- ⚠ When both operands of one `addss` or `mulss` are NaN the payload depends on operand order, which
  the two compilers place differently (sums, and the commuted products above); no other bit can
  differ.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, pinned `#cuda` shell), never run. On a device there
  is no libmvec and `expf`, `logf`, `powf`, `tanhf`, `sincosf` are CUDA's (the SASS has `FFMA`
  from them); `powi` on the device is `__powisf2`'s algorithm written out. Not bit for bit there.
- (v) Fresh review 2026-10-16: no arithmetic defect in the header. Recompiled `w3src6md.F90` from
  `build_st6f4` and `build_st6f5` flags (disassembly identical to each CMake object, and to each
  other) and the C++ unit with the `openmp-release` flags (warning-free); compared the GIMPLE and
  assembly of `lfactor`: the libm/libmvec/libgcc call inventory (`logf` x2, `_ZGVbN4vv_powf` with
  scalar `powf` tail and the `NK10Hz >= 4` threshold, `_ZGVbN4v_expf` with `expf` tail and the same
  threshold, `sincosf`, `__powisf2`, `tanhf`, inline `sqrtss`), every folded constant (identical
  decimal expansions in both GIMPLE dumps), the `minss`/`maxss`/`minps`/`maxps` operands per site,
  the in-order lane chains of all 11 inlined `TAUWINDS` and of the `SUM(...,1)` reductions, the
  loop exit (the object tests `ABS(ERR)` before `__powisf2`, `IK == 80` at `EXIT` or 81 at the end
  both give the warning, as `ik >= kItermax` does), and the first-call blocks against
  `lfactor_tables()` and `TAU_WAVE_ATMOS_KOKKOS.md` "The 10 Hz tables". `STME21`
  (`w3timemd.F90:676-747`) only writes its string; `TAU_NND`, `TAU_INIT` only feed `WRITE`s. The
  `OMPG` object of the translation has the same operation inventory. Fixed in this report only: five
  `SQRT`, not four; the NaN-payload caveat covers products too.

## What it found in WW3

- (v) `#ifdef W3_OMPG || W3_OMPH` (`w3src6md.F90:106`) tests only `W3_OMPG` (the preprocessor
  warns "extra tokens at end of #ifdef directive"): an `OMPH` build without `OMPG` shares the
  `SAVE` arrays between threads, and their first-call `ALLOCATE` races. Every ST6 switch with
  `OMPH` also has `OMPG` at `03adda91`, so no regtest reaches it.
- (v) `SIG10Hz` and `DSII10Hz` are filled on the first call only (`FLGSET10Hz`, `:963-969`) and
  kept per thread, not per grid: in `ww3_multi`, a second grid with the same `NK10Hz` but another
  `SIG(1)` or `XFR` runs with the first grid's 10 Hz bands, silently; another `NK10Hz` stops in
  `EXTCDE(3)`. A port that keeps one table per grid would differ from WW3 there; the shim must
  keep the first call's table to reproduce it ⚠.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
