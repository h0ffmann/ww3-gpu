# DSVBKSB_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Back-substitution of a singular value decomposition, x = V diag(1/w) U^T b with 1/0 taken as 0
(Numerical Recipes `svbksb`, double precision), in the least-squares fit of tidal constituents
(switch `TIDE`). Its only caller is `SVD` (`w3tidemd.F90:1415`), called by
`flex_tidana_webpage` (`:2308`), called by the forcing preprocessor `ww3_prnc`
(`ww3_prnc.F90:1675`). It never runs inside `ww3_shel` or `ww3_multi`.

## What was ported

| | |
|---|---|
| WW3 routine | `DSVBKSB`, `model/src/w3tidemd.F90:995-1019` at NOAA-EMC/WW3 `03adda91`; `w3tidemd` is compiled by every switch, `DSVBKSB` is reached only through `ww3_prnc` with `TIDE` (`ww3_tp2.18/input/switch_TIDE`, `switch_TIDE_MPI`; `model/bin/switch_Ifremer1`, `switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OASACM`, `switch_OASICM`, `switch_OASOCM`, `switch_ite_pdlib`; `git grep -lw TIDE -- '*switch*'`), no NCEP or NWS operational switch. No `#ifdef` in the routine |
| Kokkos code | [`../src/ww_kokkos/tide_dsvbksb.hpp`](../src/ww_kokkos/tide_dsvbksb.hpp), `ww::tide::dsvbksb`; `U(MP,NP)` and `V(NP,NP)` column-major with leading dimensions `MP` and `NP`; `TMP(NMAX=500)` stays a local array, as in the Fortran (not `SAVE`) |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3tidemd.F90` (WW3's CMake Release flags, gfortran 16.2) in
`build_mixa` (`switch_lab_shrd` plus `TIDE`) and `build_lab` (`switch_lab_shrd`), recompiled and
found identical to both CMake objects; the `DSVBKSB` code is the same in both builds. Compared with
the GIMPLE and object of a C++ unit compiled with the openmp-release flags, 2026-10-16.

- (v) Both sums are in index order in both objects: gfortran packs two products `u(i,j)*b(i)`
  (and `tmp(jj)*v(j,jj)`) into one `mulpd` and adds the lanes to `s` one after the other; g++
  does the same with four lanes. A lane multiply is the scalar multiply, so the bits are those of
  the scalar loop. The product's operand order differs in the second loop (`tmp*v` in the
  Fortran object); a double multiply commutes exactly.
- (v) `s/w(j)` is one `divsd` where `w(j) != 0` (a NaN `w` divides, as with `ucomisd`/`jp`);
  `tmp(j) = +0` otherwise. No FMA (no `vfmadd` in either object), no reciprocal, no libm call.
- ⚠ `n > 500` overruns `TMP` in the Fortran and in the port; the caller passes
  `N2 = NMAXP1 = 140` (`:93`, `:2308`), so it does not happen in WW3.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3tidemd.F90` from the `build_mixa` and
  `build_lab` flags (identical to both CMake objects, and `DSVBKSB` identical between them) and
  compared the `-O3` GIMPLE and object with a C++ unit under the openmp-release flags: both sums
  start from `+0` and add each product in index order (two lanes in the Fortran, four then two in
  the C++, scalar tails in order), one `divsd` per nonzero `w(j)` (also when `m <= 0`), no FMA, no
  libm call (the only `call` in either is `__stack_chk_fail`); call chain and switch list
  re-read with `git grep` at `03adda91`. Nothing changed. ⚠ Where the C++ scalar tail writes
  `v*tmp` and the Fortran `tmp*v`, two NaN operands would give a NaN of another payload; no
  finite or single-NaN input differs.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
