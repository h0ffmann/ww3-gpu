---
name: compact
description: "Compact the current session's context before it ends, preserving the invariants this repository requires: modified files, gate results, and the (v)/⚠ status of every number. Writes a handoff card that the next agent can read."
---

# compact: preserve what matters across session compaction

Use when a conversation is getting long, when you are about to end a session that changed files, or when switching between Claude Code and Kimi Code. The rules this skill serves are `AGENTS.md`, `CLAUDE.md` and `.kimi-code/AGENTS.md`; they win where this file disagrees.

Readers are scientists: every claim you carry into the next session still cites `file:line` and every number still carries `(v)` or `⚠`. The handoff card is a lead, not evidence.

## What to preserve on compaction

`CLAUDE.md` says: "On compaction, preserve: which files were modified, which gates were run and their result, and the `(v)`/`⚠` status of any number you wrote. Everything else can go."

## Steps

1. **Collect modified files** from `git status --short`.
2. **Collect gate results** from any `Tested:` trailers or from `memory/gates/<goal>.toml` if it exists.
3. **Run the packer**:
   ```bash
   just context-pack <goal> --files <comma-separated-modified-files> [--gate-results <file>]
   ```
   This writes `context/handoff/goal-<safe-goal>.md`.
4. **Fill the Summary section** of the handoff card in your own words: what was done, what is open, what the next agent should check first.
5. **Validate the card**:
   ```bash
   just context-check context/handoff/goal-<safe-goal>.md
   ```
6. **Resume work** from the card by reading it at the start of the next session, then verifying every `(v)` claim and gate result against the code and the tests.

## What not to put in the card

- Raw session transcripts.
- E-mail addresses or absolute home paths (`context check` refuses these).
- Contents of `WW3/`, `WW3-noaa/`, `WW4/` or `picasso/`; cite `model/src/<file>.F90:<line>` at the pin instead.
- Evidence that belongs in `AGENTS.md`, an ADR, a WFIP or `docs/log/`.

## Finding related context

If you need to refresh your memory before compacting, ask the routing skill instead of grepping:

```bash
just route why "tolerance widening rule"
just context-query "what was tried for W3SDS4"
```

Both return bounded results from `docs/`, `notes/`, `kokkos/port_report/` and the existing ADRs/WFIPs/log.

## Cross-agent handoff

A card written by Claude Code in `context/handoff/` is readable by Kimi Code and vice versa, because both tools load skills from `.agents/skills/` (a symlink to `.claude/skills/`). Do not rely on the card alone: re-read the files it names and re-run the gates it claims passed before continuing the work.
