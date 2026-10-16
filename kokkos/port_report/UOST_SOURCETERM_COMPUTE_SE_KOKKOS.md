# UOST_SOURCETERM_COMPUTE_SE_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The `COMPUTE_SE` method of the unresolved-obstacles source term (switch `UOST`): the shadow
effect of obstacles in the upstream cells on a cell that is not on the grid's edge. For every
`(IK, ITH)` with shadow transparency `ALPHASH < 1` it sets `S = SFC * SPEC * PSI` and
`D = SFC * PSI`, with `SFC = -CG/CELLSIZE * (BETASH/ALPHASH - 1)` when `ALPHASH > 0.2` and
`SFC = -CG/CELLSIZE * GAMMADOWN` otherwise, and `PSI` the wave-age reduction factor of
`COMPUTE_REDUCTION_PSI`. It is reached only from `UOST_SOURCETERM_COMPUTE`
(`w3uostmd.F90:1125`) through the type-bound procedure `COMPUTE_SE` (`:88`).

## What was ported

| | |
|---|---|
| WW3 routine | `UOST_SOURCETERM_COMPUTE_SE`, `model/src/w3uostmd.F90:920-1034` at NOAA-EMC/WW3 `03adda91` (115 lines); compiled by the `UOST` regtest switches of `ww3_tp2.21` (3) and `ww3_ts4` (6) (`git grep -lw UOST -- '*switch*'`), by no NOAA operational switch in `model/bin`. Its only `#ifdef` blocks are `W3_S` (`STRACE`, output only) |
| Kokkos code | [`../src/ww_kokkos/uost_uost_sourceterm_compute_se.hpp`](../src/ww_kokkos/uost_uost_sourceterm_compute_se.hpp), `ww::uost::uost_sourceterm_compute_se`; calls `ww::uost::compute_reduction_psi` ([`COMPUTE_REDUCTION_PSI_KOKKOS.md`](COMPUTE_REDUCTION_PSI_KOKKOS.md)) |
| Structural change | none. The `THIS%GRD` and `THIS%SGD` components it reads are arguments: the four grid arrays as contiguous column-major pointers with Fortran base 1 (`w3uostmd.F90:189-200`), `UOST_SHD_OBSTRUCTED` as `LOGICAL(4)` read as nonzero, `UOSTSHADOWALPHA`/`BETA` as `INTEGER*1`. `SPEC(ISP)` is loaded before the `PSI` call instead of after it, which changes no arithmetic and keeps the object's operand order (below) |
| C shim | none yet |
| Plan | issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3uostmd.F90` in WW3's CMake Release build, gfortran 16.2.0,
`switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8` (build `mixa`), 2026-10-16. C++ unit:
the openmp-release flags (`-O3 -march=x86-64-v3 -ffp-contract=off`), g++ 16.2.0, warning-free.

- (v) The module recompiled from the build's `flags.make` gives a disassembly identical to the
  build's `w3uostmd.F90.o` (`cmp`).
- (v) Control flow matches the object: `S`, `D` zeroed (`memset`, only when `NSPEC > 0`) before
  the edge test `IX == 1 or NX, IY == 1 or NY` and the `UOST_SHD_OBSTRUCTED` test; loops skipped
  for `NK <= 0` or `NTH <= 0`; `CYCLE` on `ALPHASH == 1`; `BETASH` loaded and computed only on
  the `ALPHASH > 0.2` branch in both objects; `COMPUTE_REDUCTION_PSI` inlined in both with one
  `cosf` call.
- (v) `ALPHASH`, `BETASH` (`:1000-1003`): `cvtsi2ss` of the `INTEGER*1`, one `divss` by
  `UOSTABMULTFACTOR`, `mulss` with the quotient as the first operand, then `minss x,1.0`,
  `maxss x,0.0` in gfortran, so a NaN product gives 1. g++ turns `ALPHASH`'s MIN/MAX and the
  `== 1` test into one `comiss` against 1.0 (it cycles for `x >= 1` and NaN, as the object) and
  `BETASH`'s into a branch on `1.0 > x` then `vmaxss x,0.0`: the same selection.
- (v) `GG = CGI/CELLSIZE`, `GAMMMA = BETASH/ALPHASH - 1` (`divss`, `subss` 1.0), the `> 0.2`
  test against the same `0x3e4ccccd`.
- (v) `S = (SPECI * SFC) * PSI` and `D = SFC * PSI`, first operands as in the object. g++ puts
  `SPECI` first only when `SPEC(ISP)` is read before the inlined `PSI` code (with the load after
  it, `SPECI` became a memory operand in second place); checked in the object.
- ⚠ (argued) `SFC` (`:1026`): gfortran multiplies with `GAMMMA` as the first operand, g++ with
  `GG`, whatever order the C++ is written in. The bits can differ only when both are NaN, and
  `GAMMMA` is never NaN: `BETASH` in `[0, 1]` over `ALPHASH` in `(0.2, 1)`, or `GAMMADOWN`, the
  default initializer 20 (`w3uostmd.F90:78`) assigned nowhere in `model/src`.
- ⚠ (argued) `CELLSIZE` (`:1008`): g++ emits `UOSTCELLSIZEFACTOR` as the first operand and
  gfortran the array element, as in `UOST_SOURCETERM_COMPUTE_LD`. `UOSTCELLSIZEFACTOR` is the
  default initializer 1000 (`w3gdatmd.F90:787`), assigned nowhere, so it is never NaN.
- (v) `PSI`: the same `subss`, `andps`, `comiss` against `PI`, the `2*PI` subtraction loop,
  `PI/2 - |thdelta| > 1e-6`, `CGI+CGI` (`addss`), two `divss`, the three-way ramp.
- (v) No libmvec call, no vectorised loop in either object, no reduction.
- (v) No undefined read: `DT` is passed through and never read; `N = 8` (`:972`, an implicit
  `SAVE`), `CGDIAG`, `THDIAG`, `ITHDIAG` and the other unused locals are never read.
- (v) Fresh review 2026-10-16: no defect. `w3uostmd.F90` recompiled from build `mixa`'s `flags.make` (disassembly identical to the CMake object); GIMPLE and assembly compared with a C++ unit at the openmp-release flags (warning-free) operation by operation: edge and obstruction tests, `ALPHASH`/`BETASH` selection incl. NaN and -0, `BETASH` only on the `> 0.2` branch, `GG`, `BETASH/ALPHASH - 1`, `S`/`D` operand order (`SPECI` first), the inlined `PSI`, `.rodata.cst4` bits, signed `INTEGER*1` loads (`movsb`), no vectorisation; the two argued operand-order differences (`CELLSIZE`, `SFC`) confirmed in both objects. Nothing changed.
- ⚠ CUDA not compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
