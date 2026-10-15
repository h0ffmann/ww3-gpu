# MEMORY_DUMP.md

A cheat sheet of the memory commands, for a person at the terminal. It is not an instruction file:
agents do not load it, and [`AGENTS.md`](AGENTS.md) does not link it. Where it and
[`MEMORY.md`](MEMORY.md) disagree, `MEMORY.md` is right.

## In a session (ask in plain words)

The ai-memory plugin's skills trigger on intent, so no command name is needed.

| Ask | What runs |
|---|---|
| "what did we do on X?", "have we discussed Y?" | wiki search (`memory_query`) |
| "what's been going on lately?" | recent pages (`memory_recent`) |
| "memory status", "brief me" | `memory_status`, `memory_briefing` |
| "remember: ..." | a durable page; "tag it publish" to export it |
| "wrap up", "hand off to the next session" | `memory_handoff_begin` |
| "any pending handoff?" | answered from the SessionStart block |
| "lint the memory", "consolidate the memory" | `memory_lint`, `memory_consolidate` |
| "check my inbox", "tell the other project ..." | cross-project messaging |

⚠ `MEMORY.md` names `/ai-memory:status` as a slash command; no command file for it was found in
the plugin cache (14 October 2026). Asking in words works either way.

## The `ai-memory` CLI (v3.0.0)

```bash
ai-memory status                          # counts, paths, version
ai-memory doctor                          # is capture working for this project?
ai-memory search "<terms>"                # full-text search of the wiki
ai-memory read-page "<query>"             # or --path <exact/path.md>
ai-memory write-page --tag publish ...    # write a page; `publish` marks it for export
ai-memory delete-page ...
ai-memory handoffs                        # list open handoffs, cancel stale ones
ai-memory message ...                     # send, list, pop, cancel cross-project messages
ai-memory lint                            # stale pages and duplicates
ai-memory backfill                        # one-time import of earlier sessions
ai-memory backup                          # snapshot wiki/, db/ and config.toml
ai-memory checkpoints                     # wiki git checkpoints; restore-page recovers one page
ai-memory list-projects                   # this repository is h0ffmann-ww3-gpu (.ai-memory.toml)
```

(v) The command list was read from `ai-memory --help` on the owner's workstation, 14 October 2026.
`ai-memory <cmd> --help` gives each command's flags.

## Repository recipes (`scripts/memory.py`)

```bash
just memory export   # owner: the pages tagged `publish` -> memory/, index rewritten
just memory import   # any reader: memory/ -> their own store, check runs first
just memory check    # CI: no session page, log, e-mail address or home path; index current
```

## Claude Code's own file memory

Separate from ai-memory: `~/.claude/projects/<project>/memory/`, indexed by its own `MEMORY.md`,
local to the machine. "Remember X" in a session writes there.

What is recalled from either store is a lead, not evidence; a fact that matters goes into one of
the files `MEMORY.md` lists, with its `(v)` or `⚠`.
