# Handoff cards

`context/handoff/` holds one Markdown card per goal. A card is written when an agent compacts a session or hands off work between Claude Code and Kimi Code. It is a lead, not evidence: every fact it mentions must still be checked against the code and the tests.

A card is produced by:

```bash
just context-pack <goal> --files <modified-files> [--gate-results <file>]
```

and validated by:

```bash
just context-check memory/handoff/goal-<safe-goal>.md
```

## What a card contains

- **Goal id**: the branch or session identifier.
- **Modified files**: `git status --short` at the time of writing.
- **Gate results**: commands run and their result (`pass`/`fail`).
- **Evidence claims**: every `(v)` and `⚠` string found in the listed files.
- **Related context**: bounded pointers to ADRs, WFIPs, log entries and port reports.
- **Summary**: free-text handoff written by the agent.

## What a card must never contain

- E-mail addresses or absolute home paths.
- Raw session transcripts.
- Contents of `WW3/`, `WW3-noaa/`, `WW4/` or `picasso/`.

## Relationship to other memory

- `AGENTS.md`, `docs/ADRs/`, `docs/WFIPs/` and `docs/log/` keep the permanent record.
- `MEMORY.md` and `ai-memory` keep the long-term session memory.
- `context/handoff/` is the per-session, reviewable summary an agent writes when it ends.

Do not import handoff cards into `ai-memory` with `publish` tags: they are working notes, not published pages.
