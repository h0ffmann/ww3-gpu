# Kimi Code in ww3-gpu

Kimi Code loads this file and the root `AGENTS.md` together. `AGENTS.md` is the rulebook and wins
on any disagreement; this file only says what Kimi Code does not get from the Claude Code setup,
and where to look first. `CONTRIBUTING.md`, "Working with Kimi Code", has the evidence.

## Read the section for the task, not the whole book

| Task | Read first |
|---|---|
| Where a change goes, and which gate it needs | `AGENTS.md`, "Where a change belongs" and "Setup and commands" |
| Anything that writes a number | `AGENTS.md`, "Audience and evidence": every number carries `(v)` or `⚠` |
| A WW3 fact | `AGENTS.md`, "WW3 source is read-only": cite `model/src/<file>.F90:<line>` at the pin |
| Porting or reviewing a routine | `AGENTS.md`, "Porting" and "Bit for bit with the Fortran"; the `ww3-port` skill |
| The proposal (`pubs/proposal/`) | `AGENTS.md`, "The proposal"; then the review below |
| A diagram | the `figure` skill |
| A commit or a PR | `AGENTS.md`, "Commits and pull requests" |

## Kokkos code is not yours to write (hard rule)

Kokkos code (C++, CUDA and CMake under `kokkos/`) is written only in a Claude Code session of the
UFSC team account (`AGENTS.md`, "Which account writes Kokkos code"). A Kimi Code session is never
that account. Read, review and explain Kokkos code; do not create or edit it. When a task needs
it, stop and say so. Scripts, docs, CI and the ledgers (`kokkos/ports.tsv`,
`kokkos/PORT_STATUS.md`) are not Kokkos code.

## What Claude Code does here that you do by hand

- **Hooks do not run.** Nothing reminds you to review the proposal or stops you ending without a
  review; CI's `proposal-review.yml` still refuses the PR. In a fresh container, run
  `scripts/agent_env.sh` yourself.
- **Subagents.** `revisor-proposta` and `security-reviewer` are Claude Code definitions. Read
  `.claude/agents/<name>.md` and apply it as a read-only checklist to the changed files. For the
  proposal, write the parecer and record it with `just proposal-review-record <parecer>`.
- **Slash commands.** `/port-next`, `/adrs`, `/graph-check`, `/graph-view` and
  `/update-sub-modules` are procedures in `.claude/commands/<name>.md`. When asked for one, read
  that file and follow it, with the request's argument as `$ARGUMENTS`.
- **Skills load** from `.agents/skills`, a link to `.claude/skills`. Edit a vendored skill through
  its `.patch`, never the copy. Fields such as `allowed-tools` are Claude Code's and may be ignored.
- **The ponytail plugin is not loaded.** The `ponytail*` skills are; even so, "the smallest
  change" never licenses a refactor or a "simplification" of a ported routine.

## Working habits that save turns

- Start in the repository root. `WW3/`, `WW3-noaa/`, `WW4/` and `picasso/` are submodules you
  read, never edit.
- Enter the toolchain before building (`just dev` or `just ww3`); a missing tool means enter the
  shell, not skip the gate.
- Run builds and regtests through their `just` recipes and read the verdict, not the compiler
  output. Send small lookups to the local model with `just ask "…"`.
- Never `--gpu` without the owner.
- Commits: a subject, a body saying what and why, and a `Tested:` trailer naming the commands run
  and their result. No `Cost:` trailer, and no model name in code, docs or PR titles.
