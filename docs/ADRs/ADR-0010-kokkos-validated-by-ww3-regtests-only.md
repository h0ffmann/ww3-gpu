# ADR-0010: The Kokkos port has no tests of its own; WW3's regtests are its only tests

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-14 |
| **Deciders** | Hoffmann |
| **Written by** | Hoffmann, with an agent |
| **Scope** | How a ported routine is validated: the L1 unit tests of the port (`kokkos/tests/L1_test_snl1_*`, `shim_roundtrip`, their fixture generators), the `googletest` column of `kokkos/ports.tsv`, the coverage measurement, and the parity rule behind a timing. Not the tests of the course's intro programs or of the lab's tools (`nccmp-tol`, `bench_case`, `fetch_analyse`), which test no port. Not the proposal's text |
| **Related** | [WFIP-0003](../WFIPs/WFIP-0003-kokkos-regtest-matrix.md) (the WW3 regtest matrix that replaces the L1 evidence); [`docs/AGENTS_KOKKOS_202609.md`](../AGENTS_KOKKOS_202609.md) §1.5 (the validation contract this changes); [`AGENTS.md`, "Porting"](../../AGENTS.md#porting-hard-rule); [ADR-0002](ADR-0002-port-order-wall-time.md) (port order, unchanged) |

## Context

The owner, 2026-10-14, verbatim: "implement WFIP-003, now, A single PR, step by step, use
subagents, now kokkos code should NOT have its own tests, hard rule. All tests should come com WW3
regtests". Asked what happens to the existing tests, the owner chose to remove the port's own tests
and keep those that test no port.

Until that day a port was "done" with an L1 GoogleTest against a fixture captured from the verbatim
Fortran, then an L2 replay inside WW3 (`AGENTS_KOKKOS_202609.md` §1.5, items 3 and 4). For
`W3SNL1` and `INSNL1` the L1 tests showed bit-identity, max relative error 0.0, on serial-debug and
openmp-release `(v)` (`just kokkos-test`, 14/14, 2026-10-14) and on cuda-release `(v)`
(`bench/results/snl1-i9-14900-rtx4090-2026-09-15.json`, `l1`). The L2 replay ran one WW3 regtest,
`ww3_ts1`, judging `hs` only `(v)` (CI health h1, 2026-10-15 01:34 UTC, the evening of 14 October in Brazil).

The L1 fixture is the lab's own capture of the Fortran, a second copy of the reference
(`snl1_ref.F90`) next to WW3's; WW3's regtests are the cases NOAA runs on the code as it ships.

## Decision

**Kokkos code in this repository has no tests of its own. A ported routine is validated only by
WW3's regression tests, run inside WW3 built from the h0ffmann/WW3 fork with the routine on Kokkos,
and compared with the same WW3 built without it.** The tests are `scripts/hoff.sh --hoff --cpu`,
pair to pair with `--ww3` (CI health h1), and the matrix of WFIP-0003 (`scripts/hoff_matrix.sh`):
every serial `NL1` entry of `regtests/bin/matrix.base`, compared by WW3's `test.comp` and by
`nccmp-tol` within `kokkos/tools/nccmp-tol/tolerances.txt`. If WW3's regtests agree for the
Fortran routine and for its Kokkos translation, the routine is ported. They run on the CPU (Kokkos
on OpenMP): no `--hoff --gpu` run is part of validating a port (`AGENTS.md`, "Running on the
GPU").

In this repository that means:

1. The port's L1 tests, the `shim_roundtrip` driver and the fixture generators (`snl1_ref.F90`,
   `snl1_sea_state.F90`, `gen_snl1_fixture.F90`, `gen_snl1_ww3lib.F90`) are deleted, with the
   `snl1-fixtures` and `l1-crosscheck` recipes. The committed `snl1_nk25_nth24.bin` stays, frozen,
   as the input of the kernel benchmark `ww_bench_snl1` and of `proof/snl1_cons`.
2. The status ladder of `kokkos/ports.tsv` is `ported → L2 → gpu`; `L1` is gone. The `googletest`
   column is removed; `regtested` stays and is required.
3. `scripts/kokkos_coverage.py` and `docs/data/kokkos_coverage.tsv` are removed: they measured
   what CTest runs of the port, and CTest no longer runs the port.
4. "No timing without parity" now means a passed WW3 regtest replay on the same commit. A
   `bench/results/` record carries `regtest: passed: <command>`; the record of 2026-09-15, whose
   parity was the L1 of its day, stays as history.
5. A new port (`/port-next`) scaffolds the kernel, the shim, the port report and the ledger row,
   and no test file.

## Alternatives considered

| Option | Why not |
|---|---|
| Keep the L1 tests beside the regtests | The owner's rule; and two references of the same Fortran drift apart unseen |
| Keep them, but stop adding new ones | Offered; the owner chose removal |
| Remove every CTest, the intro and tool tests too | Offered; those test a lesson or a tool, not a port, and stay |

## Consequences

- One reference: the Fortran WW3 ships, through the harness NOAA uses. No second copy to keep in
  step.
- The bit-identity evidence of the kernel alone is history, not a gate. L2 is not byte-identical
  even on `ww3_ts1` (`docs/PORT_ASSESSMENT_202610.md` §4), so the remaining check is a tolerance
  check, and it judges only the fields each regtest writes.
- A defect in the kernel shows up later and larger: a regtest run is minutes, an L1 test was a
  fraction of a second, and a failure points at a model run, not at one call.
- The CUDA kernel has no check at all until a regtest runs on the GPU, which is the owner's,
  supervised (`AGENTS.md`, "Running on the GPU").
- The proposal promises "per-routine unit tests with captured inputs" for deliverable D4
  (`docs/WFIPs/README.md`, "Proposal deliverables"). That text is not changed here; it goes to the
  owner and the proposal's reviewer.
- The course chapters that taught the L1 tests (lessons 07, 08, 10 to 14), the glossary, the
  dated plans and the port reports are rewritten to this rule in the same change, keeping the L1
  results as dated history ([`docs/log/2026-10-14-kokkos-regtests-only.md`](../log/2026-10-14-kokkos-regtests-only.md)).

## Revisit when

- A regtest on the GPU is supervised and passes: then the `gpu` status is reachable again.
- The matrix finds a defect that a test of one call would have located faster.
- WW3 itself gains unit tests a port could run against.
