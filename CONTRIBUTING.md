# Contributing

`AGENTS.md` is the operative summary of this guide for coding agents (and a short one for people):
the invariants, where a change belongs, and the gates. This file is the longer form.

## Who this repository is for

The first readers of this repository are scientists: PhD researchers, postdocs and independent
researchers working on ocean wave modelling, numerical methods or HPC. Every document, skill, agent
and piece of metadata here is written for them, unless a section below names a different reader.
The repository is read as a book in progress on agentic coding and agentic research
([ADR-0003](docs/ADRs/ADR-0003-repository-is-a-book.md)), which adds a second reader without
lowering the bar: a researcher or engineer from another field who wants to run coding agents on
scientific code without losing the answer. A chapter is wrong if a wave modeller reading over the
shoulder finds a mistake in it.

- Write for a specialist. Do not gloss the action balance equation or what a source term is; give
  the equation, the routine and `file:line`, the source, and the measured number with its unit,
  hardware and the command that reproduces it.
- Claims carry evidence: `(v)` with what was checked, `⚠` when it was not. A researcher will try to
  reproduce the number, so the command must be in the repo.
- Citation and discovery target academic search: a DOI on every release, the author's ORCID,
  keywords a wave modeller or HPC researcher would type, and references by DOI where one exists.
- Two readers are explicit exceptions. The proposal (`pubs/proposal/`) is read by a DEL committee
  with no oceanography background, and `/eli5` serves newcomers from other fields. Both still have
  to be correct enough that a specialist reading over the shoulder finds nothing wrong.

## What helps most

Corrections are very welcome, especially the following, in order of usefulness:

1. **Anything marked `⚠`.** Those are places I could not verify a claim. If you
   have run it and know the answer, that's the highest-value fix in the repo.
2. **Namelists that don't actually work.** The `.nml` files here were written
   against the annotated upstream templates but not executed. If `ww3_grid`
   rejects one, please say which block and paste its stdout.
3. **Stale links in `docs/AWESOME-WW3_202609.md`.** Entries marked `(v)` were fetched on
   2026-09-11; unmarked ones are from memory and may be wrong.
4. **The wind direction convention in `examples/01`.** Deliberately left as an
   exercise, but a confirmed answer with the WW3 version you used is welcome.
5. **WW4 status.** `course/14-ww4-and-the-future.md` quotes a repository snapshot from
   2026-10-07 (issue #49) and a timeline from NCEP Office Note 525. That will go stale faster than
   anything else here. Updates very welcome, with the date you checked; `just ww4-status`
   shows what changed since the last snapshot.

The issue forms (*New issue* on GitHub) ask for what each kind of report needs: a **Measurement**
asks for the command, commit, machine and L1 status; a **Spike** for one question, a time box and
its output; a **WFIP idea** for the proposal deliverable it moves.

## The proposal (`pubs/proposal/`)

Every change to `pubs/proposal/` goes through the `revisor-proposta` subagent before the pull
request: it reviews against the Escola Politécnica norm (Resolução 05 de 28/11/2012) and the DEL
proposal structure, ABNT citation practice (NBR 10520 and NBR 6023), impersonal scientific register
in pt-BR, and the wave-modelling and HPC vocabulary. It reports, it does not rewrite.

- `.claude/agents/revisor-proposta.md` is the reviewer; `just proposal-review` runs it, and
  `.claude/hooks/proposal-review.sh` reminds any agent that edits a file under `pubs/proposal/`
  to run it before finishing (`--self-test` checks the hook without Claude Code).
- `pubs/proposal/pt/` is the reference text and `en/` its mirror: a change in one is made by hand
  in the other, and `pubs/proposal/.translation-cache.json` is re-stamped so `just translate`
  does not overwrite the reviewed Portuguese.
- Placeholders such as `(REFERÊNCIA)` or `(CITAR ...)` are defects, not notes: fill them with a
  fetched source before the PR.
- `just proposal-lint` (CI runs it too) checks what a model reviewer misses: pt and en cite the same
  sources and carry the same numbers, no placeholder or decimal point in the Portuguese, and each
  acronym is expanded at first use. The reviewer gets its output, and `--record` refuses a review
  that quotes text the proposal no longer contains.
- The proposal is read by a DEL committee with no oceanography background: gloss a wave term in a
  few words the first time it appears; `humanizar` (pt-BR) and `humanizer` (en) catch AI tells.

## Port order is wall time

Routines are ported in descending order of the **measured wall-clock time** they take in the
operational case, most expensive first. Nothing else sets the order: not how easy a routine
is to validate, not whether it already has a fixture, not how well it maps to a GPU. The decision
and the alternatives weighed are
[ADR-0002](docs/ADRs/ADR-0002-port-order-wall-time.md).

- The measurement is the phase-0 profile (`docs/AGENTS_KOKKOS_202609.md` §2.4, task P0.1 in
  issue #42): inclusive and exclusive wall time per routine on 1, 4 and 16 ranks, committed to
  `kokkos/PORT_STATUS.md` with the command that produced it.
- Until that profile is committed, the order follows the best evidence there is, marked `⚠`:
  the gprof self times inside the source terms of `regtests/ww3_ts1` in
  `docs/data/ww3_ts1_gprof_202610.md` (`W3SDS4` first, then `W3SNL1` and `W3SIN4`; transcribed
  from #45 until its task T1 reproduces them), and the published shares in `AGENTS_KOKKOS` §2.1
  for everything outside the source terms. The profile replaces both, and the queue is re-sorted
  the day it lands.
- A routine that cannot start yet because of a dependency (`W3SRCE` needs its source terms
  first) keeps its place, and the next routine down starts in the meantime.
- Experiment arms (Triton, #45) follow the same rule: they target the routine at the
  top of the wall-time ranking, not the one that is cheapest to try.

## Figures

Diagrams are Mermaid fences in the page they illustrate, so GitHub draws them in place. Each one
starts with `%% figure: <id>` and `%% title: <question>` and is followed by a *How to read this
figure* card (takeaway, how to read, what is not shown, evidence) for readers new to the topic.
`just figures` renders every fence with a pinned mermaid-cli, fonts and one house style
(`pubs/figures/mermaid-config.json`) into `pubs/figures/mermaid/`,
which the PDF and Word builds use, and rewrites the gallery `pubs/figures/README.md`; CI runs
`python3 scripts/figures.py check`. The `figure` skill (`.claude/skills/figure/`) has the rules.
A mistake in a figure is as welcome as one in the text: the gallery links an issue for each.

## Ground rules

- Keep the `⚠` / `(v)` convention. Marking uncertainty honestly is the point.
- Don't vendor WW3 source. Scripts fetch it.
- The third-party skills under `.claude/skills/` (`humanizer`, `humanizar`, `ponytail*`, `sharingan`) are pinned
  in `.claude/skills/skills.lock`, and the audience paragraph each one carries (for `sharingan`,
  also the paths that point at this repository's homes instead of marola's) is the
  `.claude/skills/<name>.patch` the lock names, re-applied on every update. Edit the patch, not the
  copy. `sharingan` is the skill that does this for a new pattern: given a URL it fetches the unit
  pinned, reads the licence, maps each upstream concept to a home here and writes the lock entry
  and the patch. `skills-vendor check` (marola-devkit's `scripts/skills_vendor.py`, run as
  `python3 skills_vendor.py check --lock .claude/skills/skills.lock`) verifies the copies;
  `.github/workflows/skills.yml` opens a weekly update PR, and `skills-vendor update <name>` does
  the same by hand.
- A number a table quotes lives as a JSON record in `bench/results/` (machine, toolchain, commit,
  L1 status, each run's command); the table is generated from it with `just results table`, and
  `just results check` (CI) refuses a timing whose L1 did not pass. Anything tried that did not
  become a number in a table, failures included, goes in a dated entry in `docs/log/`.
- `codemeta.json` is generated from `CITATION.cff` and `.zenodo.json` (`just codemeta`); never
  edit it by hand.
- Run `just kokkos-test serial-debug` after touching `kokkos/`, and `bash -n` plus
  shellcheck on any shell script you touch. CI does all of these (the `kokkos` and
  `lint` jobs in `.github/workflows/ci.yml`).
- Prose style: plain, direct, no filler. If a sentence doesn't teach something,
  cut it. No em dash where a comma, a colon, parentheses or a full stop does; a lone `—` as the
  "no value" mark of a table cell, and the en dash of a range (`1–5`), are fine.
- `just vale` is the prose gate CI runs on the English Markdown (`.vale.ini`: the em dash and the
  `vale-ai-tells` rules that are never right here, such as closing pleasantries and sycophancy);
  it must pass. `just vale --report` lists every other `vale-ai-tells` rule as a suggestion for a
  person to weigh, never to apply blindly: a flagged "dynamic" is often a scheduling policy.
  Out of scope: the submodules, `.claude/`, `pubs/` (its own reviewer) and Portuguese files.
- Code comments are of three kinds, and only these: a *why* (a rejected alternative, an external
  constraint), a *trap* (what breaks if the line changes), or a *pointer* (an issue, a plan, a
  `file:line`). A comment that restates the code or narrates the history of a fix is deleted in
  review. Verbatim upstream text (`kokkos/tests/fixtures/snl1_ref.F90`) keeps its comments as
  provenance.
- Length is not a measure of a research doc (the proposal, the course, `docs/`). A Portuguese
  and an English version, or a revision and the text it replaces, may differ in length, and
  that is not a defect: do not pad or trim a text to match another one. What must match across
  languages is content, the same claims, citations and numbers (`just proposal-lint` checks
  this for the proposal). Cut a sentence because it repeats or teaches nothing, never to hit a
  word count.

## Designing a change before building it

A non-trivial change (a new forcing or example, a port, a measurement campaign, a tool the lab
will rely on) starts as a Wave Forecaster Improvement Proposal in `docs/WFIPs/`: a numbered design
doc on marola's MIP shape, tied to one of the proposal's six deliverables and carrying its
definition of done as the checklist of its §7. `just wfip new <slug> --title "…" --deliverable D5`
creates one; `just wfip index` regenerates the index, coverage and graph in
[`docs/WFIPs/README.md`](docs/WFIPs/README.md) from the files; `just wfip check` is the CI gate.
Implementation is a separate PR that ticks the boxes. `just specify init` sets a clone up for
GitHub's Spec Kit when a spec (`specs/<NNN-slug>/spec.md`) is worth writing; the
[`wfip` skill](.claude/skills/wfip/SKILL.md) has the steps, and `just wfip status --since <tag>`
prints what moved for the release notes.

## Opening a pull request

Write the commit message properly (subject, a body paragraph saying what and why, and
`Tested:` / `Cost:` trailers in the final block of the message), then `just pr`: it pushes the branch and creates the PR with a description
generated from the commits (`just uprd` regenerates it later). A PR opened from the GitHub UI
gets the same treatment from `.github/workflows/pr-body.yml`. Delete the first `<!-- uprd -->`
line of a description to hand-edit it and keep it.

Name the branch after the work (`claude/<issue-number>-<short-kebab-slug>` for an agent's branch).
The same workflow fails a PR whose head branch matches `^claude/project-thread-`, a generic session
name that says nothing about the change in the PR list or in `git log`; push the commits to a named
branch (`git push -u origin HEAD:claude/<issue>-<slug>`) and open the PR from there.
