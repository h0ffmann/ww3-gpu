# INSNL5_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Initialisation of `NL5`, the generalized kinetic equation (GKE) for four-wave interactions: it
copies the five `NL5` parameters of the grid (`QR5DPT`, `QR5OML`, `QI5DIS`, `QI5KEV`, `QI5IPL`)
into `W3GKEMD`'s state, has `PrepKernelIO` build the wavenumber grid and kernel coefficients and
write them to `gkev?_d????.cfg`, and stores the quartet count in `QI5NNZ`. A host function, called
once by `W3IOGR` when it writes the mod_def (`w3iogrmd.F90:1761`, under `W3_NL5`).

## What was ported

| | |
|---|---|
| WW3 routine | `INSNL5`, `model/src/w3snl5md.F90:433-541` at NOAA-EMC/WW3 `03adda91`; compiled by the three `ww3_ts1` regtest switches with `NL5` (`input_nl5_gaussian`, `input_nl5_growth`, `input_nl5_matrix`; `git grep -lw NL5 -- '*switch*'`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`) |
| Kokkos code | [`../src/ww_kokkos/snl5_insnl5.hpp`](../src/ww_kokkos/snl5_insnl5.hpp), `insnl5()` (host, template on the `PrepKernelIO` callable) and `GkeConfig`, the six `W3GKEMD` module variables it sets and reads |
| Not ported | `CALL PrepKernelIO(NK, NTH, SIG(1:NK), TH, 'WRITE')` (`:522`, `w3gkemd.F90:1201`): it allocates the kernel arrays and writes a file, so the caller provides it as a callable that leaves `GkeConfig` as the Fortran leaves the module state; the `WRITE(SCREEN, ...)` block (`:529-537`), which the caller prints from `GkeConfig` after the call; `W3_S` tracing (`:510`), not in the reference switch |
| C shim | none yet; it passes `NK`, `NTH`, `SIG(1:NK)`, `TH(1:NTH)` (contiguous) and the five `QR5*`/`QI5*` inputs of the grid, writes `QI5NNZ` back, and prints the screen lines on `IAPROC == NAPOUT` |
| Translated before | none (checked `kokkos/src/ww_kokkos` and `kokkos/port_report`, 2026-10-16) |

## What WW3 does here at the pin

- (v) The screen lines (`:530-536`) print `qr_depth`, `qr_oml`, `qi_disc`, `qi_kev`, `qi_interp`
  after `PrepKernelIO` has clamped them (`w3gkemd.F90:1256-1260`), so they can differ from the
  `QR5DPT`, `QI5DIS`, `QI5KEV`, `QI5IPL` that `W3IOGR` writes to the mod_def next
  (`w3iogrmd.F90:1762-1763`), which are the unclamped inputs. Reproduced: `insnl5` writes back only
  `QI5NNZ`.
- (v) `SIG(1:NK)` and `TH` are passed through `_gfortran_internal_pack` (a copy when the pointer
  array is not contiguous, freed after the call): `PrepKernelIO` receives contiguous arrays, which is
  what the C++ takes.

## Arithmetic

Reference: the `-O3` object of `w3snl5md.F90` from WW3-noaa's CMake Release build, gfortran 16.2,
`switch_lab_shrd` with `NL1` replaced by `NL5`, recompiled with `-fdump-tree-optimized-lineno` and
identical to the CMake object, read by the translator, 2026-10-16. C++: g++ 16.2 `-O3
-march=x86-64-v3 -ffp-contract=off -fopenmp`, warning-free.

- (v) No floating-point operation in either object: the Fortran's `INSNL5` is five loads and stores
  (`:515-519`), the call, and a 64-bit copy of `qi_nnz` to `QI5NNZ` (`:526`); the C++ unit stores the
  same five values (the two `REAL` as one 8-byte store), calls the callable, and copies the
  `int64_t`. Bits are moved, never computed.
- (v) No libm, libmvec, reduction, `MIN`/`MAX` or `INT` in `INSNL5`; the clamps are in
  `PrepKernelIO`.
- ⚠ CUDA not compiled: a host function with no kernel.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3snl5md.F90` from `build_nl5`'s flags (object identical to the CMake one) and a new C++ unit (warning-free); the Fortran GIMPLE of `INSNL5` stores the five inputs into the module state before `PrepKernelIO`, then copies the `INTEGER(8)` `qi_nnz` to `QI5NNZ` (declared `INTEGER(KIND=8)`, `w3gdatmd.F90:959`), and the C++ does the same in the same order. The cited lines (`:510`, `:515-519`, `:522`, `:526`, `:530-536`, `w3iogrmd.F90:1761-1763`) were read at `03adda91`; the only `#ifdef` is `W3_S`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
