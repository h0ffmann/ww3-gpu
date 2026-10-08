# WFIP template

A Wave Forecaster Improvement Proposal (WFIP) is the design doc of a non-trivial change to this
lab, written before it is built, and the unit the project reports progress in: every WFIP names
the proposal deliverable it serves (`README.md`, "Proposal deliverables"), and its §7 is the
definition of done that `scripts/wfip.py` reads into the index. The shape is marola's MIP
(`marola-dev/marola`, `docs/MIPs/TEMPLATE.md`), renamed and given the deliverable and the DoD.

Copy the block below to `docs/WFIPs/WFIP-NNNN-<kebab-slug>.md`, or run
`just wfip new <slug> --title "<title>" --deliverable D5`, which does it with the next number.

Before the first push:

- **Number**: one more than the highest WFIP in `README.md` *and* in every open PR that adds a
  `docs/WFIPs/WFIP-NNNN-*.md`. Two drafts never share one.
- **Branch**: `claude/<issue>-wfip-NNNN-<slug>` for an agent, `wfip-NNNN-<slug>` for a person.
- **Index**: `just wfip index` regenerates the tables in `README.md` from the WFIP files; CI
  runs `scripts/wfip.py check` and fails on a stale index, an unknown deliverable or a missing
  section. The index is never edited by hand.
- **Draft until done**: the PR stays a draft until every row of *Readiness* is filled.
- **An idea with no issue is not work yet** (`AGENTS.md`): a WFIP is the design; filing the issue
  that makes it work is a person's act, recorded in the *Issues* row.

Every section is required: write "None" rather than deleting a heading. §10 is skipped on
purpose, so §11 is *Open questions* in every WFIP. Keep it under ~250 lines; research goes in
the appendix. Every external fact carries `(v)` with the URL and date, or `⚠`.

```markdown
# WFIP-NNNN: <Title>

| | |
|---|---|
| **Status** | Draft / Accepted / Implemented / Rejected / Superseded by WFIP-NNNN |
| **Author** | <the person who asked for it and owns it> |
| **Created** | YYYY-MM-DD |
| **Deliverable** | `D1`..`D6` from `README.md`, comma-separated, or `none — <why>` (read by `wfip.py`) |
| **Related** | the issue it came from, the plan section, other WFIPs |
| **Effort** | S / M / L / XL — one clause why |
| **Gain** | `proposal` (moves a deliverable), `science` (answers a question), `lab/dev-loop`, `outreach`; one clause each |
| **Effort vs Gain** | `do next` / `do when X lands` / `cheap win` / `expensive, defer` / `park` — one sentence why |
| **Depends on** | prose: other WFIPs, data access, a machine, a person's decision |
| **Blocked by** | WFIP numbers that must merge first, comma-separated, or `none` (read by `wfip.py`) |
| **Risk** | the one thing most likely to make this not worth it |
| **Cost so far** | summed `Cost:` trailers of its merged PRs, or `—` |

### Readiness

| | |
|---|---|
| **Manually reviewed** | `yes — <person>, YYYY-MM-DD`, once a person has read the whole WFIP; `no` until then |
| **Written by** | `<person>, with an agent` or `<person>, by hand` (no model names in the repo) |
| **Tasks** | `WFIP-NNNN.tasks.md`, or `none needed — <why>` |
| **Tests** | the named tests §7 adds, or `none — <why>` |
| **Spec-kit** | `specs/<NNN-slug>/spec.md`, or `none` |
| **Issues** | `h0ffmann/ww3-gpu#N` per task, filed by a person once Accepted; `not filed — Draft` before that |

## 1. Summary
Two to four sentences: what changes, for whom, and why now.

## 2. Motivation
The concrete gap. Quote real output, a real number or a real limitation, with `(v)`/`⚠`.

## 3. What changes for the reader
Before and after, as the actual output (a table, a command's output, a figure), not a description.

## 4. Sources and dependencies reviewed
One subsection per data source, tool or machine: format, cadence, coverage, licence, access,
what was verified (URL and date) and what was not. End with the pick and why.

## 5. Design
Files touched, the commands, what runs where (CPU, GPU, which machine), and what is deterministic.
Enough detail that the implementing agent has nothing to guess. A flow gets a Mermaid figure
with its reading card (the `figure` skill).

## 6. Parity and physics impact
Does the result change the model's answer? Which L1/L2 tolerance applies, which fixture point is
added, or "None — it does not touch a kernel".

## 7. Verification plan and definition of done
The checks, each with the command that runs it. The definition of done is the checklist below;
`wfip.py` counts its boxes into the index, so every box is one verifiable outcome, in the past
tense once ticked:

- [ ] <outcome>: `<command>`
- [ ] <outcome>: `<command>`

## 8. Risks, limitations, and honest caveats

## 9. Alternatives considered
Including "do nothing", and why each lost.

## 11. Open questions
Each question carries the author's answer for now, so nothing is left open by omission:

- <question> **Default:** <what this WFIP does until someone decides otherwise, and who decides>.

## Appendix
### Checked live
One line per external fact fetched: URL, date, what it returned.

### Not checked
Anything referenced but not verified this session.
```
