# Matrix entries 6 and 10 (`ww3_tp2.15`, 5 km grid and ST6): the same amplification as entry 16

2026-10-15, the owner with an agent. WFIP-0003, ADR-0010; follows
[2026-10-14-hoff-matrix-entry16.md](2026-10-14-hoff-matrix-entry16.md) and
[2026-10-15-hoff-matrix-entries51-54.md](2026-10-15-hoff-matrix-entries51-54.md).

## Question

`bash scripts/hoff_matrix.sh --only ww3_tp2.15 --jobs 6` passed entries 5, 7, 8 and 9 and failed
`nccmp-tol` at entry 6, `ww3_tp2.15 -w work_5km -g 5km` (`hs` max|d| 1.5e-3 m, max rel 6.8e-4;
`dir` max|d| 2.7e-2 degrees), and at entry 10, `ww3_tp2.15 -s ST6 -i input_rho -w work_ST6FLX5`
(`hs` 3.9e-3 m, rel 4.4e-3; `fp` rel 1.8e-3; `dir` 1.3e-1 degrees; `dp` 2.9e-2 degrees), against
the `hs`, `fp`, `dir` and `dp` lines of `kokkos/tools/nccmp-tol/tolerances.txt`. Kokkos defect, or
the round-off amplification found at entries 16, 51 and 54?

## What was done

Owner's workstation, Intel Core i9-14900 (32 threads), `OMP_NUM_THREADS=4`, CPU only; WW3 at
`6d1bcdfe`, ww3-gpu at `7fd6bb2`, inside `nix develop ./nix-config/labs/pratico#ww3`. The test
directory was copied from `WW3/regtests/ww3_tp2.15`, the ftp data staged from
`build/ww3-data/v7.14.2/data_regtests` as `stage_data` does, and the `_KOKKOS` switches written as
`scripts/hoff_matrix.sh` writes them (the staged copy matched the matrix's file for file). The
`run_cmake_test` lines of `bash scripts/hoff_matrix.sh --dry-run --only 5,6,9,10` were re-run, 20
runs in parallel, each with its own work directory and build root: the Fortran and Kokkos DIA twice
each for 6 and 10 (once for the controls 5 and 9), the stock WW3 at `-O3`, and the stock WW3 at
`-O3 -march=x86-64-v3` (fused multiply-add on, Fortran only). Fields were compared per output time
and per sea point from `ncdump -p 9,17 -v hs,dir,fp,dp,time,MAPSTA`.

## Result

`hs` max relative difference over the 25 hourly outputs (other fields named where they fail):

| Comparison | 6 (5 km, ST4) | 10 (ST6, FLX5) | 5 (control) | 9 (ST4, FLX5) |
|---|---|---|---|---|
| Fortran vs Fortran, Kokkos vs Kokkos, re-run | 0 | 0 | — | — |
| KOKKOS-switch Fortran DIA vs stock `-O3` | 0 | 0 | 0 | 0 |
| Fortran DIA vs Kokkos DIA | 6.837e-04 | 4.437e-03 | 5.243e-06 | 2.136e-07 |
| stock `-O3` vs FMA `-O3`, Fortran only | 8.861e-04 | 1.217e-03 | 5.243e-06 | 1.602e-04 |

- (v) Both paths are deterministic, the `KOKKOS` switch alone changes no bit, and the re-run F–K
  numbers equal the matrix's to the last digit printed.
- (v) The FMA build alone fails the same lines: entry 6 `hs` rel 8.9e-4 and `dir` max|d| 2.676e-2
  (the F–K value to the digit); entry 10 `hs` rel 1.2e-3, `dir` 2.6e-2, `dp` 1.8e-2. Control 9
  fails `hs` (1.6e-4 > 1e-4) under FMA alone, while its F–K gap is 2e-7: it passed the matrix
  because the Kokkos DIA did not happen to tip a threshold there. Control 5 is quiet both ways.
- (v) Entry 6: the gap is episodic. F–K reaches 4.8e-4 at hour 1 at a low-`hs` cell two cells
  from land, decays, and jumps at hour 13 at cell (105, 38), far from land: the Fortran gives
  1.459759 m, the Kokkos DIA 1.458930 m and the FMA-built Fortran 1.458931 m; at hour 14, 1.455929
  against 1.455757 for both. `dir` at that cell: 105.9184, 105.9391 (K) and 105.9392 (FMA). The two
  perturbations step to the same alternative. The hour-24 F–K maximum (cell (109, 37), 1.5e-3 m)
  is not reproduced by FMA, which steps elsewhere (cell (47, 86), 3.6e-4 m).
- (v) Entry 10: the gap sits on four coastal cells one or two cells from land, (13, 32-33) and
  (18, 25-26), and recurs there for 24 hours. At hour 12 at (18, 25) the Fortran gives 1.019998 m,
  the Kokkos DIA 1.021003 and the FMA build 1.021009; at hour 15, 1.009570, 1.010611 and 1.010602.
  FMA trips the same cells; F–K is larger at (13, 32-33), where FMA stays smaller.
- (v) Thresholds on these paths at `6d1bcdfe`. Both: the step count `IDT = 1+INT(...)`
  (`WW3/model/src/w3srcemd.F90:1574`) and the limiter `DAMAX` (`:1552`). ST4 (6, 9): the tail
  cut-off `NKH` (`:1965-1966`), the direction maximum `BTH0(IK)=MAXVAL(...)` and the kink
  `MAX(0.,BTH0(IK)/COEF1-1.)` of the saturation dissipation (`w3src4md.F90:2287`, `:2309`). ST6
  (10): `NKH` (`w3srcemd.F90:1975-1976`); the stress constraint of `LFACTOR`, an iteration of up
  to 80 steps (`w3src6md.F90:928`) that reverses on a sign change (`:1078`, `:1085`) and exits
  at `ABS(ERR) .LT. 1.54E-4` (`:1090`), so the iteration count, an integer, moves with round-off;
  the threshold `MAX(0.0,EDENS-ETDENS)` of `W3SDS6` (`:717`).
- ⚠ Which threshold fires at which step was not instrumented (that needs a WW3 edit).
- (v) No tolerance, kernel or WW3 file was changed.

## What it changes

Entries 6 and 10 join 16, 51 and 54: configurations where WW3 turns a 1e-7 perturbation into a
step of 1e-4 to 1e-3, which FMA alone reproduces, on the same cells and, at entry 6, to the same
value. Control 9 shows the matrix's pass/fail on these cases depends on which threshold a
perturbation happens to tip. The Kokkos DIA's per-call difference is the known one: close to the
Fortran at `-O3`, not bit-for-bit (`docs/PORT_ASSESSMENT_202610.md` §4). Under ADR-0010 the
regtests do not yet agree, so `W3SNL1` is not ported by the rule on these entries; the owner's
choice stays the one in the entries 51-54 log: a logged per-entry exception judged against the
Fortran's own FMA spread, or closing the bit-identity gap, after which deterministic paths would
agree exactly.

Reproduction: the drivers and the per-point comparison were kept outside the repository; the
commands above, with `bash scripts/hoff_matrix.sh --dry-run --only 5,6,9,10` for the exact
`run_cmake_test` lines, rebuild them.
