# W3SPR2_KOKKOS

**Status:** ported, phase 1 (translated). Wired: WW3 at the `WW3/` pin (`357f575c`) calls it at `w3srcemd.F90:1110, :2223` behind `WW_KOKKOS_SPR2` (default off); not regtested: compiled, never run inside WW3 (the owner, 2026-10-16).

Mean wave parameters for the Tolman and Chalikov (1996) source terms (switch `ST2`): total
energy `EMEAN`, mean frequency `FMEAN`, mean wavenumber `WNMEAN`, the spectral maximum `AMAX`,
Phillips' constant per frequency `ALFA(NK)` and the peak frequency `FP` estimated from `FPI`.
`W3SRCE` calls it per sea point before the source terms (`w3srcemd.F90:1042`) and in the
time-step loop (`:1869`); `ww3_outp` and `gx_outp` call it too (`ww3_outp.F90:1962`, `:2007`,
`:2139`, `:2185`; `gx_outp.F90:1068`, `:1113`). The calls at `ww3_outp.F90:1962`, `:2139` and
`gx_outp.F90:1068` pass `FP` as both `FPI` and `FP`; the object reads `FPI` before it stores
`FP`, and the header takes `fpi` by value.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SPR2`, `model/src/w3src2md.F90:125-288` at NOAA-EMC/WW3 `03adda91`; compiled by 83 regtest switch files with `ST2` (`ww3_ts1`, `ww3_ts2`, `ww3_ts3`, `ww3_ta1`, `mww3_test_05`; `git grep -lw ST2 -- 'regtests/*switch*'`) and by the operational `model/bin/switch_NCEP_st2` |
| Kokkos code | [`../src/ww_kokkos/srce_st2_spr2.hpp`](../src/ww_kokkos/srce_st2_spr2.hpp), `ww::srce::st2::w3spr2_point`; the automatic array `EB(NK)` (`:232`, a `malloc` per call in the object) is a caller-provided work array |
| Variants | none that change the arithmetic: the routine's only `#ifdef` blocks are `W3_S` tracing (`:210-212`, `:229-231`, `:236-238`) and the `W3_T` `USE` of `NDST` (`:206-208`), which nothing reads. `STAB2`/`STAB3` and the `FLX` choice do not reach it |
| Unused arguments | `DEPTH`, `U` and the `USE`d `NITTIN`, `ZWIND`, `CINXSI`, `NAR1D`, `DFAC`, `N1MAX`, `ECG1`, `EWN1`, `DSIE` are never read (`:203-213`); `depth` and `u` stay in the signature in Fortran order |
| C shim | none yet |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3src2md.F90` (WW3's CMake Release flags, gfortran 16.2, the
`ST2 STAB2 FLX3` build that carries `switch_NCEP_st2`'s physics), recompiled with
`-fdump-tree-optimized-lineno` and checked identical to the CMake object, against the C++ unit
compiled with the `openmp-release` flags and `-ffp-contract=off`, operation by operation, 2026-10-16.

- (v) Section 1 (`:249-255`): the sum over `ITH` is in order in every path of the object (the
  vectorised loop adds the four lanes one by one). The `MAX` over `ITH` is vectorised for
  `NTH >= 8`: four `maxps` lanes seeded with `AMAX`, folded as `max(max(l3,l1), max(l2,l0))`
  (`movhlps`, `shufps 0x55`), then the `NTH mod 4` tail with `maxss`; for `NTH` 1 to 7 a scalar
  `maxss` chain. The header keeps both paths and the lane order, so a NaN or a `-0` in `A` gives
  the same bits; for finite inputs without signed zeros the order does not matter.
- (v) `MAX`: every site has the variable as the `maxss` destination and the constant or
  `A(ITH,IK)` as the source (`UST` `:240`, the two `1.E-7` of `:275-276`, `FPISTR` `:280`),
  whatever the source order; `maxss(d, s) = d > s ? d : s` reproduces it, and the C++ GIMPLE
  shows the same conditional at each site.
- (v) Section 2 (`:259-265`): vectorised (4 and 2 lanes) with in-order sums of `EMEAN`, `FMEAN`,
  `WNMEAN`; `ALFA = ((2*DTH)*SIG)*EB*((WN*WN)*WN)`, `WN**3` as a power tree, `EB*DDEN/CG`,
  `EB/SIG`, `EB/SQRT(WN)` with `sqrtps`/`sqrtss`. The C++ has the same products and quotients in
  the same order; its `sqrtf` call on a negative argument (errno path) returns the same NaN as
  `sqrtss`.
- (v) Sections 3 and 4 (`:270-282`): same operation sequence; `FPISTR**3` as `(F*F)*F`; `TPIINV`
  folded to `0x3e22f983` in both objects (`static_assert` against `ww::srce::kTpiInv`); the
  literals `1.E-4`, `1.E-7`, `0.008`, `0.92`, `3.6E-4`, `6.3E-10`, `9.806` are the same
  single-precision values in both GIMPLE dumps.
- (v) No FMA, no libm or libmvec call (other than the errno `sqrtf` above) in either host object.
- ⚠ `EBAND = EB(NK)/DDEN(NK)` reads `EB(0)` when `NK = 0`, as the Fortran would; WW3 never has
  `NK = 0`.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, the `#cuda` shell's `kokkos-cuda-5.2.2`), never run;
  its `FFMA` come from the IEEE division and square-root sequences, and the constant `MAX`
  sites become `FMNMX`, which agrees with `maxss` when one operand is a non-NaN constant.
- (v) Fresh review 2026-10-16: no arithmetic defect. `w3src2md.F90` recompiled from
  `build_st2`'s `flags.make` (identical to the CMake object) and the C++ unit recompiled with the
  `openmp-release` flags; compared the assembly of every path: section 1 scalar (`NTH` 1 to 7) and
  vector (`maxps` lanes, `movhlps`/`shufps 0x55` fold, tail) against the header's lanes
  `r0 = max(l2,l0)`, `r1 = max(l3,l1)`, `max(r1,r0)`; section 2's 4-lane, 2-lane and scalar
  loops (in-order sums, operand pairs of every product and quotient); the `maxss`/`vmaxss`
  destination at all five `MAX` sites; the nine single-precision constants of both GIMPLE dumps.
  The only divergence is `NTH = 2147483647`, where the object falls back to the scalar chain and
  the header keeps the lanes, which matters only for a NaN or a `-0` and no grid has. Fixed in
  the report: the missing call site `ww3_outp.F90:2185` and the `FP`/`FPI` aliasing note.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
