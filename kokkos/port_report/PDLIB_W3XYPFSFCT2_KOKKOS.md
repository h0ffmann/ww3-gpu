# PDLIB_W3XYPFSFCT2_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16). One deliberate departure awaits the owner's decision (below).

Explicit advection of one spectral component on the unstructured grid by flux-corrected
transport (FCT), the third explicit scheme beside [N](PDLIB_W3XYPFSN2_KOKKOS.md) and
[PSI](PDLIB_W3XYPFSPSI2_KOKKOS.md), under `PDLIB`.

## What was ported

| | |
|---|---|
| WW3 routine | `PDLIB_W3XYPFSFCT2`, `model/src/w3profsmd_pdlib.F90:1504-1877` at NOAA-EMC/WW3 `03adda91`, with and without `W3_REF1`. Selected by the `UNST` namelist key `EXPFSFCT = T` (`w3gridmd.F90:3692,3722`) through `PDLIB_W3XYPUG` (`w3profsmd_pdlib.F90:834-835`); all 48 regtest files that name `EXPFSFCT` set it to `F`, so no regtest reaches it (v) |
| Kokkos code | [`../src/ww_kokkos/pdlib_fsfct2.hpp`](../src/ww_kokkos/pdlib_fsfct2.hpp), `w3xypfsfct2<kRef1>`; the sub-step count reuses `fsn2_iter` (`:1702-1710` is the same text as `PDLIB_W3XYPFSN2`'s `:1065-1073` and compiles to the same instructions) |
| C shim | none yet; `MPI_ALLREDUCE(MIN)` (`:1699-1701`, only with `LCALC`) and `PDLIB_exchange1DREAL(AC)` (`:1863`, after each sub-step) are host callables. The `UL` exchange at `:1759` is behind `MPI_PARALL_GRID`, which nothing defines |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch A, issue #42 |

## A WW3 defect, and the departure it forces

(v, read from the GIMPLE and the object by the translator and a fresh reviewer, 2026-10-16):
`UIP(NE)` and `UIM(NE)` (`:1623,1625`) are indexed by node at `:1769-1770`, and the array
statements at `:1773-1774` take their trip count from them, `S = 1..NE`, while `WII` is
`WII(2,NPA)`. So:

- with `NE > npa`, the usual triangulation (`NE` about `2*npa` ⚠), `:1773-1774` read `PP` and `UL` past
  `npa` and write `2*(NE-npa)` doubles past the end of `WII`, a heap overflow (an abort in `free()`
  or a crash is likely ⚠, not run);
- with `NE < npa`, `:1769-1770` write `UIP` and `UIM` past their ends, and `:1782-1794` read
  `WII(:,I>NE)`, which nothing wrote.

The Fortran has a defined result only when `NE == npa`. The C++ sizes `UIP`, `UIM` and `WII` by
`npa` and computes `S = 1..min(NE,npa)`: the Fortran's values wherever it stays inside its arrays,
and with `NE < npa` it reads `WII` columns left from the previous sub-step or call (zero on first
use). That is a departure, not a translation. **Owner's decision:** keep it, compute `WII` over
`S = 1..npa` instead, or refuse the scheme in the shim.

## Arithmetic

Reference: the `-O3` objects of `w3profsmd_pdlib.F90` for `switch_PDLIB` and with `REF1`
(gfortran 16.2), byte-identical to the CMake builds', read by the translator and the reviewer.

- (v) Element-to-node sums (`KKSUM`, `ST`, `PP`, `PM`, the second `ST`) and the `UIP`/`UIM`
  `MAX`/`MIN` gathered per node in increasing `IE` from 0, no atomics; the CFL loop (`:1691-1698`)
  and the boundary loop (`:1837-1850`) serial.
- (v) `MIN`/`MAX` operand orders that depend on the index, read from the object and reproduced for
  every trip count: `USTARI` (`:1762-1763`; `REF0` swaps the `MAX` at the first tail element, `REF1`
  the `MIN` at the first two, with cases for `npa <= 3`), `WII` (`:1773-1774`; the third tail
  element), `UIP`/`UIM` (`:1769-1770`; the third corner).
- (v) Precision as declared (`LAMBDA`, `FL*` in float widened; `DT/(2*TRIA)` a float division;
  `UL`, `U` in double rounded to `REAL`; `MINVAL` widened); the `REF1` statement at `:1805`; no FMA.
- ⚠ Not checked by the reviewer: the order of the `TMP1` `MINVAL` comparisons, the strided copy of
  the `CFLXYMAX` loop. CUDA compiled (no `fma` in the PTX), never run; `min.f32` of −0 against the
  CPU's +0 argued harmless, not shown.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run; no regtest reaches it.
