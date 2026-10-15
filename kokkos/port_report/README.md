# Port reports

One report for each WW3 routine with a Kokkos port, named `<ROUTINE>_KOKKOS.md` after the routine
in upper case: [`W3SNL1_KOKKOS.md`](W3SNL1_KOKKOS.md), [`INSNL1_KOKKOS.md`](INSNL1_KOKKOS.md).
Every row of [`../ports.tsv`](../ports.tsv) has one (`scripts/port_score.py --check` fails
without it). The owner asked for them on 14 October 2026.

A report says, for one routine:

1. **What was ported:** the WW3 file and lines at the pinned commit, and the Kokkos files.
2. **The benchmarks:** every measured number, in the table between `<!-- bench:start -->` and
   `<!-- bench:end -->`. The website reads that table (`site/content`) and draws each step as a
   bar: the speed-up over the step's own baseline, in shades of the `--proof` light, deeper as the
   speed-up grows; a step slower than its baseline is drawn grey and says so.
3. **How it was regtested:** WW3's own regtests (inside WW3: `--hoff --cpu` against the Fortran
   DIA and pair to pair against `--ww3`; the WFIP-0003 regtest matrix), the GPU, and CI. The port
   has no tests of its own (the owner, 2026-10-14: "kokkos code should NOT have its own tests,
   hard rule. All tests should come [from] WW3 regtests."); an L1 result from before that date
   stays in the report as history, dated, marked removed with the tests.
4. **GoogleTest:** none for the port since 2026-10-14; the report says which suites were removed.

## The bench table

One row per measurement, and nothing typed that a file does not hold:

| column | what it holds |
|---|---|
| `step` | the order of the steps on the page, 1, 2, 3 … |
| `date` | the day the number was measured |
| `what` | what was timed, in a few words |
| `backend` | where it ran |
| `value` | the time of this step |
| `baseline` | the time of what this step is compared with, in the same unit |
| `unit` | `ms` or `s` |
| `source` | the file or the command the number comes from |

The speed-up is `baseline / value`, computed by the site, never typed. A number in the table is
copied from its source; the source is the authority when the two differ.
