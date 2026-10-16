# WKFNC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Wavenumber `k` (rad/m) of the TSA nonlinear transfer (switch `NL4`) from frequency `f` (Hz) and
depth `d` (m), by a rational approximation of the inverse of `σ² = g k tanh(kd)`, `σ = 2π f`:
with `y = σ² d / g`, `(kd)² = y (y + 1/(1 + y(0.66667 + y(0.35550 + ... + y·0.00011))))`, `k = kd/d`,
evaluated in double precision with `g = 9.806`. Callers: `INSNL4` (`w3snl4md.F90:474`, `:518`,
`:532`, building the depth tables on the first call of `W3SNL4`) and `shloxr` (`:2187`).

## What was ported

| | |
|---|---|
| WW3 routine | `wkfnc`, `model/src/w3snl4md.F90:5286-5369` at NOAA-EMC/WW3 `03adda91`, 84 lines; no `#ifdef` inside. Compiled by the `NL4` regtest switches (`ww3_ts1/input/switch_ST4_TSA`, eight `ww3_ts3/input/switch_ST4_TSA_PR3_*`; `git grep -lw NL4 -- '*switch*'`), by no NOAA operational switch |
| Kokkos code | [`../src/ww_kokkos/snl4_wkfnc.hpp`](../src/ww_kokkos/snl4_wkfnc.hpp), `ww::snl4::wkfnc(f, dep, twopi)`; `twopi` is the module variable `w3snl4md:twopi` (`:142`), set to `TPI` in `W3SNL4` (`:872`), passed in |
| C shim | none yet |
| Plan | issue #42, easy batch |

## Arithmetic

Reference: the `-O3` object of `w3snl4md.F90` from WW3's CMake Release build (gfortran 16.2,
`switch_lab_shrd` with `NL4 IC5 BT9 NLS`, `build_mixb`), recompiled with
`-fdump-tree-optimized-lineno` and checked identical to the CMake object; the C++ unit compiled
with the `openmp-release` flags and `-ffp-contract=off`, warning-free, 2026-10-16.

- (v) Same operations in the same order: `(f*twopi)²` as one `mulss`, `* dep` in single
  precision, widened, `/ g` in double; the Horner chain `y*0.00011 + 0.00039`, then `*y + c` eight
  times down to `+ 1`, `1/p`, `+ y`, `* y`, `sqrt`, `/ dble(dep)`, rounded to single. No FMA in
  either object.
- (v) Every literal is a default `REAL` widened to `REAL(8)`, including `g = 9.806`
  (`9.805999755859375`); the C++ builds each as a `float` literal cast to `double`, and the ten
  double constants in the GIMPLE of both units are the same values.
- (v) `sqrt` is `sqrtsd` in the Fortran object. The C++ object has `vsqrtsd` plus g++'s errno
  fallback call to glibc `sqrt` for a negative argument; ⚠ that path (only for `dep < 0`, or
  `f*twopi` NaN) returns NaN in both, its sign bit not compared.
- ⚠ `dep = 0` gives `0/0` = NaN in both; WW3 has no guard, the port adds none.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. `w3snl4md.F90` recompiled from `build_mixb`'s flags and
  found identical to the CMake object; the C++ unit recompiled warning-free; the two disassemblies
  compared instruction by instruction (the same 9 Horner steps, operand order of every
  `mul`/`add`/`div` so a NaN operand propagates the same way, `cvtss2sd` placement, `sqrtsd`, the
  final `cvtsd2ss`) and the eleven double constants of both GIMPLE dumps checked equal. The one
  difference is g++'s errno branch to `sqrt` when `x < 0`, already marked ⚠ above. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
