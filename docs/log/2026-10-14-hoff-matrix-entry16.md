# Matrix entry 16 (`ww3_ts1 -g ST4_T500`): the Kokkos DIA fails `nccmp-tol`, and the Fortran alone does too

2026-10-14, the owner with an agent. WFIP-0003, ADR-0010.

## Question

`just hoff-matrix --only ww3_ts1:ST4` ran four entries on the CPU. Entries 15, 17 and 24 passed
`nccmp-tol` on `hs` near 1e-7 relative; entry 16, `ww3_ts1 -s ST4 -g ST4_T500 -N`, failed at
4.742e-04 against the 1e-4 of `kokkos/tools/nccmp-tol/tolerances.txt`. Is that a defect of the
Kokkos DIA, or a configuration that amplifies the port's known round-off?

## What was done

Owner's workstation, Intel Core i9-14900 (32 threads), CPU only; ww3-gpu at `f721736` with this
change uncommitted, WW3 at `6d1bcdfe` (`origin/hoff/kokkos-snl1`). The matrix's `work_F_16` and
`work_K_16` were copied out before the full matrix ran, and entry 16 was re-run by hand with
`run_cmake_test`, own work directories and own build roots, inside
`nix develop ./nix-config/labs/pratico#ww3`:

- the Fortran DIA twice, the Kokkos DIA twice, each at `OMP_NUM_THREADS=32` and `=1`;
- the stock WW3 (no `KOKKOS` switch) at `-O3`, at `-O2 -g`, and at `-O3 -march=x86-64-v3`, which
  turns on fused multiply-add and so perturbs the Fortran's round-off with no Kokkos code at all;
- the same FMA perturbation on entry 15;
- `hs` compared per output time from `ncdump -v hs,time -p 9,17`.

## Result

| Comparison, entry 16 unless named | `hs` max rel over 37 outputs |
|---|---|
| Fortran DIA vs Fortran DIA, re-run | 0 (byte-identical netCDF) (v) |
| Kokkos DIA vs Kokkos DIA, re-run | 0 (byte-identical) (v) |
| either DIA, 32 threads vs 1 | 0 (v) |
| KOKKOS-switch executable, Fortran DIA, vs stock WW3 | 0 (v) |
| stock `-O3` vs `-O2 -g` (entry 15 the same) | 0 (v) |
| stock `-O3` vs `-O3 -march=x86-64-v3`, Fortran only | 3.773e-04 (v) |
| the same FMA perturbation, entry 15 | 2.761e-07 (v) |
| Fortran DIA vs Kokkos DIA | 4.742e-04, as in the matrix (v) |

- (v) Both paths are deterministic, and the `KOKKOS` switch alone changes no bit.
- (v) The F–K gap is 1.9e-6 at hour 1, 2.0e-4 at hour 2, then stays between 1e-6 and 4.7e-4
  for 36 hours, worst at hour 12. Entry 15's gap is 0 or about 8e-8 at every output.
- (v) Entry 16 alone sets `SDSBCHOICE=2`, the "TEST 500" breaking of `W3SDS4`
  (`WW3/model/src/w3src4md.F90:2334`). It looks its tables up by nearest neighbour,
  `IKHS = 1+ANINT(KBAR(IKL)*HS(IKL)/DKHS)` (`:2395-2396`), then `DCKI(IKHS,IKD)` and
  `QBI(IKHS,IKD)` (`:2416`, `:2420`): the dissipation is a step function of `KBAR*HS`.
- ⚠ That one ulp moves `IKHS` across a bin is the likely mechanism; which bin flips at which step
  was not instrumented, since that needs an edit to WW3.
- (v) No tolerance, kernel, `WW3/` or `tolerances.txt` line was changed.

The configuration amplifies a 1e-7 perturbation about a thousandfold; the Fortran perturbed by
FMA lands at the same size as the Kokkos DIA. The Kokkos DIA's own per-call difference is the one
already known: close to the Fortran at `-O3`, not bit-for-bit (`docs/PORT_ASSESSMENT_202610.md`
§4).

## What it changes

Under ADR-0010 a routine is ported when WW3's regtests agree; on entry 16 they do not, so
`W3SNL1`'s status in `kokkos/ports.tsv` does not move to L2 on this matrix. The tolerance is not
widened. The owner decides between:

1. recording entry 16 as a round-off-sensitive configuration, judged against the Fortran's own
   FMA spread, as an explicit and logged exception for that entry only; or
2. closing the bit-identity gap (the §4 check: one `W3SNL1` call at the `ww3_ts1` grid against
   the `-O3` Fortran), after which entry 16 would match exactly, since both paths are
   deterministic; any change of reduction order found on the way is asked about first.

Reproduction: the drivers and the per-step comparison were kept outside the repository; the
commands above, with `bash scripts/hoff_matrix.sh --dry-run --only 16` for the exact
`run_cmake_test` line, rebuild them.

## Update, 2026-10-15

Resolved by the fix of `pow11()` (`kokkos/src/ww_kokkos/snl1_tables.cpp`, commit `e305841`): the
Kokkos table `AF11 = (SIG*TPIINV)**11` used the `-O0` chain `x^3*x^8`, WW3 at `-O3` computes
`x^5*x^6`. With that one change the Kokkos DIA is byte-identical to the Fortran inside WW3, and this
entry's `test.comp` finds 0 differing files (v)
([`2026-10-14-snl1-bitwise-af11.md`](2026-10-14-snl1-bitwise-af11.md),
[`../data/hoff_matrix_2026-10-15.tsv`](../data/hoff_matrix_2026-10-15.tsv)). The amplification
described above is real in WW3, so a perturbation of the Fortran's own round-off (FMA) still
moves these entries; the port no longer perturbs it.
