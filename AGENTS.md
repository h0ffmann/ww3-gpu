# AGENTS.md

Instructions for any AI coding agent working in ww3-gpu (Claude Code or otherwise). Read this
before writing, building or measuring anything. Humans should read it too. `CONTRIBUTING.md` is
the contributor guide and the longer form of several rules here; this file says what an agent must
not get wrong and where the detail lives.

<!-- invariants:start -->
## Repo invariants

Non-negotiable in this repository; a skill, an agent or a task may make these stricter, never
looser.

- **Write for scientists, carry evidence**: the first readers are PhD researchers, postdocs and
  independent researchers in wave modelling, numerical methods and HPC; every claim carries `(v)`
  with what was checked or `⚠` when it was not, and every number comes with the command that
  reproduces it ([Audience and evidence](#audience-and-evidence-hard-rule)).
- **WW3 is read-only**: never vendor WW3 source or edit the Fortran physics; the `WW3/` submodule
  and `$WW3` are references, and the only Fortran this repo adds is a shim behind a switch
  ([WW3 source](#ww3-source-is-read-only-hard-rule)).
- **Translate, don't improve**: a ported routine reproduces the Fortran's arithmetic; a change of
  reduction order, limiter or integration order is a separate PR with its own L2 evidence, asked
  about first, and a tolerance is never widened to pass ([Porting](#porting-hard-rule)).
- **No timing without parity**: a speed figure enters a table only after the L1 gate passed on the
  same build, with the machine, the command and the median of three runs
  ([Porting](#porting-hard-rule)).
- **The proposal goes through the reviewer**: any change under `pubs/proposal/` passes
  `revisor-proposta` and `just proposal-lint` before the PR, and the review is recorded
  ([The proposal](#the-proposal-hard-rule)).
- **Commits carry `Tested:` and `Cost:`**, in the final block of the message, and the PR is opened
  with `just pr` ([Commits and pull requests](#commits-and-pull-requests-hard-rule)).
<!-- invariants:end -->

## What this repository is

An open lab for running WAVEWATCH III® (WW3) and for moving its expensive kernels to GPUs without
changing the answer: a 16-lesson course, a Nix-pinned Fortran/MPI/NetCDF toolchain, a C++/Kokkos
port of the DIA source term (`W3SNL1`) that matches the Fortran bit for bit on Serial, OpenMP and
CUDA, benchmarks, plans, and the UFRJ/DEL project proposal co-advised at LabECO/UFSC. `README.md`
has the study areas and their state; `README.pt-BR.md` is its Portuguese mirror, and a change in one
is made in the other (content must match, length need not; see `CONTRIBUTING.md`, "Ground rules").

Lab code is C++, Fortran and shell. Python appears only in the publishing and repo tooling
(`scripts/*.py`, `tests/`), never in lab code or kernels. `just` is the task runner and every
recipe is a thin wrapper over a script in `scripts/`; `just` lists them.

## Where a change belongs

| Change | Where | Its own rules |
|---|---|---|
| A kernel, its tests, its shim | `kokkos/` | [`kokkos/README.md`](kokkos/README.md), the ledger [`kokkos/PORT_STATUS.md`](kokkos/PORT_STATUS.md), the rules [`docs/AGENTS_KOKKOS_202609.md`](docs/AGENTS_KOKKOS_202609.md) §1 and §3 |
| A measurement | `bench/`, `kokkos/PORT_STATUS.md`, the issue the number answers | [`bench/README.md`](bench/README.md): a row may only claim what a command in the repo reproduces |
| A plan, an evaluation, a survey | `docs/` as `NAME_YYYYMM.md`, dated in the name | this file; the `(v)`/`⚠` convention; `docs/GLOSSARY.md` for every new abbreviation |
| A lesson, an example, an exercise | `course/`, `examples/`, `exercises/` | lessons are numbered and cross-linked; examples use `.nml`, never `.inp` |
| The proposal | `pubs/proposal/pt/` (reference) and `en/` (mirror) | [The proposal](#the-proposal-hard-rule) |
| A diagram | a Mermaid fence in the page it illustrates | [Figures](#figures-hard-rule) |
| A skill or agent | `.claude/skills/<name>/SKILL.md`, `.claude/agents/<name>.md` | [Skills and agents](#skills-and-agents) |
| The toolchain | `nix-config/` (submodule, sparse) and `flake.nix` | [`docs/TOOLCHAIN.md`](docs/TOOLCHAIN.md); pin deliberately, name the version |
| A WW3 fact | nowhere here: cite `model/src/<file>.F90:<line>` at the pinned commit | [WW3 source](#ww3-source-is-read-only-hard-rule) |

Before implementing, check whether the idea is already designed or decided:
[`docs/KOKKOS_H100_PLAN_202609.md`](docs/KOKKOS_H100_PLAN_202609.md) (the single-H100 port),
[`docs/AGENTS_KOKKOS_202609.md`](docs/AGENTS_KOKKOS_202609.md) (how agents port, in which order),
the port planner issue #42, the
`W3SDS4`/Triton issue #45 with its plan
[`docs/W3SDS4_TRITON_PLANO_202610.pt.md`](docs/W3SDS4_TRITON_PLANO_202610.pt.md), and the
port-order rule in #46. An idea with no issue is not work yet; filing is a person's act.

## Setup and commands

```bash
git clone --recurse-submodules git@github.com:h0ffmann/ww3-gpu.git && cd ww3-gpu
just submodule-init          # nix-config, sparse (labs/pratico)
just get                     # upstream NOAA-EMC/WW3 develop into ~/src/WW3 (or set $WW3)
just rt                      # build with ww3_tp1.1's switch and run that regtest
just build                   # the lab switch (switches/switch_lab_shrd, ST4)
just kokkos-test serial-debug
```

Run the gates of what you changed before calling it done. CI (`.github/workflows/ci.yml`,
`proposal-review.yml`) runs exactly these, so a gate skipped locally fails in the PR:

| You changed | Run |
|---|---|
| any `*.sh` or `justfile` recipe | `bash -n <script>` and `shellcheck <script>`; a hook also has `--self-test` (`.claude/hooks/proposal-review.sh --self-test`) |
| `kokkos/` | `just kokkos-test serial-debug` (sanitizers, bounds checks) and `just kokkos-test openmp-release`; `just kokkos-cuda-test` where a GPU exists |
| Fortran in `gpu/`, `examples/`, `exercises/` | they must compile with gfortran with directives ignored; the exercise parity test must pass |
| a Mermaid fence or its page | `python3 scripts/figures.py check`, then `just figures` to re-render and `python3 -m unittest tests/test_figures.py` |
| `pubs/proposal/` | `just proposal-lint`, the `revisor-proposta` review, `just proposal-review-record <parecer>`, `just proposal-review-check` |
| `scripts/*.py` | `python3 -m unittest discover tests` |
| `.claude/skills/` or `.claude/agents/` | `python3 .claude/hooks/check_agent_frontmatter.py`; for a vendored skill, `skills-vendor check --lock .claude/skills/skills.lock` |
| any Markdown | links are checked by lychee in CI; a relative link must resolve from the file |

The toolchain is the pinned `nix-config/labs/pratico` shell (`just ww3`, `just dev`); `just prereqs`
installs a host alternative on Debian/Ubuntu. `just figures` and the publication builds use the
root `flake.nix`. A missing tool is a reason to enter the shell, not to skip the gate.

## Audience and evidence (hard rule)

The first readers are scientists; `CONTRIBUTING.md`, "Who this repository is for", is the full
statement. In practice:

- Do not gloss the action balance equation or what a source term is. Give the equation, the
  routine, `file:line` at the pinned commit, the source by DOI, and the measured number with its
  unit, hardware and command.
- `(v)` marks a claim checked against a source fetched while writing, with the date when it can go
  stale; `⚠` marks one that was not. Never remove a `⚠` without doing the check, and never promote
  a projection to a measurement.
- A number without a command in the repository does not enter a table. "Transcribed" numbers are
  `⚠` until the command exists (`docs/data/` holds such tables and says which task replaces them).
- Two readers are explicit exceptions: the proposal (`pubs/proposal/`), read by a DEL committee with
  no oceanography background, and `/eli5`. Both still have to be correct enough that a specialist
  reading over the shoulder finds nothing wrong.
- Prose is plain and direct. Length is not a measure of a research document: cut a sentence because
  it repeats or teaches nothing, never to hit a word count, and never pad one language's version to
  match the other's. `humanizar` (pt-BR) and `humanizer` (en) are the filters for text a person will
  sign or publish.
- Every abbreviation, switch, routine and tool name used here is expanded in
  [`docs/GLOSSARY.md`](docs/GLOSSARY.md); add the entry with the first use.

## WW3 source is read-only (hard rule)

`WW3/` is the `h0ffmann/WW3` fork of NOAA-EMC/WW3 as a submodule, pinned (`just src-st` shows the
pin); `just get` clones upstream into `~/src/WW3`. Neither is edited from here, and no WW3 source is
copied into this repository (`CONTRIBUTING.md`, "Don't vendor WW3 source. Scripts fetch it."). The
exceptions are a `bind(C)` shim module and the caller patch described in
`kokkos/src/fortran_iface/PATCH.md`, which live on a fork branch, behind a switch, and leave the
original routine selectable. Every statement about WW3 cites `model/src/<file>.F90:<line>` at the
pinned commit (`WW3@761cf79d` at the time of writing), never a comment, a paper or memory.
Namelists are written against the templates in `$WW3/model/nml/` and are `⚠` until executed.

## Porting (hard rule)

The operative rules are `docs/AGENTS_KOKKOS_202609.md` §1.2 (Kokkos), §1.5 (what "done" means)
and the anti-patterns in §1.6; the plan is `docs/KOKKOS_H100_PLAN_202609.md`. The ones that are
broken most often:

- **Translate, don't improve.** Phase 1 reproduces the Fortran's arithmetic: no refactor, no
  vectorisation, no "simplification"; loop nesting changes only where a kernel may not allocate.
  Contraction is off (`-ffp-contract=off`, `--fmad=false`): turning it on is a physics change, not
  a tuning knob, and it invalidates the L1 column (`kokkos/PORT_STATUS.md`).
- **Ask before** changing the order of a reduction, a limiter or the integration order. The change
  is its own PR, with L2 evidence.
- **Never widen a tolerance.** Report and stop.
- **Order follows wall time.** The next routine is the one the committed profile says is most
  expensive, not the one that is easiest to validate or already has a fixture (the rule of #46,
  applied in #45; `W3SDS4` before `W3SIN4`, both after `W3SNL1`).
- **Done means all of §1.5:** the kernel with its heritage header, the shim with its argument
  table, an L1 test with stated and justified tolerances, an L2 regtest replay, a timing line in
  `PORT_STATUS.md`, and a conservation or sign property where physics allows one.
- **Timings** follow `kokkos/tests/bench_snl1.cpp`: three warm-up calls, twenty timed, median of
  three runs, machine named, kernel-only and end-to-end both reported; nothing timed enters a table
  without a passed L1 on the same build.
- **The memory contract is explicit.** Phase 1 copies per call and exists to validate; phase 2
  keeps the spectrum resident. Never use Unified Memory to avoid thinking about transfers, and never
  read the CUDA kernel column as "the model will be this much faster".

## The proposal (hard rule)

`pubs/proposal/pt/` is the reference text and `en/` its mirror; `refs.bib` the bibliography. Every
change passes the `revisor-proposta` subagent (`.claude/agents/revisor-proposta.md`,
`just proposal-review`) before the PR: it reviews against the Escola Politécnica norm (Resolução 05
de 28/11/2012), the DEL structure, ABNT (NBR 10520, NBR 6023), impersonal pt-BR register and the
wave-modelling vocabulary; it reports and does not rewrite. The review is recorded with
`just proposal-review-record <parecer>`; without it the Stop hook and CI refuse. `just proposal-lint`
checks what a model misses: pt and en cite the same sources and numbers, no placeholder, no decimal
point in Portuguese, every acronym expanded at first use. A change in `pt/` is made by hand in `en/`
and `.translation-cache.json` is re-stamped so `just translate` does not overwrite the reviewed
Portuguese. Placeholders such as `(REFERÊNCIA)` are defects, not notes.

## Figures (hard rule)

A diagram is a ```` ```mermaid ```` fence in the page it illustrates, starting with
`%% figure: <id>` and `%% title: <question>`, followed by a *How to read this figure* card
(*Como ler esta figura* in a `.pt.` file): takeaway, how to read, not shown, evidence. The `figure`
skill has the visual grammar; `scripts/figures.py check` fails CI when a fence has no card or its
render is stale, so `just figures` renders and the renders, `index.json` and the gallery
`pubs/figures/README.md` are committed with the page. A bar chart of a measured table goes through
`figures.py chart` so the table and the chart cannot drift. Where a figure and its prose disagree,
the figure is the bug.

## Skills and agents

Skills live in `.claude/skills/<name>/SKILL.md`, agents in `.claude/agents/`. Each restates the
audience rule for its own job; a new one does too, and its YAML front matter must parse
(`check_agent_frontmatter.py`: an unquoted description containing `: ` silently disables it).

| Name | Job |
|---|---|
| `figure` | add or change a Mermaid diagram and its reading card; `figures.py` is its harness |
| `eli5` | explain a topic to someone from another field (the second audience exception) |
| `release` | cut a citable release: tag, GitHub release, Zenodo DOI, `CITATION.cff`, `.zenodo.json` |
| `ww4-status` | diff NOAA-EMC/WW4 against `docs/ww4-status.json` and refresh lesson 14 and the proposal |
| `humanizar`, `humanizer` | remove AI tells from pt-BR and English prose without changing facts; third-party |
| `ponytail`, `ponytail-review`, `ponytail-audit` | the laziest solution that works; over-engineering review and audit; third-party |
| `revisor-proposta` (agent) | the proposal's scientific reviewer, read-only |

The third-party skills are vendored and pinned in `.claude/skills/skills.lock`, and the audience
paragraph each carries is the `.claude/skills/<name>.patch` the lock names, re-applied on every
update. **Edit the patch, not the copy.** `.github/workflows/skills.yml` opens a weekly update PR
with marola-devkit's `skills-vendor`; `skills-vendor update <name>` does it by hand.

## Commits and pull requests (hard rule)

- **Commit message:** a subject, a body paragraph saying what and why, and `Tested:` and `Cost:`
  trailers in the final block. `Tested:` names the commands run and their result; `Cost:` names
  what the change costs to run or `none`. Agent commits also carry `Co-Authored-By` and the session
  link the harness adds; those lines stay in the trailer block, never in code, comments, PR titles
  or docs.
- **PR workflow:** `just pr` pushes the branch and writes the PR body from the commits
  (`just uprd` refreshes it; `.github/workflows/pr-body.yml` does the same for a PR opened from the
  UI). A hand-written body is kept when its first `<!-- uprd -->` line is deleted. The body opens
  with a *Before* and an *After* paragraph.
- **A PR links the issue or plan it answers**, and a number it adds points at the command that
  reproduces it. A merged PR is finished: follow-up work restarts from `main`.
- **The branch is named after the work**, `claude/<issue-number>-<short-kebab-slug>` for an agent's
  branch. A head branch matching `^claude/project-thread-` (a session name that says nothing about
  the change) fails the `branch` job of `.github/workflows/pr-body.yml`, the rule marola-devkit
  applies to every marola repo; push the same commits to a named branch and open the PR from it.
- **Never** rewrite history on someone else's branch, skip or quarantine a test to get green, or
  push an empty commit to kick CI.
- **Releases** go through `just release X.Y.Z` and the `release` skill; a release needs a change
  that a citing paper would notice, and the concept DOI stays `10.5281/zenodo.23221351`.

## Code style

- Shell: `set -euo pipefail`, shellcheck clean, a `--dry-run` or `--self-test` where the script
  changes state. Everything real lives in `scripts/`; the `justfile` only dispatches.
- C++: the rules of `docs/AGENTS_KOKKOS_202609.md` §1.1 and §1.2 (RAII, explicit memory spaces,
  no allocation in kernels, `float` as WW3's `REAL`, `-Wall -Wextra -Wpedantic`, sanitizers in
  Debug, one `Kokkos::initialize` per process). A kernel never prints or writes a file.
- Fortran added here is a shim or a teaching example: `.nml` inputs, compiles with gfortran with
  directives ignored (CI checks), no edit to WW3 physics.
- Comments say why, name a trap, or point to the issue or plan; they never restate the code or
  narrate a fix's history. A docstring longer than its function is a defect. The same goes for prose
  in docs.
- Reproduce a bug with a failing test before fixing it; a parity bug gets a fixture point.

## When something here turns out to be wrong

Update this file and `CONTRIBUTING.md` in the same change, and the skill or doc that restates the
rule. `CLAUDE.md` imports this file for Claude Code and holds only what is specific to that tool;
other agents read this file as plain text, so a rule that matters stays stated here even when its
detail lives in a skill.
