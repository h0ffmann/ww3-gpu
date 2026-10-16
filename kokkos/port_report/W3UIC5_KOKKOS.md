# W3UIC5_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the `W3WAVE` wiring first.

Ice floe diameters of each sea point from the input field: where `ICE(ISEA) = 0` or
`ICEH(ISEA) = 0`, `ICEF(ISEA) = 0` and `ICEDMAX(ISEA) = 1000`; elsewhere both take
`ICEP5(IX,IY)`. The valid time is copied, `TIC5 = TI5`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3UIC5`, `model/src/w3updtmd.F90:1670-1786` at NOAA-EMC/WW3 `03adda91` (117 lines), as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/updt_w3uic5.hpp`](../src/ww_kokkos/updt_w3uic5.hpp): `ww::updt::w3uic5_flfloe` (device function) and the launcher `ww::updt::w3uic5` |
| C shim | none yet |
| Caller | `W3WAVE`, `model/src/w3wavemd.F90:1407`, only under `W3_IS2` (`:1392`) and when the floe-size input (`FLIC5`, `INFLAGS1(-3)`, `w3idatmd.F90:565`) is on |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

`w3updtmd.F90` is compiled under every switch, but the call needs `IS2`. Regtests whose
switch has `IS2` and whose `ww3_shel.nml` sets `ICE_PARAM5`: `ww3_tic1.1` (`input_IS2`,
`input_IC2_ifr`), `ww3_tic2.3`, `ww3_tp2.14` (`input_oasicm`), found by
`git -C WW3-noaa grep -lw IS2 -- '*switch*'` and `git -C WW3-noaa grep -l ICE_PARAM5 -- regtests`
⚠ (not checked that each run reads the field). No `switch_NCEP_*` or `switch_NWS_*` in
`model/bin` has `IS2`.

## Arithmetic

The reference is the `-O3` object of `w3updtmd.F90` (WW3's CMake Release flags, gfortran 16.2,
inside the pinned `ww3` shell, byte-identical to the object of the `switch_lab_shrd` CMake
build), read as GIMPLE and assembly, 2026-10-16.

- (v) No floating-point arithmetic. Per point the object does `ucomiss` of `ICE(ISEA)` against
  0, and only when that is not equal (a NaN is not equal) `ucomiss` of `ICEH(ISEA)` against 0:
  `.OR.` short-circuits. A -0 equals 0. The C++ (`ice == 0 || iceh == 0`) compiles with the
  `openmp-release` flags to the same two `vucomiss`, in the same order, in the serial loop and
  in the OpenMP functor.
- (v) `ICEP5(IX,IY)` is loaded once (`movss`) and stored to `ICEF(ISEA)` then `ICEDMAX(ISEA)`,
  so the value is copied bit for bit, NaN payload included; the C++ does one `vmovss` load and
  two stores in that order. `MAPSF` is read only on that branch in both.
- (v) The loop `:1761-1773` is not vectorised in the Fortran object (`-fopt-info-vec` has no
  line there) nor in the C++. No libm call, no reduction, no MIN/MAX.
- (v) No `#ifdef` changes the arithmetic: `W3_T` only prints `TIME`, `TIC5`, `TI5`
  (`:1753-1755`), which a kernel does not. The `W3UIC5` code of the `lab + NL4 IC5 BT9 NLS`
  build is the same instruction stream as under `switch_lab_shrd` (only its offset in the
  object differs). `FLFRST` is not read.
- `TIC5(1:2) = TI5(1:2)` (`:1756-1757`) is integer copying, done on the host by the launcher.
- (v) Fresh review 2026-10-16: no defect. `w3updtmd.F90` recompiled from `build_lab`'s
  `flags.make` in the pinned shell (gfortran 16.2.0), disassembly identical to the CMake object;
  from the relocations the first `ucomiss` reads `ICE` and the second `ICEH`, each skipped to the
  next test on parity (NaN), as in the C++ `Kokkos::OpenMP` functor; the no-floe path stores
  `+0` and `0x447a0000` (1000) in both, the other path one load of `ICEP5(IX,IY)` and two
  stores. The `W3UIC5` instruction stream is the same in the `mixa` (which has `IS2`, the switch
  of its caller), `mixb`, `smc`, `nl5`, `fld1`, `nl3` and `setup` reference builds (addresses
  aside).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a throwaway translation unit with
the `openmp-release` flags compiled the header with no warning, and its object was compared
instruction by instruction with the Fortran's for `:1761-1773`. Not compiled for CUDA.
