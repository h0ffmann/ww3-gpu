# The Kokkos port's own tests removed; WW3's regtests are its only tests

2026-10-14, the owner with agents. [ADR-0010](../ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md),
[WFIP-0003](../WFIPs/WFIP-0003-kokkos-regtest-matrix.md), branch `claude/wfip-0003-impl-regtests-only`.

## Question

The owner, verbatim: "implement WFIP-003, now, A single PR, step by step, use subagents, now
kokkos code should NOT have its own tests, hard rule. All tests should come com WW3 regtests".
What has to go, what stays, and what does the repository say about validation afterwards?

## What was done

Deleted from `kokkos/tests/` (v) (`git status` on the branch, 2026-10-14, against `main` at
`f721736`):

| Removed | What it was |
|---|---|
| `L1_test_snl1_tables.cpp` | `make_tables` against `INSNL1`: 32 address tables exact, weights and `AF11` to 1e-6 relative |
| `L1_test_snl1_dia.cpp` | `snl1` against `W3SNL1` to 1e-5 relative, zero in and out, cubic scaling, relaunch reproducibility |
| `L1_test_snl1_shim.cpp` | the same parity through the C ABI, with the error paths |
| `fixtures/shim_driver.F90` (CTest `shim_roundtrip`) | the same call from Fortran |
| `fixtures/snl1_ref.F90`, `snl1_sea_state.F90`, `gen_snl1_fixture.F90`, `gen_snl1_ww3lib.F90`, `fixtures/CMakeLists.txt` | the standalone verbatim copy of `W3SNL1`/`INSNL1`, the sea state, the generator and the never-run cross-check |

With them went the `snl1-fixtures` and `l1-crosscheck` recipes, `scripts/kokkos_coverage.py`,
`docs/data/kokkos_coverage.tsv`, `just kokkos-coverage` and the `googletest` column of
`kokkos/ports.tsv` (the code and CI side of the same change, by another agent, ⚠ not checked
here beyond the file list). Kept: `snl1_nk25_nth24.bin`, frozen, as the input of `ww_bench_snl1`
and `proof/snl1_cons`; `bench_small/`; the GoogleTest suites that test no port
(`L1_test_intro`, `L1_test_nccmp_tol`, `L1_test_bench_case`, `L1_test_fetch_analyse`) (v)
(`ls kokkos/tests`). Every file is still in git history: `git show f721736:<path>`.

The prose was rewritten to the rule: `AGENTS.md`, `CONTRIBUTING.md`, `README.md` and
`README.pt-BR.md`, `docs/AGENTS_KOKKOS_202609.md` §1.5, §1.6, §4 and §5, `docs/GLOSSARY.md`, the
dated plans (a dated note at the top, the plan kept as written), lessons 07, 08, 10, 11, 12, 13
and 14 with the figure `porting-validation-ladder` re-rendered, exercise 13, the port reports,
`kokkos/README.md`, `kokkos/PORT_STATUS.md` (hand-written parts), `bench/README.md` and the two
porting notes in `notes/`. `pubs/proposal/` was not touched.

## Result

The rule as the repository now states it: a ported routine is validated only by WW3's regtests,
run inside WW3 from the h0ffmann/WW3 fork with the Fortran routine and with the Kokkos routine, on
the CPU (`--hoff --cpu` against `--ww3`; `just smoke-hoff`, `just full-hoff`, `just hoff-matrix`),
compared within `kokkos/tools/nccmp-tol/tolerances.txt`. If they agree, the routine is ported. No
`--hoff --gpu` run is part of the port.

The last results of the removed tests, kept as history: max relative error 0.0 on serial-debug,
openmp-release and cuda-release (2026-09-15; the CPU presets re-run 2026-10-14, `just
kokkos-test`, 14/14, as ADR-0010 records) ⚠ (not re-run for this entry). The regtest evidence that
replaces them: `hs` within 9.879e-8 relative on `ww3_ts1` and 1.832e-7 on `bench_small`, PASS,
not byte-identical (v) (`kokkos/port_report/W3SNL1_KOKKOS.md`, read 2026-10-14). The regtest
matrix has not run ⚠.

## What it changes

- "No timing without parity" now means a passed WW3 regtest replay on the same commit; the
  2026-09-15 timing record keeps its `l1` as history.
- A defect in a kernel is found in a model run, minutes long, instead of in one call; the CUDA
  kernel has no check until a supervised GPU regtest.
- The proposal still promises per-routine unit tests for D4; that text goes to the owner and the
  proposal's reviewer (ADR-0010, *Consequences*).
