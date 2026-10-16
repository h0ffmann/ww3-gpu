# ST0, ST1, ST2, ST3, ST6, FLX1-5 and SWL translated to Kokkos, one agent per routine

2026-10-16, the owner with agents. Issue #42, PR #157 (`goal/42-port-st-flx-swl`).

## Question

With ST4 and the NOAA operational batches translated, every routine the committed profiles time
in compute is in C++ (`kokkos/PORT_RANKING.md`). The source-term packages the other switches
compile are not. Can all of them (`ST0`, `ST1`, `ST2` with `FLX3`, which is the physics of
`switch_NCEP_st2`, `ST3`, `ST6`, the `FLX1`-`FLX5` drag laws and the `SWL` swell dissipation) be
translated in one pull request to the bit-for-bit standard, with one translator and one fresh
reviewer per routine, and nothing run?

## What was done

- Eight WW3 CMake Release builds of `WW3-noaa` at `03adda91`, target `ww3_lib`, in the pinned
  shell (gfortran 16.2.0), each from `switches/switch_lab_shrd` with the physics keys swapped:
  ST0 FLX2 LN0, ST1 FLX1, ST2 STAB2 FLX3, ST3, ST3 STAB3, ST6 FLX4, ST6 FLX5, ST4 FLX5. The
  objects of every module concerned are the reference; every translator and reviewer recompiled
  its module from that build's `flags.make` and checked the object identical to the CMake one.
- 32 routines translated, one agent each, into 29 headers under `kokkos/src/ww_kokkos/`
  (`srce_st0_*` to `srce_st6_*`, `srce_flx1`-`5`, `srce_swl*`), with a port report each. Then a
  fresh agent per routine compared the C++ object (openmp-release flags, `-ffp-contract=off`) with
  the Fortran object again, operation by operation, and fixed only what it was sure of.
- Not run: no ctest, no regtest, no executable that calls a kernel, no GPU (the owner,
  2026-10-16). `just kokkos-build serial-debug` and `openmp-release` compile every header through
  `wave_support.cpp`; CUDA was compiled per routine in the `#cuda` shell, never run.

## Result

- (v) Static parity on the host for all 32: the same operations in the same order, the folded
  constants' bits, gfortran's power chains (`x*x*x`, `__powisf2` for an integer variable exponent
  in `W3SDS6` and `LFACTOR`), `MIN`/`MAX` operand order per site including the vector body and the
  scalar tail, libmvec where gfortran calls it (`_ZGVbN4vv_powf` in `W3SPR3` and the ST6 10 Hz
  tables, `_ZGVbN4v_expf` in `LFACTOR`, `_ZGVbN4v_tanhf` in `W3SIN6`), and libgfortran's
  `_gfortran_maxval_r4` in `W3SWL6`. The rows of `kokkos/ports.tsv` quote each review.
- (v) What the fresh reviews found in the port, each fixed and read again in the object:
  `W3FLX3` computed `(-0.4)/x` where the object computes `-(0.4/x)`, which flips the sign of a NaN
  `Z0`; the ST3 `TABU_STRESS` and `CALC_USTAR` headers could not be compiled together (a name
  clash); `INSIN3` and `W3SDS6` described module tables as per grid when WW3 builds them once per
  process. The other reviews found no arithmetic defect; several corrected line citations.
- (v) What they found in WW3 at `03adda91` (read from the source and the objects):
  `W3FLX2`/`W3FLX3` read `EWN1(INT_MIN)` when `DEPTH` is negative or NaN (no lower bound,
  `w3flx2md.F90:198`); `W3SDS2`'s `IKHW` has no lower bound (`w3src2md.F90:713`); `W3SWL4` has no
  caller; the guard `#ifdef W3_OMPG || W3_OMPH` (`w3src6md.F90:106`) tests `W3_OMPG` alone; the
  ST3, ST6 and SWL tables are SAVE, built by the first grid and reused by every other grid of a
  `ww3_multi` run. The port reproduces each of these, ⚠ in its report.
- ⚠ On a GPU none of these is shown bit for bit: no libmvec there, and CUDA's `expf`, `logf`,
  `powf`, `tanhf` and its division put FFMA in the SASS.
- (v) `python3 scripts/port_score.py --write` on a clean tree with all 32: 91 of 1 023 routines
  translated, 25 187 of 263 375 lines of `model/src` (9.56 %).

## What it changes

Every source-term package WW3 compiles now has a C++ translation. Shims and caller hunks for six
of the seven units were written and compiled (`ww3-gpu/kokkos-st-flx-swl`, not pushed); they wait
for the owner's call before entering a pull request. The checkpoint (ADR-0011) of each unit then
needs that fork branch and the regtest matrix.
