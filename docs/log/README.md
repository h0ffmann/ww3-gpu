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
