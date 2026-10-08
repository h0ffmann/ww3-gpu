# WFIP index

Wave Forecaster Improvement Proposals: the design docs of this lab's non-trivial changes, written
before they are built, each tied to a deliverable the project proposal promised
(`pubs/proposal/pt/06-objective.md`, `08-schedule.md`). The shape is [`TEMPLATE.md`](TEMPLATE.md);
the process is the [`wfip` skill](../../.claude/skills/wfip/SKILL.md); `scripts/wfip.py` generates
every table below from the WFIP files and CI fails when they are stale (`just wfip check`).

Statuses: Draft → Accepted → Implemented (or Rejected / Superseded). A WFIP built in stages
carries `Partially implemented (tasks a–b of N — #PR)` in its own Status row. The **DoD** column is
the ticked share of the checklist in each WFIP's §7, so "Implemented" and "6/6" say the same
thing from two sources.

This index is the project's progress report: each tagged release archives it on Zenodo with the
source, and `just wfip status --since <tag>` prints what changed for the release notes
(`.claude/skills/release/SKILL.md`).

## Proposal deliverables

The six specific objectives of the proposal and the schedule rows that deliver them
`(v)` [`06-objective.md`](../../pubs/proposal/pt/06-objective.md),
[`08-schedule.md`](../../pubs/proposal/pt/08-schedule.md), read 2026-10-08. A WFIP's
**Deliverable** row names one or more of these ids; `wfip.py check` rejects any other.

| ID | Deliverable | Objective | Due |
|---|---|---|---|
| D1 | Frozen operational configuration of each case and a reproducible benchmark with the metric (run time per forecast hour) | 1 | 10–11/2026 |
| D2 | Performance profile of the reference run per routine and per phase, with one and several MPI ranks | 2 | 11/2026 |
| D3 | Measured gain of the compile, configuration and modern-Fortran steps, each with its parity check, and the best build documented for the lab | 3 | 12/2026–02/2027 |
| D4 | The validation infrastructure WW3 lacks: a field-by-field comparator with versioned tolerances and per-routine unit tests with captured inputs | 4 | 11/2026–01/2027 |
| D5 | C++/Kokkos kernels in profile order, validated on CPU against the Fortran, measured on the H100, and the operation decision | 5 | 03–04/2027 |
| D6 | Tools, results and recommendations published so the lab can repeat the measurements; the final report | 6 | 04–05/2027 |

## Proposals

<!-- wfip-index:start -->
| WFIP | Title | Status | Created | Deliverable | Effort | Verdict | DoD | Cost so far |
|---|---|---|---|---|---|---|---|---|
| [WFIP-0001](WFIP-0001-weathernext3-wind-rtx4090.md) | WW3 samples driven by WeatherNext 3 wind on an RTX 4090 box | Draft | 2026-10-08 | D1, D6 | M | do next | 0/6 | — |
<!-- wfip-index:end -->

## Coverage

Which deliverable has a WFIP behind it, and which has none yet. A deliverable with no WFIP is not
late; it means its work has not been designed here yet.

<!-- wfip-coverage:start -->
| Deliverable | WFIPs | Implemented |
|---|---|---|
| D1 | [WFIP-0001](WFIP-0001-weathernext3-wind-rtx4090.md) | 0/1 |
| D2 | none yet | 0/0 |
| D3 | none yet | 0/0 |
| D4 | none yet | 0/0 |
| D5 | none yet | 0/0 |
| D6 | [WFIP-0001](WFIP-0001-weathernext3-wind-rtx4090.md) | 0/1 |
<!-- wfip-coverage:end -->

## Dependency graph

Drawn from the **Blocked by** rows only; **Depends on** is prose and is never parsed.

<!-- wfip-graph:start -->
_No WFIP declares a **Blocked by** relationship yet._
<!-- wfip-graph:end -->
