# INSIN3_KOKKOS

**Status:** ported, phase 1 (translated). Wired: WW3 at the `WW3/` pin (`357f575c`) runs it inside the Kokkos `W3KOKKOS_ST3_SETUP`, behind `WW_KOKKOS_SPR3, SIN3 or SDS3` (default off); not regtested: compiled, never run inside WW3 (the owner, 2026-10-16).

The initialisation of the WAM4+ input term (switch `ST3`): `INSIN3` of `W3SRC3MD` builds the
friction velocity table `TAUT` with [`TABU_STRESS`](W3SRC3MD_TABU_STRESS_KOKKOS.md) and the
high-frequency stress table `TAUHFT` with [`TABU_TAUHF`](W3SRC3MD_TABU_TAUHF_KOKKOS.md), called
with the maximum frequency `SIG(NK)*TPIINV`.

(v) Its only caller is `W3IOGR`, `model/src/w3iogrmd.F90:1532`, in the branch that reads the
model definition file, behind `IF ( .NOT. FLINP )` with `FLINP` a `SAVE` flag set `.TRUE.` after
the call (`w3iogrmd.F90:355`, `:1531-1534`); found by `git grep -n INSIN3 -- model/src` at
`03adda91`. The routine header's "Called by W3SIN3" (`w3src3md.F90:762`) is wrong: `W3SIN3` never
calls it. So `INSIN3` runs once per process, on the parameters of the first grid whose
`mod_def` is read, and `TAUT`, `TAUHFT`, `DELU`, `DELTAUW`, `DELUST`, `DELALP` are `W3SRC3MD`
module variables (`w3src3md.F90:97-99`), shared by every grid. ⚠ In `ww3_multi` with several ST3
grids of different `SIG(NK)`, `ZZWND`, `AALPHA`, `ZZ0MAX`, `BBETA` or `ZZALP`, the later grids use
the first grid's tables; a shim must reproduce that (one copy per process), not keep a table per
grid. Whether `mww3_test_05` has such grids was not checked.

## What was ported

| | |
|---|---|
| WW3 routine | `INSIN3`, `model/src/w3src3md.F90:726-813` at NOAA-EMC/WW3 `03adda91`; the two calls are `:808-809`. Its only `#ifdef` is `W3_S` (trace) |
| Kokkos code | [`../src/ww_kokkos/srce_st3_insin3.hpp`](../src/ww_kokkos/srce_st3_insin3.hpp), `ww::srce::st3::insin3(p, sig, zzwnd, aalpha, zz0max, bbeta, zzalp, taut, tauhft)`, an `inline` host function returning `Insin3Steps{delu, deltauw, delust, delalp}`; `sig` is `SIG(1:NK)`, the pointer at `SIG(1)` |
| Tables | `taut` and `tauhft` are caller arrays laid out as the two called headers declare (Fortran column-major, 0-based) |
| C shim | none yet; it must keep both tables and the four steps once per process, built on the first `mod_def` read, as `FLINP` does (`w3iogrmd.F90:1531-1534`) |
| Which regtests compile it | every switch with `ST3`: `ww3_ts1` (1), `ww3_ts2` (5), `ww3_ts3` (20), `ww3_tnc1` (1), `mww3_test_05` (20); no `switch_NCEP_*` or `switch_NWS_*` |
| Plan | issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3src3md.F90` (WW3's CMake Release flags, gfortran 16.2) under
`ST3 STAB0 FLX0` and `ST3 STAB3 FLX0`, both recompiled with `-fdump-tree-optimized-lineno` and
identical to the CMake objects (v); the C++ unit compiled with the `openmp-release` flags and
`-ffp-contract=off`, warning-free, and read against it, 2026-10-16.

- (v) The routine's only arithmetic is `:809`: one `mulss` of `SIG(NK)` by the folded `TPIINV`,
  `0x3e22f983` (`1.5915493667125701904296875e-1` in both GIMPLE dumps), rounded to `REAL` and
  stored before `TABU_TAUHF` is called. The C++ does the same single float multiply by the same
  constant (`static_assert` against `0x1.45f306p-3f`) before `tabu_tauhf`.
- (v) Call order as in the object: `TABU_STRESS` first (`:808`), then `TABU_TAUHF` (`:809`); g++
  inlines `tabu_stress` and calls `tabu_tauhf` out of line, which does not change any operation.
- (v) No variant: `INSIN3`'s GIMPLE is the same under `STAB0` and `STAB3`; the called routines'
  variants are covered in their own reports.
- ⚠ CUDA compiled (`nvcc_wrapper`, `sm_89`, `--fmad=false`, the `#cuda` shell), never run; the
  routine is host code there too.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
