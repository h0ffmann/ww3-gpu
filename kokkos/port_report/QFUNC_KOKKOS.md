# QFUNC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The first function of the generalized kinetic equation module of the `NL5` nonlinear source
term (switch `NL5`, `W3GKEMD`, Gramstad and Stiassnie 2013): `q = ω²/g = k tanh(kd)` for a
wavenumber vector `(k_x, k_y)` and the module's water depth `d`, the `q` of the Zakharov kernel
(Krasitskii 1994; Janssen 2009, appendix A). For `d ≤ ε` or `d ≥ 3000 m` it returns `q = |k|`.
`VpFunc`, `VmFunc`, `UFunc`, `TFunc`, `FindQuartetNumber` and `FindQuartetConfig` call it.

## What was ported

| | |
|---|---|
| WW3 routine | `QFunc`, `model/src/w3gkemd.F90:234-271` at NOAA-EMC/WW3 `03adda91` (38 lines), private to `W3GKEMD`. Callers in the module: `:299-301`, `:341-343`, `:382-392`, `:452-468`, `:699`, `:822`; none outside it. Compiled by the switches of `ww3_ts1/input_nl5_gaussian`, `input_nl5_growth`, `input_nl5_matrix` (`git grep -lw NL5 -- '*switch*'`), by no NOAA operational switch. No `#ifdef` inside it |
| Kokkos code | [`../src/ww_kokkos/gke_qfunc.hpp`](../src/ww_kokkos/gke_qfunc.hpp), `ww::gke::qfunc(kx, ky, qr_depth)`; the module variable `qr_depth` (`:165`, set from `QR5DPT` at `w3snl5md.F90:323`, `:515`, clipped at `:1256`) is an argument. `kQrEps` and `kQrDmax` are the module's `qr_eps` (`:193`) and `qr_dmax` (`:201`) |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3gkemd.F90` (WW3's CMake Release flags, gfortran
16.2, `switch_lab_shrd` with `NL5`, object identical to the CMake build's), against a throwaway
C++ unit with the openmp-release flags, read operation by operation, 2026-10-16.

- (v) The out-of-line `__w3gkemd_MOD_qfunc` and the C++ object run the same sequence: `kx*kx`,
  `ky*ky`, add, `sqrtss`; ordered `comiss` with `qr_eps` (`0x34000000`, `EPSILON(100.0)`) then
  with `qr_dmax` (`0x453b8000`, 3000); `q*d`; scalar `tanhf`; `q*tanh`. The same `.rodata`
  words in both objects. A NaN depth takes the deep-water branch in both.
- (v) Nothing calls `__w3gkemd_MOD_qfunc`: gfortran inlines its 26 calls (39 inlined copies in
  the GIMPLE). The copies read (in `FindQuartetConfig`, `FindQuartetNumber`, `UFunc`) are the
  same sequence, some with the commutative `d*q` written `q*d`. ⚠ Not every copy was read; a
  caller's own translation checks its inlined copies.
- (v) libm: scalar `tanhf` only, no libmvec variant anywhere in the object (27 `tanhf` calls).
  No reduction, no MIN/MAX, no FMA. Compiled warning-free.
- (v) CUDA compiled (`nvcc_wrapper`, sm_89, `--fmad=false`), compile only. ⚠ On a device
  `Kokkos::tanh` is CUDA's, not glibc's `tanhf`, and its SASS has `FFMA` from the math library
  despite `--fmad=false`: not shown to give the same bits.
- (v) Fresh review 2026-10-16: no defect. `w3gkemd.F90` recompiled under `build_nl5`'s flags (object
  identical to the CMake one), against a fresh C++ unit with the scalars passed by value: same
  `mulss`, `mulss`, `addss`, `sqrtss`, ordered `comiss` on `0x34000000` and `0x453b8000`, `d*q`,
  scalar `tanhf`, `q*tanh`; no relocation to `__w3gkemd_MOD_qfunc`, 27 `tanhf` relocations (26
  call sites plus the out-of-line copy), no `_ZGV*`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
