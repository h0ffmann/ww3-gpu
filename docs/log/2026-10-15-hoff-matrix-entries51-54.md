# Matrix entries 51 and 54 (`ww3_ts3`, ST1): the same amplification as entry 16

2026-10-15, the owner with an agent. WFIP-0003, ADR-0010; follows
[2026-10-14-hoff-matrix-entry16.md](2026-10-14-hoff-matrix-entry16.md).

## Question

The first full `just hoff-matrix --jobs 8` failed `nccmp-tol` on `hs` (1e-4 relative) at entry
51, `ww3_ts3 -s ST1_PR1`, and 54, `ww3_ts3 -s ST1_PR3_UQ`, while the ST4 and ST6 entries of
`ww3_ts3` passed. Kokkos defect, or the round-off amplification found at entry 16?

## What was done

Owner's workstation, Intel Core i9-14900, `OMP_NUM_THREADS=4` as in the matrix, CPU only; WW3 at
`6d1bcdfe`, inside `nix develop ./nix-config/labs/pratico#ww3`. The `run_cmake_test` lines of
`bash scripts/hoff_matrix.sh --dry-run --only <n>` for 51, 54 and the control 66
(`ww3_ts3 -s ST4_PR1`), re-run with own work directories and build roots: Fortran and Kokkos DIA
twice each, the stock WW3 at `-O3`, and the stock WW3 at `-O3 -march=x86-64-v3` (fused
multiply-add on, Fortran only). `hs` compared per output time and per point from `ncdump -p 9,17`.

## Result

`hs` max relative difference over the 7 hourly outputs:

| Comparison | 51 ST1_PR1 | 54 ST1_PR3_UQ | 66 ST4_PR1 |
|---|---|---|---|
| Fortran vs Fortran, Kokkos vs Kokkos, re-run | 0 | 0 | — |
| KOKKOS-switch Fortran DIA vs stock `-O3` | 0 | 0 | 0 |
| Fortran DIA vs Kokkos DIA | 2.137e-04 | 3.774e-04 | 1.916e-07 |
| stock `-O3` vs FMA `-O3`, Fortran only | 4.204e-05 | 4.079e-04 | 1.235e-06 |

- (v) Both paths are deterministic and the `KOKKOS` switch alone changes no bit.
- (v) The gap is local: a few low-`hs` points (0.2 to 1.1 m) within four cells of the open edge
  of the all-sea 41×41 grid; the 10 m vortex core agrees to about 1e-7. At 51, F–K stays near
  1e-6 for four hours and jumps to 2.1e-4 at one point at hour 5; the FMA run jumps at the same
  hour, by 4.2e-5.
- (v) The case is point-symmetric. At hour 5 the Fortran gives 0.7684794 and 0.7684793 m at the
  mirrored points (125, 500) and (-125, -500) km; the Kokkos DIA steps to 0.7687694 at the first,
  and the FMA-built Fortran steps to 0.7687535 at the second. A step of the same size landing
  on whichever twin the perturbation tips is a threshold crossed inside WW3, not a kernel error.
- (v) Thresholds on the ST1 path at `6d1bcdfe`: the integer cut-off
  `NKH = MAX(2, MIN(NKH1, INT(FACTI2+FACTI1*LOG(FHIGH))))` (`WW3/model/src/w3srcemd.F90:1931-1932`),
  which sets the bins replaced by the parametric tail (`:2014-2020`); the step count
  `IDT = 1+INT(...)` (`:1574`); the limiter `DAMAX` (`:1552`); the kink of `W3SIN1`
  (`w3src1md.F90:384-386`).
- ⚠ Which of them fires, and when, was not instrumented (that needs a WW3 edit). ST4 has the same
  `NKH` construct (`w3srcemd.F90:1962-1966`) and passes here.
- (v) No tolerance, kernel or WW3 file was changed.

## What it changes

The three failures of the first full matrix (16, 51, 54) share one cause: configurations where WW3
itself turns a 1e-7 perturbation into a step of 1e-4, which FMA alone reproduces. The Kokkos DIA's
per-call difference is the known one: close to the Fortran at `-O3`, not bit-for-bit
(`docs/PORT_ASSESSMENT_202610.md` §4). Under ADR-0010 the regtests do not yet agree, so
`W3SNL1` is not ported by the rule on these entries. The owner chooses between a logged
per-entry exception judged against the Fortran's own FMA spread, and closing the bit-identity
gap, after which deterministic paths would agree exactly.

## Update, 2026-10-15

Resolved by the fix of `pow11()` (`kokkos/src/ww_kokkos/snl1_tables.cpp`, commit `e305841`): the
Kokkos table `AF11 = (SIG*TPIINV)**11` used the `-O0` chain `x^3*x^8`, WW3 at `-O3` computes
`x^5*x^6`. With that one change the Kokkos DIA is byte-identical to the Fortran inside WW3, and this
entry's `test.comp` finds 0 differing files (v)
([`2026-10-14-snl1-bitwise-af11.md`](2026-10-14-snl1-bitwise-af11.md),
[`../data/hoff_matrix_2026-10-15.tsv`](../data/hoff_matrix_2026-10-15.tsv)). The amplification
described above is real in WW3, so a perturbation of the Fortran's own round-off (FMA) still
moves these entries; the port no longer perturbs it.
