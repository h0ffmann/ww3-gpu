# Contributing

## Who this repository is for

The first readers of this repository are scientists: PhD researchers, postdocs and independent
researchers working on ocean wave modelling, numerical methods or HPC. Every document, skill, agent
and piece of metadata here is written for them, unless a section below names a different reader.

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

## Figures

Diagrams are Mermaid fences in the page they illustrate, so GitHub draws them in place. Each one
starts with `%% figure: <id>` and `%% title: <question>` and is followed by a *How to read this
figure* card (takeaway, how to read, what is not shown, evidence) for readers new to the topic.
`just figures` renders every fence with a pinned mermaid-cli and font into `pubs/figures/mermaid/`,
which the PDF and Word builds use, and rewrites the gallery `pubs/figures/README.md`; CI runs
`python3 scripts/figures.py check`. The `figure` skill (`.claude/skills/figure/`) has the rules.
A mistake in a figure is as welcome as one in the text: the gallery links an issue for each.

## Ground rules

- Keep the `⚠` / `(v)` convention. Marking uncertainty honestly is the point.
- Don't vendor WW3 source. Scripts fetch it.
- The third-party skills under `.claude/skills/` (`humanizer`, `humanizar`, `ponytail*`) are pinned
  in `.claude/skills/skills.lock`, and the audience paragraph each one carries is the
  `.claude/skills/<name>.patch` the lock names, re-applied on every update. Edit the patch, not the
  copy. `skills-vendor check` (marola-devkit's `scripts/skills_vendor.py`, run as
  `python3 skills_vendor.py check --lock .claude/skills/skills.lock`) verifies the copies;
  `.github/workflows/skills.yml` opens a weekly update PR, and `skills-vendor update <name>` does
  the same by hand.
- Run `just kokkos-test serial-debug` after touching `kokkos/`, and `bash -n` plus
  shellcheck on any shell script you touch. CI does all of these (the `kokkos` and
  `lint` jobs in `.github/workflows/ci.yml`).
- Prose style: plain, direct, no filler. If a sentence doesn't teach something,
  cut it.
- Length is not a measure of a research doc (the proposal, the course, `docs/`). A Portuguese
  and an English version, or a revision and the text it replaces, may differ in length, and
  that is not a defect: do not pad or trim a text to match another one. What must match across
  languages is content, the same claims, citations and numbers (`just proposal-lint` checks
  this for the proposal). Cut a sentence because it repeats or teaches nothing, never to hit a
  word count.

## Opening a pull request

Write the commit message properly (subject, a body paragraph saying what and why, and
`Tested:` / `Cost:` trailers in the final block of the message), then `just pr`: it pushes the branch and creates the PR with a description
generated from the commits (`just uprd` regenerates it later). A PR opened from the GitHub UI
gets the same treatment from `.github/workflows/pr-body.yml`. Delete the first `<!-- uprd -->`
line of a description to hand-edit it and keep it.
