# WAVNU3_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Wavenumber and group velocity from frequency and depth by an explicit approximation, the one
`PDLIB` builds use in place of `WAVNU1`.

## What was ported

| | |
|---|---|
| WW3 routine | `WAVNU3`, `model/src/w3dispmd.F90:348-454` at NOAA-EMC/WW3 `03adda91`; no `#ifdef` inside. Called only under `W3_PDLIB`: `W3INIT` (`w3initmd.F90:1415,1429`), `W3ULEV` (`w3updtmd.F90:2361,2378`), `PDLIB_EXPLICIT_BLOCK` (`w3profsmd_pdlib.F90:6513`); its call in `W3SNL3` (`w3snl3md.F90:398`) needs `NL3` with `PDLIB`, in no switch file |
| Kokkos code | [`../src/ww_kokkos/disp_wavnu3.hpp`](../src/ww_kokkos/disp_wavnu3.hpp), `wavnu3(si, h, k, cg)`, on `disp_distab.hpp`'s constants and `tanhf`/`sinhf` wrappers |
| C shim | none yet |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch B, issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3dispmd.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd`), read by the translator and a fresh reviewer, 2026-10-16.

- (v) `TP` a true division, `KH0 = (H*ZPI2/GRAV)*TP*TP`, `TMP = (KH0*1.3+1.55)+(KH0*0.216)*KH0`;
  `powf`, `expf`, `tanhf`, `sinhf` called in the Fortran's order, `sqrtss`, `2KH` as `addss`,
  `CG = ((2KH/S+1)*0.5)*SI/K`; `ZPI2` folded to `0x421de9e7`; no FMA, no vectorisation.
- (v) The three `MIN` sites have different operand orders in the object (`TMP<20?TMP:20`,
  `20<KH0?20:KH0`, `20<2KH?20:2KH`) and the header reproduces each, NaN included.
- ⚠ On CUDA, `powf`, `expf`, `tanhf`, `sinhf` are not shown to return glibc's bits; compiled for
  `sm_89` with `--fmad=false`, never run.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
