# NOAA's operational switches translated to Kokkos; what the reviews found in WW3

2026-10-16, the owner with agents. Issue #42,
[`NOAA_OPS_KOKKOS_PLAN_202610.md`](../NOAA_OPS_KOKKOS_PLAN_202610.md), PR #152
(`goal/42-port-noaa-ops`).

## Question

GFS-Wave's operational switches use the physics already translated. Can the code NOAA runs beyond
it, the unstructured `PDLIB` solver of `switch_NWS_rwps` and `ww3_ufs1.1` with `IC4`, `REF1`, `TR1`,
`BT4`, be translated to the same standard (bit for bit, no test run, one translator and one fresh
reviewer per routine), and in what order?

## What was done

- A gprof profile of `ww3_tp2.17` grid c (`PDLIB`, implicit, one MPI rank):
  `scripts/profile_case.sh pdlib`, `docs/data/ww3_pdlib_gprof.tsv`. `ww3_shel` took 1 439 s; gprof
  attributes 369 s to routines (v). `PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK` is 37.5 % and
  `CALCARRAY_JACOBI_VEC` 20.4 % of the attributed time, both then untranslated.
- `kokkos/PORT_TABLE.tsv`, written by `scripts/port_score.py --write` from `kokkos/ports.tsv`, the new
  hand-kept `kokkos/port_claims.tsv` and every committed profile: one row per routine, the next port
  first, with its claim, checkpoint and regtest record.
- 23 routines translated against the `-O3` objects of the switches that compile them, each with a
  fresh review: the PDLIB solver (`PDLIB_W3XYPUG`, the N, PSI and FCT explicit schemes, the explicit
  block, the Gauss-Seidel block, the matrix assembly, the boundary routines), its `W3PARALL` helpers,
  `W3SIC4`, `W3SREF`, `W3SBT4` with its tables, `W3STR1`, `W3SIS1`, `W3SIC1`, `WAVNU3`, `WAVNU_LOCAL`.
  Translated code went from 5.20 % to 6.52 % of WW3's lines (17 176 of 263 375) (`site/data/port_score.tsv`).
  Nothing is wired and nothing was regtested.
- A comment pass cut the full-line comments of the 19 new headers from 2 050 to 738 lines, objects
  byte-identical before and after (the owner asked for it: comments cost every agent context and go
  stale).

## Result

What the reviews caught in the port, each fixed before its commit:

- (v) Switch variants missed: the translators worked against `switch_PDLIB`, without `REF1`, which
  `switch_NWS_rwps` and `ww3_tp2.17/input_deep` compile; three routines lacked their `W3_REF1`
  blocks. They became template flags, each checked against an object built with that switch.
- (v) Undefined behaviour in C++: a `static_cast<int>` of a float out of range let g++ delete a
  Fortran `MAX(...,0)` clamp in `W3SBT4`; `INT()` now goes through `fint`, which reproduces
  `cvttss2si`.
- (v) gfortran puts `MIN`/`MAX` operands on different sides in vectorised bodies and scalar tails
  (`CALCARRAY_JACOBI_SPECTRAL_1`, the FCT scheme, the explicit block), so with NaN or ±0 a result
  depends on the element's position; the headers reproduce each rule.

What they found in WW3 itself at `03adda91` (v, read from the source, GIMPLE and objects):

- `PDLIB_EXPLICIT_BLOCK` reads `CGSIG`, an automatic array only `WAVNU3` fills inside `IF (LCALC)`,
  outside that `IF` (`w3profsmd_pdlib.F90:6487,6513,6623,6679`); `LCALC` is false on steps 2..NT of
  every `W3WAVE` call without currents (`w3wavemd.F90:767,973,1188-1220,1436`). On those steps the
  Fortran converts with whatever the allocator returns. The one regtest with `EXPTOTAL = T` has
  currents and never takes that path.
- `PDLIB_W3XYPFSFCT2` runs the `WII` statements over `NE` (the extent of `UIP`, `UIM`) while `WII` is
  `(2,NPA)` (`:1623-1625,1769-1774`): with `NE > npa` it writes past `WII` on the heap, with `NE < npa`
  it reads `WII` nothing wrote. No regtest selects the scheme (`EXPFSFCT = F` everywhere). The port
  bounds the loop; whether to keep that is the owner's decision.
- `PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK`'s norm `MPI_ALLREDUCE` sends a `REAL(8)` with `MPI_REAL4` into an
  unwritten local (`:5593,6190`, `PDLIB/yowdatapool.F90:77`), so its result is undefined; its limiter
  reads `TAUWX`, `TAUWY`, `LLWS`, never assigned. Both are off in every regtest.
- `FreqShiftMethod` is a module variable fixed at 2 (`w3profsmd_pdlib.F90:125`), so `PROP_FREQ_SHIFT`
  is unreachable; `WAVNU_LOCAL` is called only under `NOCGTABLE`, which nothing defines.

Where the Fortran is undefined no port can be bit-identical; those paths are ⚠ in their reports.

Two agents ran throwaway bit comparisons in scratch against the static-only instruction; they are
not committed and are not evidence under ADR-0010. No regtest was run (the owner, 2026-10-16).

## What it changes

The checkpoint of these units (ADR-0011) needs shims first: the MPI hooks of the PDLIB routines,
the `U_JAC` and `CGSIG` lifetime contracts, the `sig` and `S`/`D` contracts of the source terms, a new
fork branch with `bind(C)` interfaces and `w3wavemd.F90` hunks, and the `ww_kokkos` ABI moved to 0.3
on both sides (`kokkos/src/fortran_iface/PATCH.md`). The regtests that exercise the undefined paths
above are none, so they cannot show those defects; a case built to reach them would.
