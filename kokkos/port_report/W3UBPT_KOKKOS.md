# W3UBPT_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12).

Spectra at the active boundary points: the space interpolation, with the four weights `RDBPI`, of
the two boundary spectra `W3IOBC` read (`ABPI0` at `TBPI0`, `ABPIN` at `TBPIN`), converted from
sigma spectra by `CG/SIG2`, into `BBPI0` and `BBPIN`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3UBPT`, `model/src/w3updtmd.F90:1329-1545` at h0ffmann/WW3 `96892c95` (217 lines), byte-identical to NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (no `PDLIB`: the `CG(MAPWN(ISP),ISEA)` branches `:1461-1465`, `:1487-1491`) |
| Kokkos code | [`../src/ww_kokkos/updt_ubpt.hpp`](../src/ww_kokkos/updt_ubpt.hpp): device function `w3ubpt_point` and the launcher `ww::updt::w3ubpt<ExecSpace>` |
| C shim | none yet |
| Caller | `W3WAVE`, `model/src/w3wavemd.F90:1344` at the pin, after `W3IOBC('READ')` |

Not compiled under the lab switch and not ported: `PDLIB` (no `CG` factor), `RTD` (the
`W3ACTURN` rotation, `:1496-1506`), `T0` (a wave-height print, `:1513-1532`), `S`.

The test of `:1445` (`BBPI0(1,0) .EQ. -1.`) reads one `REAL` of `BBPI0` back to the host; the
boundary arrays are not `VA`, and nothing else crosses.

## Arithmetic

Reference and method as in [`W3UCUR_KOKKOS.md`](W3UCUR_KOKKOS.md) (the same recompiled object,
equal to the CMake build's, 2026-11-12).

- (v) `:1461-1465` and `:1487-1491` are `(CG/SIG2) * PAREN(((R1*A1 + R2*A2) + R3*A3) + R4*A4)`:
  the GIMPLE keeps the parentheses (`PAREN_EXPR`), and the object has 18 scalar FP instructions
  in the routine, nine per loop (one `divss`, four `mulss`, three `addss` left to right, one
  `mulss`). The C++ kernels emit the same nine per point, scalar, in the same order; the sums,
  `CG/SIG2` and the last product have the Fortran's operand positions.
- ⚠ The four `RDBPI*ABPI` products: the Fortran has `RDBPI` as the first operand (kept in a
  register over the `ISP` loop), the C++ kernel `ABPI` (writing `a*r` in the source, or iterating
  `ISP` innermost, does not change the object). The result differs only in the payload of a
  product of two NaNs, a NaN weight and a NaN spectrum value; whether `RDBPI` can be NaN was not
  checked.
- (v) `:1447-1450` the column-0 fills and `:1473` `BBPI0 = BBPIN` (through a `malloc`ed temporary
  in the Fortran) copy bits; the C++ uses `deep_copy`.
- (v) No loop of `:1329-1545` is vectorised; no FMA in either object.
- ⚠ No libm call; on CUDA the arithmetic is IEEE with `--fmad=false`, not compiled for CUDA here.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested, postponed to the validation phase (ADR-0016). Not wired: no shim, no caller hunk.
Compiled with no warning under the `openmp-release` flags and at `-O0 -g
-fsanitize=address,undefined` (v, 2026-11-12); nothing was run.

## WW3 defects found

None.
