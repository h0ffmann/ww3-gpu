# SMCAVERG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The 1-2-1 smoothing of a sea-point field on a Spherical Multiple-Cell (SMC) grid. Each U-face
adds `IJKUFc(3,i)*(CVQ(M)+CVQ(L))` to `AUN` of both its cells, each V-face adds
`IJKVFc(3,j)*(CVQ(M)+CVQ(L))` to `AVN`, and the cell value becomes
`CVQ(n) = AUN(n)*0.125/IJKCel(4,n) + AVN(n)*0.125/IJKCel(3,n)`; the polar cell keeps its value
when `ARCTC`. `W3PSMC` calls it on the propagated field `CQ` when `FVERG` is set
(`w3psmcmd.F90:900`); it is the only caller (`git grep -n SMCAverg WW3-noaa/model/src`).

## What was ported

| | |
|---|---|
| WW3 routine | `SMCAverg`, `model/src/w3psmcmd.F90:2549-2700` at NOAA-EMC/WW3 `03adda91` (152 lines); compiled by every switch with `SMC`: `model/bin/switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, `regtests/ww3_tp2.10`, `ww3_tp2.16`, `mww3_test_09`, `ww3_tp2.14/input_oasacm6` (`git grep -lw SMC -- '*switch*'`); `ww3_tp2.10` and `ww3_tp2.16` `switch_MPI_OMPH` carry `OMPG` and `B4B` |
| Kokkos code | [`../src/ww_kokkos/psmc_smcaverg.hpp`](../src/ww_kokkos/psmc_smcaverg.hpp): `smcaverg_ufc_point` (one iteration of `DO i`, `:2585-2615`), `smcaverg_vfc_point<B4bOmpg>` (one iteration of `DO j`, `:2632-2662`), `smcaverg_cel_point` (one iteration of `DO n`, `:2679-2687`) and the host driver `smcaverg<B4bOmpg>` |
| Work arrays | `CVF`, `AUN`, `AVN(-9:NSEA)` (automatic arrays, `:2565`) are caller-provided, `NSEA+10` each |
| `#ifdef` | `W3_OMPG`: the three loops in one parallel region, the face updates `OMP ATOMIC`; arithmetic the same (below). `W3_B4B` with `W3_OMPG`: the template flag `B4bOmpg` (`:2622-2624`, `:2640-2642`, `:2669-2671`). `W3_B4B` alone changes nothing: its only line, `:2594`, is a comment |
| C shim | none yet |
| Plan | [`BATCH3_SHIMS_KOKKOS_PLAN_202610.md`](../../docs/BATCH3_SHIMS_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3psmcmd.F90` built with the flags of WW3's CMake Release build
under `switch_SMCMlt` (gfortran 16.2.0, pinned shell), identical to the CMake object, the same
file with `-DW3_OMPG -fopenmp`, and with `-DW3_OMPG -DW3_B4B -fopenmp`; compared with a throwaway
C++ unit instantiating both template values, compiled with the openmp-release flags (g++ 16.2.0,
warning-free), 2026-10-16.

- (v) Face sums: `cvtsi2ss` of `IJKUFc(3,i)` (`IJKVFc(3,j)`) times `(CVF(M)+CVF(L))`, the sum
  with `CVF(M)` as first operand, a `PAREN_EXPR` in the GIMPLE, then the product, in both objects.
  The Fortran reads `CVF`, a copy, so the face loops never see an averaged value.
- (v) Updates: `AUN(L)` only when `L > 0`, then `AUN(M)` only when `M > 0`, in face order; the
  same for `AVN`. Boundary cells (`-9:0`) are read and never written.
- ⚠ Operand order of the update `AUN(L) + CNST5` (and `AVN(L)`): the Fortran object puts
  `AUN(L)` first (`addss`), the C++ object `CNST5` first (`vaddss` with a memory operand). The
  `AUN(M)` update has `CNST5` first in both. This changes only which NaN payload survives when both
  operands are NaN.
- (v) Cell loop: `0.125/REAL(IJKCel(3,n))` and `0.125/REAL(IJKCel(4,n))` by `divss`/`divps`,
  then `AUN*CNST4 + AVN*CNST3`. The Fortran object vectorises this loop (4 and 2 lanes, then a
  scalar tail); element-wise, so the lanes give the same bits as the scalar C++ loop. ⚠ The
  vector body and the 2-lane tail add with the `AUN` term first, the two scalar tails with the
  `AVN` term first; the C++ puts the `AUN` term first everywhere. Again a NaN payload only, and
  only for the cells that fall in the Fortran's scalar tail.
- (v) `W3_OMPG`: the outlined `smcaverg._omp_fn.0` has the same face sum, product, divisions and
  cell expression; each atomic update is a `lock cmpxchg` loop on `CNST5 + old`. Run on several
  threads the order of the additions into one cell is not fixed, so WW3 itself is not
  reproducible there; the C++ driver runs the faces serially in the Fortran's order, which is the
  one-thread result.
- (v) `W3_B4B` with `W3_OMPG`: `CNST6*1.0E6` (`mulss` with `CNST6` first), `INT()` as
  `cvttss2si` then `cvtsi2ss`, reproduced by `ww::st4::fint` (out of range or NaN gives
  `INT_MIN`, as the instruction does); `AUN/1.0E6` and `AVN/1.0E6` over `-9:NSEA` by `divss`/`divps`
  in both objects.
- (v) No FMA, no libm call in either object; the C++ face loops stay scalar.
- (v) `CNST0 = CVQ(NSEA)` is read before any write and restored after the cell loop only when
  `ARCTC`.

## WW3 itself

- ⚠ Under `W3_B4B` with `W3_OMPG` (`switch_MPI_OMPH` of `ww3_tp2.10` and `ww3_tp2.16`) the
  x-sum `AUN` is divided by `1.0E6` (`w3psmcmd.F90:2623`) while the matching scale
  `CNST5=int(CNST5 * 1.0e6)` at `:2594` is commented out, so the x-part of the average is
  `1.0E6` times too small; the y-part is scaled and unscaled (`:2641`, `:2670`). The port
  reproduces it.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
