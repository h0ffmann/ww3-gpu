# DF1F2THETA_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Second-order coupling coefficient of the surface elevation between two wave components of radian
frequencies `s1`, `s2` and wavenumbers `WN1`, `WN2` at relative angle `theta` in depth `DEPTH`
(Okihiro et al. 1992, the source comment `w3gig1md.F90:75-79`; `C4` is "Hasselmann's D times i", `:135`), switch
`IG1`. Its only caller is `W3ADDIG` (`w3gig1md.F90:283-284`), which squares it into the
infragravity coupling table `DD`; `W3ADDIG` is called by `W3SREF` (`w3ref1md.F90:384`), `ww3_ounp`
(`ww3_ounp.F90:1785`, `:1959`) and `ww3_outp` (`ww3_outp.F90:1800`).

## What was ported

| | |
|---|---|
| WW3 routine | `Df1f2theta`, `model/src/w3gig1md.F90:60-142` at NOAA-EMC/WW3 `03adda91` (83 lines); compiled by the regtest switches of `ww3_tig1.1`, `ww3_tp1.7` (`input`, `input_OBST`) and `ww3_ts4` (`input_rg_multi`, `input_rg_shel`, `input_ug`, each with its MPI switch), and by `model/bin/switch_Ifremer2`, `switch_Ifremer2_pdlib` (`git grep -lw IG1 -- '*switch*'`); by no `switch_NCEP_*` or `switch_NWS_*`. It has no `#ifdef` block |
| Kokkos code | [`../src/ww_kokkos/gig1_df1f2theta.hpp`](../src/ww_kokkos/gig1_df1f2theta.hpp), `ww::gig1::df1f2theta`; all six dummies by value (`WN1`, `WN2` have no `INTENT` but are only read) |
| C shim | none yet; `W3ADDIG` calls it out of line (`call __w3gig1md_MOD_df1f2theta` in `w3gig1md.o`), so it is a port of its own only together with `W3ADDIG` |
| Plan | [`EASY_BATCH_KOKKOS_PLAN_202610.md`](../../docs/EASY_BATCH_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3gig1md.F90` from WW3's CMake Release build (gfortran 16.2,
`switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled with the same flags and
checked identical to the CMake object; compared with a C++ unit built with the `openmp-release`
flags (g++ 16.2, `-ffp-contract=off`, no warning), 2026-10-16.

- (v) Operation by operation, the optimised GIMPLE of both is the same sequence of `float`
  multiplies, adds, subtracts and divides with the same operands (only commuted operands of
  `*` and `+` differ): `2*k1*k2*co` as `((k1*2)*k2)*co`, `s**3` as `s*(s*s)`, `(s1*s2)**2` as
  one product squared, `C3` as `((s3**2-sk2)*s1)*s2`.
- (v) gfortran rewrites `C1 = -(k1*k2*co)/(s1*s2)` and `C1+C1b` as `C1b - (k1*k2*co)/(s1*s2)`
  (`:130`, `:139`); the C++ writes that form, so `C1` never exists as a value in either.
- (v) `GRAV**2` (`:124`) is folded to `0x42c050b5`; the C++ holds the same bits as a hex literal
  with a `static_assert` that it equals `GRAV*GRAV`. Both objects carry the same three `.rodata.cst4`
  constants (`GRAV**2`, `0.5`, `GRAV`).
- (v) libm: `cosf` and `tanhf` called in both; `SQRT` is `sqrtss` in the Fortran. g++ emits
  `sqrtss` with a call to `sqrtf` only for a negative argument (errno), which returns the same NaN.
- (v) No FMA, no reciprocal, no vectorisation, no MIN/MAX, no reduction in either.
- ⚠ `C3 = 0` (when `(s1+s2)**2 = sk2`, or `s1` or `s2` is 0) divides by zero as the Fortran does.
- ⚠ CUDA not compiled. On a device `cosf` and `tanhf` are CUDA's, not glibc's.
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3gig1md.F90` from `build_mixa`'s flags (identical to the CMake object) and the C++ unit with the `openmp-release` flags (no warning); the optimised GIMPLE is the same operation sequence, the same three `.rodata.cst4` constants, `cosf`/`tanhf` relocations in both, no FMA or reciprocal in the C++ assembly. Fixed one citation: the source comment is `:75-79` and does not cite Hasselmann 1962.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
