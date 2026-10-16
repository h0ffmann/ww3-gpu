# Research log

The lab notebook: one dated file per thing tried, `YYYY-MM-DD-<slug>.md`, written the day it
happened by whoever (person or agent) did it. A plan says what we intend (`docs/*_YYYYMM.md`), a
WFIP what we will build (`docs/WFIPs/`), `PORT_STATUS.md` and `bench/` what we measured and kept;
this log is everything in between, **negative results included**: the build that failed, the
tolerance that did not hold, the idea that cost a day and went nowhere. Those are results too, and
the proposal's D6 (`docs/WFIPs/README.md`) promises them as "what the lab can repeat".

Rules:

- **Append, never rewrite.** An entry is not edited after its day except to add a dated
  *Update* at the bottom; a wrong entry is corrected by a new one that links it.
- **Same evidence rule as everywhere** (`AGENTS.md`): every claim `(v)` with what was checked or
  `⚠`; every number with the command, commit and machine. A number here is not in a table; it
  moves to `PORT_STATUS.md` or `bench/results/` only through that file's own rules.
- **Link both ways**: the entry names the issue, PR or WFIP it belongs to, and that issue or PR
  links the entry.

An entry has four headings, kept even when one is "nothing":

```markdown
# <what was tried, as a title>

Date, author, issue/PR/WFIP.

## Question
## What was done   (commands, commits, machine)
## Result          ((v)/⚠; numbers with their command)
## What it changes (next step, or "nothing: recorded so nobody repeats it")
```

## Entries

| Date | Entry | For |
|---|---|---|
| 2026-10-08 | [SPIKE-001 quick wins: research metadata, forms, log, results data, agent environment](2026-10-08-spike-001-quick-wins.md) | #65 |
| 2026-10-11 | [Graphify on WW3 at the pin: raw and preprocessed, code only](2026-10-11-graphify-on-ww3.md) | #68, ADR-0007 |
| 2026-10-14 | [ponytail enabled as a Claude Code plugin, off by default](2026-10-14-ponytail-plugin.md) | AGENTS.md skills |
| 2026-10-14 | [ai-memory 3.0.0 installed on the owner's workstation](2026-10-14-ai-memory-install.md) | ADR-0009 |
| 2026-10-14 | [The Kokkos port's own tests removed; WW3's regtests are its only tests](2026-10-14-kokkos-regtests-only.md) | ADR-0010, WFIP-0003 |
| 2026-10-14 | [Graphify on ww3-gpu's own code: build cost, query cost, what it misses](2026-10-14-graphify-on-ww3-gpu.md) | ADR-0007 revision |
| 2026-10-15 | [ST4 translated to Kokkos, eight routines in parallel, not regtested](2026-10-15-st4-kokkos-translation.md) | #45 |
| 2026-10-15 | [W3SRCE translated to Kokkos with its three Fortran source terms, not regtested](2026-10-15-w3srce-kokkos-translation.md) | #42 |
| 2026-10-16 | [Propagation (PR3 with UQ) translated to Kokkos, six routines, nothing run](2026-10-16-propagation-kokkos-translation.md) | #42 |
| 2026-10-15 | [The ST4 checkpoint, first half: byte-identical on ww3_ts1 and bench_small, the matrix running](2026-10-15-st4-checkpoint.md) | #45, ADR-0011 |
| 2026-10-14 | [Matrix entry 16 (`ww3_ts1 -g ST4_T500`): the Kokkos DIA fails `nccmp-tol`, and the Fortran alone does too](2026-10-14-hoff-matrix-entry16.md) | WFIP-0003 |
| 2026-10-15 | [Matrix entries 51 and 54 (`ww3_ts3`, ST1): the same amplification as entry 16](2026-10-15-hoff-matrix-entries51-54.md) | WFIP-0003 |
| 2026-10-15 | [Matrix entries 6 and 10 (`ww3_tp2.15`, 5 km grid and ST6): the same amplification as entry 16](2026-10-15-hoff-matrix-entries6-10.md) | WFIP-0003 |
| 2026-10-14 | [The Kokkos DIA byte-identical inside WW3: `AF11` used the `-O0` chain of `x**11`](2026-10-14-snl1-bitwise-af11.md) | WFIP-0003, PR #120 |
| 2026-10-14 | [The first published ai-memory page, and `memory/`](2026-10-14-memory-publish.md) | ADR-0009 |
| 2026-10-15 | [The OpenMP entries of the regtest matrix: 25 of 25 byte-identical, with nesting allowed](2026-10-15-hoff-matrix-omp.md) | WFIP-0003, PR #120 |
