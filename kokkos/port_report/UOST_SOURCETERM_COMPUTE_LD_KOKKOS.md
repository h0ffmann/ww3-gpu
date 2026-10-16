# UOST_SOURCETERM_COMPUTE_LD_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The `COMPUTE_LD` method of the unresolved-obstacles source term (switch `UOST`): the local
dissipation of the spectrum in a cell partly blocked by subgrid obstacles. For every
`(IK, ITH)` with local transparency `ALPHA < 1` it sets `S = SFC * SPEC * PSI` and
`D = SFC * PSI`, with `SFC = -CG/CELLSIZE * (1-BETA)/BETA` when `BETA > 0.09` and
`SFC = -CG/CELLSIZE * GAMMAUP` otherwise, and `PSI` the wave-age reduction factor of
`COMPUTE_REDUCTION_PSI`. It is reached only from `UOST_SOURCETERM_COMPUTE`
(`w3uostmd.F90:1123`) through vtable slot `+0x50` of `SRCTRM`.

## What was ported

| | |
|---|---|
| WW3 routine | `UOST_SOURCETERM_COMPUTE_LD`, `model/src/w3uostmd.F90:793-898` at NOAA-EMC/WW3 `03adda91` (106 lines); compiled by the `UOST` regtest switches of `ww3_tp2.21` (3) and `ww3_ts4` (6) (`git grep -lw UOST -- '*switch*'`), by no NOAA operational switch in `model/bin`. Its only `#ifdef` blocks are `W3_S` (`STRACE`, output only) |
| Kokkos code | [`../src/ww_kokkos/uost_uost_sourceterm_compute_ld.hpp`](../src/ww_kokkos/uost_uost_sourceterm_compute_ld.hpp), `ww::uost::uost_sourceterm_compute_ld`; calls `ww::uost::compute_reduction_psi` ([`COMPUTE_REDUCTION_PSI_KOKKOS.md`](COMPUTE_REDUCTION_PSI_KOKKOS.md)) |
| Structural change | none. The `THIS%GRD` and `THIS%SGD` components it reads are arguments: the four grid arrays as contiguous column-major pointers with Fortran base 1 (`w3uostmd.F90:187-195`), `UOST_LCL_OBSTRUCTED` as `LOGICAL(4)` read as nonzero, `UOSTLOCALALPHA`/`BETA` as `INTEGER*1` |
| C shim | none yet |
| Plan | [`EASY_BATCH_KOKKOS_PLAN_202610.md`](../../docs/EASY_BATCH_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3uostmd.F90` in WW3's CMake Release build, gfortran 16.2.0,
`switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8` (build `mixa`), 2026-10-16. C++ unit:
the openmp-release flags (`-O3 -march=x86-64-v3 -ffp-contract=off`), g++ 16.2.0, warning-free.

- (v) The module recompiled from the build's `flags.make` gives a disassembly identical to the
  build's `w3uostmd.F90.o` (`cmp`).
- (v) Control flow matches the object: `S`, `D` zeroed (`memset`, only when `NSPEC > 0`) before
  the `UOST_LCL_OBSTRUCTED` test; loops skipped for `NK <= 0` or `NTH <= 0`; `CYCLE` on
  `ALPHA == 1` before `BETA`'s use; `COMPUTE_REDUCTION_PSI` inlined in both objects with one
  `cosf` call on the value before `:749`'s `ABS`.
- (v) `ALPHA`, `BETA` (`:867-870`): `cvtsi2ss` of the `INTEGER*1`, one `divss` by
  `UOSTABMULTFACTOR`, `mulss` with the quotient as the first operand, then `minss x,1.0`,
  `maxss x,0.0`, so a NaN product gives 1. g++ turns `ALPHA`'s MIN/MAX and the test into one
  `comiss` against 1.0 (same outcome for every input, NaN included: it cycles) and `BETA`'s into
  a branch on `1.0 > x` then `vmaxss x,0.0`, which is the same selection as the object's.
- (v) `SFC` (`:884-891`): both objects compute `CGI/CELLSIZE` once, then `(1-BETA) * q / BETA`
  or `q * GAMMAUP`, then negate (`xorps`); the C++ is written in the object's operand order.
  `S` is `(SPECI * SFC) * PSI` and `D` is `SFC * PSI`, first operands as in the object.
- (v) `PSI`: the same `subss`, `andps`, `comiss` against `PI`, the `2*PI` subtraction loop,
  `PI/2 - |thdelta| > 1e-6`, `CGI+CGI`, two `divss`, the three-way ramp with `wa - 0.5` (the
  division by `1.0` folded away in both).
- ⚠ (argued) `CELLSIZE` (`:876`): g++ emits `UOSTCELLSIZEFACTOR` as the first operand of the
  product and gfortran the array element, whichever order the C++ is written in. The bits can
  differ only when both are NaN; `UOSTCELLSIZEFACTOR` is the default initializer 1000
  (`w3gdatmd.F90:787`) and is assigned nowhere in `model/src`, so it is never NaN.
- (v) No libmvec call, no vectorised loop in either object (`-fopt-info-vec-optimized` reports
  nothing for the routine), no reduction.
- (v) No undefined read: every value it reads is set before; `DT` is passed through and never read
  (`COMPUTE_REDUCTION_PSI`). A zero `UOSTABMULTFACTOR` or `CELLSIZE` gives the same inf or NaN on
  both sides.
- (v) Fresh review 2026-10-16: no arithmetic defect. `w3uostmd.F90` recompiled from build `mixa`'s `flags.make` (disassembly identical to the CMake object); GIMPLE and assembly compared with a C++ unit at the openmp-release flags (warning-free) operation by operation: `ALPHA`/`BETA` selection incl. NaN and -0, `CGI/CELLSIZE`, `(1-BETA)*q/BETA` and `q*GAMMAUP` operand order, negation, `S`/`D` products, the inlined `PSI`, `.rodata.cst4` bits, signed `INTEGER*1` loads (`movsb`), no vectorisation. Fixed two citations: the arrays' `ALLOCATE`s span `w3uostmd.F90:187-195` (was 187-194), and the header cited `w3gdatmd.F90:1000` (`TYPE UOSTP`) for `UOSTCELLSIZEFACTOR`; the object is unchanged.
- ⚠ CUDA not compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
