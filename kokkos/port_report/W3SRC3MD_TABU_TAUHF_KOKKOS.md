# W3SRC3MD_TABU_TAUHF_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The table of the high-frequency wave-supported stress of the WAM4+ input term (switch `ST3`),
`TAUHFT(0:IUSTAR,0:IALPHA)` of `W3SRC3MD`, with its steps `DELUST` and `DELALP`. `INSIN3` builds it
once per grid with `FRMAX = SIG(NK)*TPIINV` (`w3src3md.F90:809`); `W3SIN3` reads it. Not the ST4
routine of [`TABU_TAUHF_KOKKOS.md`](TABU_TAUHF_KOKKOS.md): ST3's argument is a frequency, and
`OMEGAC = TPI*FRMAX` (`:1061`).

## What was ported

| | |
|---|---|
| WW3 routine | `TABU_TAUHF`, `model/src/w3src3md.F90:958-1116` at NOAA-EMC/WW3 `03adda91`; module parameters and tables `:95-99`. Its only `#ifdef`s are `W3_S` and `W3_T` (trace and test output) |
| Kokkos code | [`../src/ww_kokkos/srce_st3_tabu_tauhf.hpp`](../src/ww_kokkos/srce_st3_tabu_tauhf.hpp), `ww::srce::st3::tabu_tauhf(frmax, aalpha, bbeta, zzalp, zz0max, tauhft)`, an `inline` host function returning `TauhfSteps{delust, delalp}`; `TAUHFT(k,l)` is `tauhft[k + 101*l]` |
| Work array | `W(JTOT)`, which the Fortran `ALLOCATE`s per call (`:1065`), is a local `std::array<Real, 250>`: a host table builder, not a kernel |
| C shim | none yet; it keeps the table and both steps per grid, as `INSIN3` runs per mod_def read |
| Which regtests compile it | every switch with `ST3`: `ww3_ts1` (1), `ww3_ts2` (5), `ww3_ts3` (20), `ww3_tnc1` (1), `mww3_test_05` (20); no `switch_NCEP_*` or `switch_NWS_*` |
| Plan | issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3src3md.F90` (WW3's CMake Release flags, gfortran 16.2) under
`ST3 STAB0 FLX0` and `ST3 STAB3 FLX0`, both recompiled with `-fdump-tree-optimized-lineno` and
identical to the CMake objects (v); the C++ unit compiled with the `openmp-release` flags and
`-ffp-contract=off`, warning-free, and read against it operation by operation, 2026-10-16.

- (v) No variant: `TABU_TAUHF`'s disassembly is the same under `STAB0` and `STAB3`; no `FLX` or other
  switch reaches it.
- (v) Folded constants with the object's bits: `DELUST = 5./100.` stored as `0x3d4ccccd` and used as
  the `UST` step, `KAPPA**2 = 0x3e23d70b`, `X0*GRAV = 0x3efb089a`, `TPI = 0x40c90fdb`; each a
  `static_assert` of how it is built.
- (v) Every operation in the object's order: `ZZ00 = UST*UST*AALPHA/GRAV`,
  `ZZ0 = ((L*DELALP)/AALPHA + 1)*ZZ00`, `YC = OMEGACC*SQRT(ZZ0/GRAV)`, `OMEGA = SQRT(GRAV/ZZ0)*Y`,
  `(GRAV*ZZ0)/(CM*CM)*EXP(ZARG)`, `ZLOG**4` as `(ZLOG*ZLOG)**2`,
  `ZBETA = CONST1*ZMU*ZLOG**4`, `TAUHFT(K,L) + ((W(J)*ZBETA)/Y)*DELY` summed in J order. Only
  commutative operands differ; g++ hoists the `L` factor of `ZZ0` out of the `K` loop, the same value
  the Fortran recomputes per `K`. No FMA, no vectorised arithmetic (only the fill of `W` is vectorised,
  in both).
- ⚠ The commutative swaps are three: `ZZ0` (`ZZ00*f` against the object's `f*ZZ00`), `OMEGA`
  (`Y*SQRT` against `SQRT*Y`) and the sum (`term + TAUHFT` against `TAUHFT + term`). On x86 a sum or
  product of two NaNs returns the first operand's NaN, so these sites can differ in the payload or
  sign of a NaN when both operands are NaN with different payloads; a finite result is the same.
  It needs a NaN `AALPHA`, `FRMAX`, `BBETA` or `ZZ0MAX`, or a negative `AALPHA`, and the table is
  NaN there in both.
- (v) libm calls matched by relocation: `expf` 1 and `logf` 1 in each; `SQRT` is `sqrtss` in each.
- (v) The 7 `MIN`/`MAX` sites with the object's operand order (`minss`/`maxss`, the expression as
  destination, the constant, `OMEGAC` or `ZZ0MAX` as source, so a NaN gives the source); the
  `ZZ0MAX .NE. 0` test is taken for a NaN `ZZ0MAX` in both (`ucomiss`/`jp`). g++ builds the `ZLOG` site
  as `vcmpnltss`/`vandnps`, which gives the same result as `minss(log, 0)` for NaN and ±0.
- ⚠ g++ keeps `errno` (gfortran does not), so the C++ object calls `sqrtf` when `ZZ0/GRAV` or
  `GRAV/ZZ0` is negative, and for the second case g++ specialises the inner loop knowing `OMEGA` is
  NaN (`ZARG = 20`, `ZMU = 1`, `LOGF(1)` folded to 0). Argued equal bits, not compared: glibc's
  `sqrtf` and `sqrtss` both give the default NaN, and glibc's `logf(1)` is +0. The path needs
  `ZZ0 < 0`, which only a negative `AALPHA` gives.
- ⚠ CUDA compiled (`nvcc_wrapper`, `sm_89`, `--fmad=false`, kokkos-cuda 5.2.2 of the `#cuda` shell),
  never run; the routine is host code there too.
- (v) Fresh review 2026-10-16: no defect in the header. `w3src3md.F90` recompiled under `ST3 STAB0
  FLX0` and `ST3 STAB3 FLX0`, both identical to the CMake objects, `TABU_TAUHF`'s disassembly and
  preprocessed source the same under both; GIMPLE and assembly read against the C++ unit (warning-free,
  `openmp-release` flags) site by site: the 12 `.rodata.cst4` constants (20 references) by relocation, the 7
  `MIN`/`MAX` operand orders, the `ZLOG` mask, the `ZZ0MAX` NaN branch, `expf`/`logf`, the `sqrtf`
  error paths and their specialised inner loop (`ZBETA = CONST1*0`, the same bits as the
  Fortran's `(1*CONST1)*(+0)`), `INT` conversions, loop and sum order; the regtest switch counts
  re-read with `git grep`; CUDA compiled again. Added the NaN payload bullet above; nothing
  changed in the header.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
