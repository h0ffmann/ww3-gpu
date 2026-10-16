# CGFNC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Linear group velocity of the TSA nonlinear transfer (switch `NL4`), from frequency `f` (Hz),
depth `dep` (m) and phase speed `cvel` (m/s), with `kd = 2π f d / c`:
`Cg = 0.5 c (1 + kd (1 - tanh²kd) / tanh kd)`, the identity `2kd/sinh 2kd = kd(1 - tanh²kd)/tanh kd`
avoiding `sinh` overflow at large `kd`. Its only caller is `INSNL4`, once per depth of the depth table
(`w3snl4md.F90:477`), building the TSA look-up tables on the first call of `W3SNL4`.

## What was ported

| | |
|---|---|
| WW3 routine | `cgfnc`, `model/src/w3snl4md.F90:5396-5469` at NOAA-EMC/WW3 `03adda91`, 74 lines; no `#ifdef` inside. Compiled by the `NL4` regtest switches (`ww3_ts1/input/switch_ST4_TSA`, eight `ww3_ts3/input/switch_ST4_TSA_PR3_*`; `git grep -lw NL4 -- '*switch*'`), by no NOAA operational switch |
| Kokkos code | [`../src/ww_kokkos/snl4_cgfnc.hpp`](../src/ww_kokkos/snl4_cgfnc.hpp), `ww::snl4::cgfnc(f, dep, cvel, twopi)`; `twopi` is the module variable `w3snl4md:twopi` (`:142`), set to `TPI` in `W3SNL4` (`:872`), passed in |
| C shim | none yet |
| Plan | issue #42, easy batch |

## Arithmetic

Reference: the `-O3` object of `w3snl4md.F90` from WW3's CMake Release build (gfortran 16.2,
`switch_lab_shrd` with `NL4 IC5 BT9 NLS`), recompiled with `-fdump-tree-optimized-lineno` and
checked identical to the CMake object; the C++ unit compiled with the `openmp-release` flags and
`-ffp-contract=off`, 2026-10-16.

- (v) Same operations in the same order: `wkd = ((f*twopi)*dep)/cvel`, `tkd = tanhf(wkd)`,
  `tkd*tkd` (`tkd**2` expanded to one multiply), `1 - tkd²`, `* wkd`, `/ tkd`, `+ 1`, `* (cvel*0.5)`.
  Only operand order of commutative multiplies differs (same bits). No FMA in either object.
- (v) `tanhf` is a scalar call of glibc's `tanhf` (`R_X86_64_PLT32 tanhf`) in both objects; no libmvec.
- (v) The Fortran parentheses (`PAREN_EXPR` around `1.-tkd**2` and `1.+...`) match the C++
  evaluation order; g++ did not reassociate anything (GIMPLE compared).
- ⚠ `wkd = 0` (f or dep zero) gives `0/0` = NaN in both; WW3 has no guard, the port adds none.
- ⚠ CUDA not compiled. On a device `Kokkos::tanh` is not shown to return glibc's bits.
- (v) Fresh review 2026-10-16: no defect. The `w3snl4md` object recompiled from `build_mixb`'s `flags.make` is identical to the CMake object; its `cgfnc` GIMPLE and assembly compared with a C++ unit of the header, operation by operation: same multiplies, divides, subtract and add in the same order (only commutative operands swapped), `tanhf@PLT` in both, constants 1.0 and 0.5 as single precision, no FMA; no `#ifdef` in the routine; caller `:477` in `INSNL4`, switches by `git grep -lw NL4`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
