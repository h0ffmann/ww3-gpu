# FD_SET_MEANVALUE_TO_ZERO_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Removes the area-weighted mean from a field on the `NX` points of a finite-difference grid:
`TheMean = SUM(SI*TheVar) / SUM(SI)`, then `TheVar = TheVar - TheMean`. It fixes the free constant
of the Neumann wave setup solution (`FD_WAVE_SETUP_COMPUTATION`, `w3wavset.F90:3043`).

## What was ported

| | |
|---|---|
| WW3 routine | `FD_SET_MEANVALUE_TO_ZERO`, `model/src/w3wavset.F90:2863-2943` at NOAA-EMC/WW3 `03adda91`. Called only from `FD_WAVE_SETUP_COMPUTATION` (`w3wavset.F90:3043`), reached from `W3WAVE` (`w3wavemd.F90:1124`, under `W3_SETUP`) through `WAVE_SETUP_COMPUTATION` (`w3wavset.F90:3159`, when `GTYPE /= UNGTYPE`). No regtest switch and no NCEP/NWS operational switch has `SETUP` (`git grep -lw SETUP -- '*switch*'` finds only `model/src/cmake/switches.json`), so no regtest exercises it. Its only `#ifdef` is `W3_S` (tracing, no arithmetic) |
| Kokkos code | [`../src/ww_kokkos/wavset_fd_set_meanvalue_to_zero.hpp`](../src/ww_kokkos/wavset_fd_set_meanvalue_to_zero.hpp), `ww::wavset::fd_set_meanvalue_to_zero(thevar, nx, si)`; `nx` and `si` are the module variables `W3GDATMD NX` and `SI(:)` passed in |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3wavset.F90` in WW3's CMake Release build (gfortran 16.2,
`regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP`), recompiled from the build's `flags.make`
with the disassembly identical to the CMake object, and a throwaway C++ unit with the
openmp-release flags (g++ 16.2, `-march=x86-64-v3 -ffp-contract=off`), 2026-10-16.

- (v) Kinds: `TheVar`, `SUM_SI_Var`, `SUM_SI`, `TheMean` are `REAL(rkind)`, `rkind = 4`
  (`PDLIB/yowdatapool.F90:48`); `SI` is `REAL(8)` (`w3gdatmd.F90:1125`). Each accumulator is
  widened to double, `SI*TheVar` (or `SI`) is added in double, and the sum is rounded back to
  float every step (`cvtss2sd`, `mulsd`, `addsd`, `cvtsd2ss` in the object). The C++ keeps the same
  casts and its GIMPLE is the same statement for statement.
- (v) Reduction order: both sums are scalar in `IP` order from `+0`, in the Fortran and the C++
  object (neither vectorises the first loop). The mean is one `divss`.
- (v) The second loop is element by element `TheVar - TheMean` (`subps`, 4 wide, in the Fortran;
  `vsubps`, 8 wide, in the C++), same operand order, so the width does not change the bits.
- (v) `NX <= 0`: the Fortran skips both loops and never divides; the C++ computes `0/0` and
  discards it, leaving `thevar` untouched as well.
- (v) No FMA, no libm or libmvec call in either object.
- ⚠ Operand order of `SUM_SI_Var + SI*TheVar`: the Fortran object adds with the accumulator as
  first operand (`addsd prod, sum`), the C++ (VEX) object with the product first
  (`vaddsd sum, prod, sum`); reordering the source did not change it. The sum differs only in which
  NaN payload survives when both operands are NaN. The multiply (`TheVar*SI`) and `SUM_SI + SI`
  have the Fortran's operand order.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect beyond the NaN-payload operand order above, which a source reordering does not move (confirmed again: the GIMPLE of both is `sum + product`, the swap comes from SSE two-operand vs VEX three-operand register allocation). Object recompiled from `ref3/build_setup` flags identical to the CMake object; GIMPLE and assembly compared with a new C++ unit: same widening, same scalar `IP`-order sums, one `divss`, element-wise subtract. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
