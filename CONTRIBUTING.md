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
asks for the command, commit, machine and parity status; a **Spike** for one question, a time box and
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
- Don't cite a third party's issues or pull requests: no `NOAA-EMC/WW4#72`, no link to a
  NOAA-EMC/WW3 issue, no "WW4 PR #67", in a file, a commit, or an issue or pull request of this
  repository: GitHub turns each one into a backlink on the upstream item. Describe what it contains
  at a fixed point instead: the code at a commit, the merge date, a release, a DOI
  (`AGENTS.md`, "Third-party issues and pull requests").
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
  parity status, each run's command); the table is generated from it with `just results table`, and
  `just results check` (CI) refuses a timing without a passed parity check: `regtest: passed`
  (ADR-0010), or the `l1: passed` of a record made before 2026-10-14.
- The Kokkos port has no tests of its own (ADR-0010): no L1 test, GoogleTest or fixture of a ported
  routine. Its tests are WW3's regtests, `just smoke-ww3-gpu`, `just full-ww3-gpu` and `just ww3-gpu-matrix`,
  on the CPU. They run at checkpoints, not after each routine: a port moves in units such as all
  of ST4, and between checkpoints bit parity is argued from the `-O3` GIMPLE of the Fortran and
  the C++ (ADR-0011). `just ww3-gpu-matrix` is a CMake/CTest driver in `regtests/` (WFIP-0003), the one set of
  recipes that does not wrap a script in `scripts/`: one CTest test per `matrix.base` entry, each
  WW3's own regtest run with the Fortran routine and with the Kokkos routine; CTest is the runner,
  not a test of the port's own. A change to `regtests/` runs `just ww3-gpu-matrix-selftest` and
  `just ww3-gpu-matrix-list`. Anything tried that did not become a number in a table, failures included, goes in a dated entry in `docs/log/`.
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
  review. Verbatim upstream text (the WW3 section comments `snl1_dia.cpp` keeps) keeps its comments
  as provenance.
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

## Is WW3 deterministic enough for bit for bit

AGENTS.md, "Bit for bit with the Fortran", excuses only a file that two runs of the Fortran
itself write differently. Is WW3 deterministic enough for this rule? Measured on the owner's
workstation, CPU only:

- (v) Serial, OpenMP and MPI runs reproduce: the Fortran and Kokkos DIA of one executable gave
  byte-identical output on the 100 serial and 25 OpenMP `NL1` entries
  ([`docs/data/hoff_matrix_2026-10-15.tsv`](docs/data/hoff_matrix_2026-10-15.tsv),
  [`docs/data/hoff_matrix_omp_2026-10-15.tsv`](docs/data/hoff_matrix_omp_2026-10-15.tsv)), which
  needs each run to be reproducible; `ww3_multi`, MPI and hybrid MPI/OpenMP entries were
  byte-identical in the pilots of 2026-10-15 (`docs/log/2026-10-15-snl1-regtest-all.md`).
- (v) Not everywhere: with PDLIB under MPI (`ww3_tp2.17 -g d`, 4 ranks), two runs of the same
  Fortran executable wrote 25 of 67 files differently, all `restart.ww3`, while every field
  output was identical. That is what `ww3_self` exists for. The cause is not traced ⚠; the
  bytes look undefined rather than computed (one run wrote 0 where the other did not).
- (v) WW3 has a data race of its own under `OMPG`: `IKN`, `ALLOCATABLE, SAVE`, allocated inside
  the parallel region in `W3SWL6` (`model/src/w3swldmd.F90:359-377`). It aborts a run; it does not
  change bits. Rerun, record, never edit WW3.

## Skills and agents

AGENTS.md, "Skills and agents", has the rules (front matter that parses, the audience paragraph,
edit a vendored skill through its patch); this is what each one is for.

| Name | Job |
|---|---|
| `figure` | add or change a Mermaid diagram and its reading card; `figures.py` is its harness |
| `eli5` | explain a topic to someone from another field (the second audience exception) |
| `release` | cut a citable release: tag, GitHub release, Zenodo DOI, `CITATION.cff`, `.zenodo.json` |
| `ww4-status` | diff NOAA-EMC/WW4 against `docs/ww4-status.json` and refresh lesson 14 and the proposal |
| `noaa-notices` | list NOAA notices (NWS PNS/SCN, NCEP model changes, NOAA-EMC/WW3 production branches) new since `docs/noaa-notices.json` and route each to the file it breaks or dates |
| `ww3-port` | the WW3-specific traps of a Fortran-to-Kokkos translation (gfortran `-O3` powers, array bases, per-grid tables, OpenMP nesting) and of claiming a routine regtested; follows `/port-next` |
| `wfip` | write, revise or report on a WFIP; `scripts/wfip.py` is its harness and `just specify` wires GitHub's Spec Kit (its `/speckit-*` skills are generated per clone and not tracked) |
| `humanizar`, `humanizer` | remove AI tells from pt-BR and English prose without changing facts; third-party |
| `ponytail`, `ponytail-review`, `ponytail-audit` | the laziest solution that works; over-engineering review and audit; third-party |
| `routing` | find where something is in one bounded call (`scripts/route.py`): a glossary term, a definition, the pages that name a file, the paragraph that answers a why, one section of a plan |
| `sharingan` | port a skill, workflow or pattern from another repository by URL: fetch it pinned, let the licence decide vendor, adapt or rewrite, map each upstream concept to a home here; third-party (marola-devkit) |
| `revisor-proposta` (agent) | the proposal's scientific reviewer, read-only |
| `security-reviewer` (agent) | security review of a branch's diff (secrets and personal data in git, hooks, prompts, workflows, scripts), read-only; run it before a PR that touches `.claude/`, `.github/workflows/` or `scripts/` is marked ready |

ponytail is also enabled as a Claude Code plugin (`ponytail@ponytail`, `.claude/settings.json`,
pinned to `v5.2.0`) for its commands (`/ponytail:ponytail-debt`, `-gain`, `-help`) and the
statusline. Its mode is off by default (`PONYTAIL_DEFAULT_MODE=off` in the same file, the owner,
14 October 2026): its rules cover a whole session and have no setting to limit them to some files
or languages, and they pull against "Translate, don't improve" (v) (3536 characters injected with
`full`, nothing with `off`; `docs/log/2026-10-14-ponytail-plugin.md`). A person turns it on for a
session with `/ponytail`, for C++ around the port; even then "the smallest change" never licenses
a refactor, a vectorisation or a "simplification" of a ported routine. `PONYTAIL_MAP=0` turns off
its codebase map, which lists Python and JS only, never C++, CUDA or Fortran (v)
(`hooks/ponytail-map.js:17` at 5.2.0). README.md, "ponytail, off by default", is the user's guide.
The vendored skills above stay the ones that carry this repository's audience paragraph.

## Working with Antigravity

Google's Antigravity (the IDE, Antigravity 2.0 and the `agy` CLI) reads the root `AGENTS.md` as an
always-on rule and workspace skills from `.agents/skills/<name>/SKILL.md` `(v)` 2026-10-09
([rules](https://antigravity.google/docs/rules), [skills](https://antigravity.google/docs/skills)).
`.agents/skills` is a symlink to `.claude/skills`, so both harnesses load the same `SKILL.md`
files; edit the copy under `.claude/skills/`, never through the link. There is no `GEMINI.md`: it
would take precedence over `AGENTS.md`. Antigravity workflows (`.agents/workflows/`) are not used,
because Antigravity stops running them on 2026-10-19 in favour of skills `(v)` 2026-10-09
([workflows](https://antigravity.google/docs/ide/workflows/)). Gemini CLI stopped serving Google AI
Pro and Ultra accounts on 2026-06-18; on those plans `agy` is its replacement `(v)` 2026-10-09
([Google Developers Blog](https://developers.googleblog.com/en/an-important-update-transitioning-gemini-cli-to-antigravity-cli/)).

What Antigravity does not get:

- **The Claude hooks.** The proposal-review reminder and Stop gate, and the cloud `SessionStart`
  setup, run only in Claude Code. The checks behind them run in CI whatever wrote the change:
  `proposal-review.yml` refuses a proposal change without a recorded review, and `ci.yml` runs
  `check_agent_frontmatter.py` and both hooks' `--self-test`. Run `scripts/agent_env.sh` yourself
  in a fresh container.
- **The `revisor-proposta` subagent.** It is a Claude Code agent definition; from Antigravity, ask
  the agent to apply `.claude/agents/revisor-proposta.md` to the changed files, then record the
  parecer with `just proposal-review-record`.
- **Claude-only tools named in a skill** (the Agent tool, `ToolSearch`). A skill that needs one
  says what it is for; do that step by hand.

Keep it cheap. Antigravity's Individual tier is free with weekly limits that have been cut since
launch, and Google AI Pro raises them ⚠ (limits not published as numbers; check
[pricing](https://antigravity.google/pricing) before relying on them). Send small lookups to the
local model (`just ask "…"`), and run builds and regtests through their `just` recipes so the model
reads the result, not the compiler output. Google AI Pro also carries $10 a month of Google Cloud
credit through the Google Developer Program `(v)` 2026-10-09
([Google blog, 2026-01-27](https://blog.google/innovation-and-ai/technology/developers-tools/gdp-premium-ai-pro-ultra/));
it must be activated there, and it does not cover a GPU for the Kokkos CUDA build.

| Task | Surface | Usage against the weekly limit | Date |
|---|---|---|---|
| `/ponytail-review` of one PR diff | `agy` in Cloud Shell, Google AI Pro | ⚠ not measured yet | |

Running `agy` inside `just dev` or ai-jail has not been tried ⚠.

## Working with Kimi Code

Kimi Code (the desktop app, 1.1.0) loads, from the repository root down to the working
directory, `.kimi-code/AGENTS.md` and then `AGENTS.md`, and skills from `.kimi-code/skills/` and
`.agents/skills/` `(v)` 2026-10-16 (read in the app's own bundle, `resources/app.asar`:
`loadAgentsMdForRoots`, `skillRoots.ts`). It does not read `CLAUDE.md`, `.claude/skills/` or
`.claude/commands/`, and it warns when the loaded instructions pass 32 KB; this repository's two
files together are past that. So the root `AGENTS.md` stays the rulebook, the skills reach Kimi
Code through the same `.agents/skills` link Antigravity uses, and
[`.kimi-code/AGENTS.md`](.kimi-code/AGENTS.md) holds only what Kimi Code lacks: where to read
first, and the hooks, subagents and slash commands it must do by hand. Lookups go through the
[`routing`](.claude/skills/routing/SKILL.md) skill, a question-to-source map read on demand, and
its `scripts/route.py`: because the two auto-loaded files already pass the 32 KB warning,
retrieval detail stays out of them.

## Opening a pull request

Write the commit message properly (subject, a body paragraph saying what and why, and
a `Tested:` trailer in the final block of the message; never a `Cost:` trailer), then `just pr`: it pushes the branch and creates the PR with a description
generated from the commits (`just uprd` regenerates it later). A PR opened from the GitHub UI
gets the same treatment from `.github/workflows/pr-body.yml`. Delete the first `<!-- uprd -->`
line of a description to hand-edit it and keep it.

Name an agent's branch after its goal: `goal/<id>-<short-kebab-slug>`, where `<id>` is the issue
number (`goal/143-h100-access`) or, with no issue, the last 8 characters of the Claude project
thread id plus a random 4-digit number (`goal/ke1RYpdP-7316-branch-naming`); a session outside a
Claude project uses the last 8 characters of its session id. The first commit and the PR body
name the goal id and link the Claude project thread it came from, so the work can be traced back
to the conversation. `scripts/branch_name.sh` refuses a `claude/…` or `claude-project…` branch,
a generic session name that says nothing about the change, and a `goal/` name without an id; `just
pr` and the same workflow run it. Push the commits to a named branch (`git push -u origin
HEAD:goal/<id>-<slug>`) and open the PR from there.
