# SIG2WN_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Wavenumber `k` from radian frequency `σ` and depth `h` on the linear dispersion relation
`σ² = g k tanh(k h)`, by Newton iteration from the deep-water value `σ²/g`, used by the
Reichl flux parametrisations (switches `FLD1`, `FLD2`) to build the wavenumber grid and the tail.

## What was ported

| | |
|---|---|
| WW3 routine | `SIG2WN`, `model/src/w3fld1md.F90:1174-1247` at NOAA-EMC/WW3 `03adda91` (74 lines); no `#ifdef` inside it, and the preprocessed `w3fld1md.F90` is the same under `FLD1` and `FLD2` |
| Callers | `W3FLD1` (`w3fld1md.F90:286`, `:304`, `:311`, `:343`, `:345`) and `W3FLD2` (`w3fld2md.F90:294`, `:306`, `:312`, `:339`, `:341`) |
| Compiled by | `FLD1`: `ww3_ts3/input/switch_ST4_PR1_FLD1`, `switch_ST4_PR1_FLD1_MPI`, `switch_ST6_PR1_FLD1`, `switch_ST6_PR1_FLD1_MPI`; `FLD2`: `ww3_ts3/input/switch_ST4_PR1_FLD2(_MPI)`, `switch_ST6_PR1_FLD2(_MPI)`, `ww3_ufs1.1/input/switch_MPI`, `switch_MPI_OMPH`, `ww3_ufs1.1/input_unstr/switch_PDLIB`, `switch_PDLIB_TRNK`, `ww3_ufs1.2/input/switch_MPI_OMPH`, `ww3_ufs1.3/input/switch_MPI_OMPH`, `model/bin/switch_multi_esmf` (`git grep -lw FLD1 -- '*switch*'`, same for `FLD2`); no `switch_NCEP_*` or `switch_NWS_*` |
| Kokkos code | [`../src/ww_kokkos/fld1_sig2wn.hpp`](../src/ww_kokkos/fld1_sig2wn.hpp), `ww::fld1::sig2wn(sig, depth, wn&)` |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3fld1md.F90` recompiled with the flags of WW3's CMake Release
build (gfortran 16.2.0, pinned shell), identical to the CMake object of `build_mixa`
(`switch_lab_shrd` + `FLD2`) and of `build_fld1` (`ww3_ts3/input/switch_ST4_PR1_FLD1`); the
C++ unit compiled with the `openmp-release` flags, warning-free, 2026-10-16.

- (v) The two assemblies match instruction for instruction (SSE in the Fortran, VEX in the C++):
  `sig*sig/GRAV`, one `tanhf` before the loop, one `tanhf` and one `coshf` per iteration (gfortran
  and g++ both reuse the single `tanh(wn1*depth)` of the three Fortran sites), `(grav*wn1)*tanh - sig²`,
  `grav*tanh + (grav*wn1)*depth/cosh²`, `abs` by `andps`, the two `comiss` tests.
- (v) Constants: `GRAV` 9.806 (`constants.F90:62`) as `9.805999755859375`, 0.99 and 0.0001 as their
  `float` values, in both GIMPLE dumps. `cosh(...)**2` is `c*c` in both. No FMA, no `powf`, no libmvec.
- ⚠ No iteration cap (`:1204`): `SIG = 0` gives `0/0` in the first step and `DEPTH = 0` an infinite
  step then `tanh(inf*0)`; both leave a NaN that never passes the `< 0.0001` test, so the Fortran and
  the port loop forever. Reproduced, not fixed. A NaN input skips the loop (`tanh(NaN) < 0.99` is false).
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3fld1md.F90` with the `flags.make` of
  `build_mixa` and `build_fld1` (both identical to the CMake objects, `SIG2WN` assembly the same under
  `FLD1` and `FLD2`); the `-O3` GIMPLE of the Fortran and of the C++ unit match statement for statement
  (same constants `9.805999755859375`, `0.99f`, `1e-4f`, same `tanhf`/`coshf` sites, same operand order),
  and the assemblies match instruction for instruction with the same `.rodata` bits. Fixed in the report:
  the FLD2 switch list lacked `ww3_ufs1.1/input_unstr/switch_PDLIB_TRNK`, `ww3_ufs1.2` and `ww3_ufs1.3`;
  fixed in the header: `DEPTH = 0` gives an infinite first step, then NaN.
- ⚠ On a device `tanhf` and `coshf` are CUDA's, not glibc's. CUDA not compiled for this unit.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
