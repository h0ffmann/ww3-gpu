# HASNAN_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A diagnostic of the `NL5` nonlinear source term (switch `NL5`, `W3SNL5MD`): it tells whether a
spectrum `ARR2D(NK,NTH)` holds a value outside `[-HUGE, HUGE]`, that is a NaN or an infinity.
`W3SNL5` calls it twice at `w3snl5md.F90:389`, on `E(f,θ)` and `S_nl(f,θ)` of an output point,
only to print a warning; it changes no field.

## What was ported

| | |
|---|---|
| WW3 routine | `HasNaN`, `model/src/w3snl5md.F90:884-914` at NOAA-EMC/WW3 `03adda91` (31 lines); its only caller is `W3SNL5` (`:389`). Compiled by the switches of `ww3_ts1/input_nl5_gaussian`, `input_nl5_growth`, `input_nl5_matrix` (`git grep -lw NL5 -- '*switch*'`), by no NOAA operational switch. No `#ifdef` inside it |
| Kokkos code | [`../src/ww_kokkos/snl5_hasnan.hpp`](../src/ww_kokkos/snl5_hasnan.hpp), `ww::snl5::hasnan`; `arr2d` 0-based in Fortran column order, column stride `MAX(NK,0)` |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3snl5md.F90` (WW3's CMake Release flags, gfortran
16.2, `switch_lab_shrd` with `NL5`, object identical to the CMake build's), against a throwaway
C++ unit with the openmp-release flags, read operation by operation, 2026-10-16.

- (v) No floating-point arithmetic: two loops of ordered `comiss`, `x >= -HUGE` then `x <= HUGE`
  (`HUGE` as `0x7f7fffff`, the same `.rodata` words in both objects), each with an early exit on
  its first false, outer loop over `NTH`, inner over `NK`. A NaN fails both tests; `±Inf` fails
  one, so it also returns true.
- (v) gfortran does not short-circuit the `.AND.`: the second `ALL` is scanned even when the first
  was false, and the result is `.NOT.` of the first when the second is true. The C++ object has
  the same two loops and the same exits. The order has no effect on the value; it is kept.
- (v) `NTH <= 0` or `NK <= 0` gives false (`ALL` of an empty array is true), as in the object.
- (v) No libm, libmvec, reduction or MIN/MAX. Compiled warning-free.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3snl5md.F90` in the pinned shell (object identical to `build_nl5`'s) and the header in a throwaway unit; compared GIMPLE and assembly: same two column-major scans with the same exits, ordered `comiss`/`vcomiss` against the same `.rodata` words `0xff7fffff` and `0x7f7fffff`, `MAX(NK,0)` stride, empty-array and NaN/±Inf paths, result `.NOT.` of the first `ALL`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
