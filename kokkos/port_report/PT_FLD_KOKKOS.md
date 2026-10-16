# PT_FLD_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The incremental flooding of the watershed partitioning (Vincent and Soille 1991): `W3PART` digitises
the inverted spectrum into `IHMAX` levels, sorts it with `PTSORT`, builds the neighbour table with
`PTNGHB`, and `PT_FLD` floods level by level to label each spectral bin with its partition
(`w3partmd.F90:349-363`). `W3CPRT` calls `W3PART` once per sea point in a serial loop
(`w3iosfmd.F90:278`), for the partitioned output GFS-Wave writes. 0.19 s self time, 0.05 % of the
PDLIB profile (`docs/data/ww3_pdlib_gprof.tsv`).

## What was ported

| | |
|---|---|
| WW3 routine | `PT_FLD` with its internal `FIFO_ADD`, `FIFO_EMPTY`, `FIFO_FIRST`, `model/src/w3partmd.F90:839-1156` at NOAA-EMC/WW3 `03adda91`; its only `#ifdef` is `W3_S`. The fork's `WW3/model/src/w3partmd.F90` is the same file (`diff`, 2026-10-16) |
| Kokkos code | [`../src/ww_kokkos/part_fld.hpp`](../src/ww_kokkos/part_fld.hpp), `ww::part::pt_fld`, serial per spectrum, no allocation |
| Translated before | none (checked `kokkos/ports.tsv`, `kokkos/src`, remote branches, PRs, 2026-10-16) |
| C shim | none yet; `W3PART` will call it after `ptsort` (`part_ptsort.hpp`) and `ptnghb` (`part_ptnghb.hpp`), each written by another translator |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch C, issue #42 |

The contract the shim honours:

- `NEIGH` is module state of `W3PARTMD` (`w3partmd.F90:92`), rebuilt by `PTNGHB` only when `NK` or
  `NTH` differs from the last build (`MK`, `MTH`, `:694-699`). It depends on nothing else, so one
  table per `(NK, NTH)` serves every grid and spectrum; `pt_fld` reads it as `neigh[(N-1)*9 + K-1]`,
  the column-major `NEIGH(9,NSPEC)`.
- `IHMAX` is `OUTPTS(IMOD)%OUT6%IHMAX` (`w3odatmd.F90:1836`), `MAX(50, IHM)` (`w3gridmd.F90:3140`),
  per grid.
- Scratch per spectrum: `imd[NSPEC]` (the local `IMD`) and `iq[NSPEC]`. The FIFO must have exactly
  `NSPEC` slots: a full ring reads as empty, and the size is part of the Fortran's behaviour.

## What WW3 does here at the pin

No defect found. Read by the translator, 2026-10-16:

- (v) The FIFO cannot overflow when `NSPEC > 9`. Every pending entry is a pixel of the current level,
  queued once (`IMD` guards `:949`, `:1009-1012`; `IMO = MASK` guards `:1046-1048`), plus one
  fictitious pixel. The level never holds all `NSPEC` bins: `ZMAX` maps to level 1 and `ZMIN` to
  `IHMAX >= 50` (`:349`), and `W3PART` returns before `PT_FLD` when `ZMAX-ZMIN < 1E-9` (`:344`). A full ring
  needs all `NSPEC-1` other bins pending at once, before any pop; only bins next to a lower one
  enter then (`:946-953`), and with a single lower bin that is at most 8, so `NSPEC <= 9`. The
  translation keeps the same ring anyway.
- (v) When the last sorted bin is at level `IH`, `M` stays at `NSPEC` (`:955-959`); the next level
  reads `IND(NSPEC)`, finds another level and leaves at once. No bin is skipped.
- (v) The final sweeps (`:1071-1089`) are Jacobi: they read `IMO` and write `IMD`; with `<=` the last
  of equally close neighbours wins. After flooding no bin is negative, so `MINVAL(IMO) > 0` means no
  watershed bin is left; five sweeps may leave some at 0, as in WW3.

## Arithmetic

Reference: the `-O3` object of `w3partmd.F90` (WW3's CMake Release flags, gfortran 16.2,
`WW3-noaa/model/bin/switch_NCEP_st4`), identical to the CMake build's, read by the translator,
2026-10-16.

- (v) `ZPMAX = MAXVAL(ZP)` (`:923`) as gfortran inlines it: the first element `>= -Inf` seeds the
  limit, then `maxss` with the element as first operand (`z > limit ? z : limit`); all-NaN gives
  NaN (`0x7fc00000`), an empty array `-HUGE` (`0xff7fffff`). The C++ object has the same constants
  and `vmaxss` operand order.
- (v) `DIFF = ABS(ZP(JL) - ZP(NEIGH(JN,JL)))` (`:1078`): one `subss` in that order and an `andps`
  mask in both objects; the test `DIFF <= EP1` is `comiss %xmm0,%xmm1` with `jb` in both, so a NaN
  `DIFF` is never taken. No FMA, no libm call.
- (v) Everything else is integer. The vectorised loops of the C++ (`IMO = INIT`, `IMD = 0`,
  `IMD = IMO`, `IMO = IMD`, `MINVAL(IMO)`) are copies or an integer minimum; gfortran makes the first
  two `memset`, and vectorises `MINVAL` too (`w3partmd.vec.txt`, `:1088`).
- (v) Control flow followed line by line: the three passes per level (`:935-961`, `:965-1018`,
  `:1022-1062`), the fictitious-pixel test and re-queue (`:973-982`), the label branch
  (`:991-1014`), the FIFO wrap at `NSPEC` (`:1110`, `:1149`).
- (v) In the CUDA SASS (`sm_89`, `--fmad=false`, no `FFMA`, no `MUFU`) `MAXVAL` is `FSETP.GE` against
  `-INF` for the seed and `FSETP.GT` for the rest, no `FMNMX`: the same strict order as the host
  (reviewer, 2026-10-16). `ZPMAX` only enters through `DIFF <= EP1`, so the sign of a zero maximum
  cannot show.
- ⚠ CUDA compiled, never run.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
