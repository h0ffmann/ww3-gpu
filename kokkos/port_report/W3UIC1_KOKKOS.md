# W3UIC1_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the `W3WAVE` wiring first.

Ice thickness of each sea point from the input field: `ICEH(ISEA) = MAX(IICEHMIN, IICEHFAC*ICEP1(IX,IY))`,
and the valid time `TIC1 = TI1`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3UIC1`, `model/src/w3updtmd.F90:1555-1659` at NOAA-EMC/WW3 `03adda91` (105 lines), as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/updt_w3uic1.hpp`](../src/ww_kokkos/updt_w3uic1.hpp): `ww::updt::w3uic1_point` (device function) and the launcher `ww::updt::w3uic1` |
| C shim | none yet |
| Caller | `W3WAVE`, `model/src/w3wavemd.F90:1381`, when the ice-thickness input (`IC1`, `INFLAGS2(-7)`) is due |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

Which regtests feed `IC1` and so reach it: `ww3_tic1.1` to `ww3_tic1.4`, `ww3_tic2.1` to
`ww3_tic2.3`, `ww3_tp2.14`, `mww3_test_08` (the `IC1` shel variants) and `ww3_ts1`, found by
`grep -rlE "ICE_PARAM1 *= *'?(T|H|C)" WW3-noaa/regtests` on their `ww3_shel.nml` ⚠ (not
checked that each run reads the field). `w3updtmd.F90` is compiled under every switch.

## Arithmetic

The reference is the `-O3` object of `w3updtmd.F90` (WW3's CMake Release flags, gfortran 16.2,
inside the pinned `ww3` shell, byte-identical to the object of the `switch_lab_shrd` CMake
build), read as GIMPLE and assembly, 2026-10-16.

- (v) `:1647` is, per point, one scalar `mulss` with `ICEP1(IX,IY)` in the register and
  `IICEHFAC` from memory, then `maxss` with `IICEHMIN` from memory: the result is
  `prod > IICEHMIN ? prod : IICEHMIN`, so a NaN product or a tie gives `IICEHMIN`. The C++
  (`fmax2(iicehmin, icep1*iicehfac)`) compiles with the `openmp-release` flags to the same
  `vmulss` (`ICEP1` in the register) and `vmaxss` (`IICEHMIN` the second source) in the serial
  loop and in the OpenMP functor.
- (v) The loop `:1643-1648` is not vectorised in the Fortran object (`-fopt-info-vec` has no
  line there) nor in the C++ functor. No libm call, no FMA (contraction off), no reduction.
- (v) No `#ifdef` changes the arithmetic: `W3_T` only prints `TIME`, `TIC1`, `TI1`
  (`:1635-1637`), which a kernel does not. `FLFRST` is not read (`:1547`).
- `TIC1(1:2) = TI1(1:2)` (`:1638-1639`) is integer copying, done on the host by the launcher.
- (v) Fresh review 2026-10-16: no defect. `w3updtmd.F90` recompiled from `build_lab`'s
  `flags.make` in the pinned shell (gfortran 16.2.0), disassembly identical to the CMake object;
  from the relocations, `mulss` takes `IICEHFAC` from memory with `ICEP1(IX,IY)` as first source
  and `maxss` takes `IICEHMIN` from memory, and the C++ `Kokkos::OpenMP` functor does the same
  (`vmulss` of the captured `iicehfac` onto `ICEP1`, `vmaxss` with `iicehmin` second, in both
  its stride-1 and general-stride loops). The `W3UIC1` instruction stream is the same in the
  `mixa`, `mixb`, `smc`, `nl5`, `fld1`, `nl3` and `setup` reference builds (addresses aside), so
  no switch of those changes it. `MAPSF(MSEA,3)` with `MSEA = NSEA` (`w3gdatmd.F90:1736`,
  `:1892`) and `ICEH(0:NSEA)` (`w3wdatmd.F90:567`) match the launcher's views.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a throwaway translation unit with
the `openmp-release` flags compiled the header with no warning, and its object was compared
instruction by instruction with the Fortran's for `:1647`. Not compiled for CUDA.
