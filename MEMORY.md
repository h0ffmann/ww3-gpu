# MEMORY.md

Where the memory of this repository is. Read it after [`AGENTS.md`](AGENTS.md). The shape follows
h0ffmann/closed-source's `MEMORY.md`
([at `5cf9a6a`](https://github.com/h0ffmann/closed-source/blob/5cf9a6a/MEMORY.md)).

## The record, in files

What a reader or the next agent must know is in a file, in the same pull request as the change.
Never only in a chat.

| What | File |
|---|---|
| The rules, the hard rules and the gates | [`AGENTS.md`](AGENTS.md), [`CONTRIBUTING.md`](CONTRIBUTING.md) |
| Decisions | [`docs/ADRs/`](docs/ADRs/README.md) |
| Designs, each with its definition of done | [`docs/WFIPs/`](docs/WFIPs/README.md) |
| What was tried, including what failed (the journal) | [`docs/log/`](docs/log/README.md) |
| Notes, one idea each | [`notes/`](notes/index.md), under [`NOTES.md`](NOTES.md) |
| How each reference of the proposal was checked | [`pubs/proposal/REFERENCES.md`](pubs/proposal/REFERENCES.md) |
| Each release and its permanent DOI | [`RELEASES.md`](RELEASES.md) |
| The ties of the Zenodo record, with a date of record | [`RELATED_WORK.md`](RELATED_WORK.md) |
| The measured numbers | [`bench/results/`](bench/README.md), [`kokkos/PORT_STATUS.md`](kokkos/PORT_STATUS.md) |
| The ai-memory pages the owner publishes | [`memory/`](memory/README.md), written by `just memory export` |
| The Claude project's settings | [`.claude/project/`](.claude/project/) ([ADR-0005](docs/ADRs/ADR-0005-claude-project-settings-in-repo.md)) |

## The long-term memory: ai-memory

**Chosen on 2026-10-14; in use on the owner's workstation since 2026-10-14, version 3.0.0**
([ADR-0009](docs/ADRs/ADR-0009-ww3-gpu-adopts-ai-memory-from-fabio-akita.md);
[the install](docs/log/2026-10-14-ai-memory-install.md)). Not in cloud sessions: they have no
binary, so the plugin captures nothing there and says so once.
[ai-memory](https://github.com/akitaonrails/ai-memory) is Fabio Akita's long-term memory for coding
agents (MIT). It holds what each session did, what failed, what is open, and a handoff to the next
session or the next agent. It does not hold the record: the files above do, because its wiki lives
on the owner's machine and no reader of this repository sees it.

What the owner does, once for each machine:

1. Enable the plugin in `.claude/settings.json`: the marketplace `akitaonrails/ai-memory` at
   `ref: v3.0.0`, and `enabledPlugins: {"ai-memory@ai-memory": true}`. Do this first, so `setup`
   does not install the hooks a second time.
2. Install the binary, pinned to the plugin's version (without the variable the script installs the
   latest release):

   ```bash
   curl -fsSL https://github.com/akitaonrails/ai-memory/releases/download/v3.0.0/install.sh | AI_MEMORY_VERSION=3.0.0 sh
   ai-memory setup --agent claude-code
   ```

3. Keep everything on the machine: `embedding_provider` stays `local`, assistant capture stays off,
   and `AI_MEMORY_RERANKER` is not `llm` (ai-memory's `DATA_HANDLING.md`).
4. Restart Claude Code, run `/ai-memory:status`, then `ai-memory backfill` to bring in the earlier
   sessions of this repository.

[`.ai-memory.toml`](.ai-memory.toml) names the project `h0ffmann-ww3-gpu`, so every checkout,
worktree and branch lands in the same one. `ai-memory setup` also writes its hooks into the user's
`~/.claude/settings.json`; while those exist the plugin skips every event they deliver, so nothing
is captured twice (ai-memory's `plugins/claude-code/README.md` at `v3.0.0`).

### Published pages

Most of the wiki stays on the owner's machine. A page the owner wants every reader to have is
tagged `publish` (ask the agent, or `ai-memory write-page --tag publish`), and then:

```bash
just memory export   # owner: the wiki pages tagged `publish` -> memory/, index rewritten
just memory import   # any reader with ai-memory: memory/ -> their own store, unchanged pages skipped
just memory check    # CI: no session page, log, e-mail address or home path; the index is current
```

Session pages (`sessions/`) and the monthly logs (`log-YYYY-MM.md`) are never exported, whatever
their tags: they hold raw prompts and local paths. `import` writes into the project named by
[`.ai-memory.toml`](.ai-memory.toml), so every reader's copy lands in `h0ffmann-ww3-gpu`. A page
in `memory/` can arrive through any pull request, so `import` runs `check` first and refuses on a
fault, writes each page under `published/`, tags it `imported`, never pins it, and turns any kind
other than `fact`, `gotcha` or `decision` into `fact`: a page from git can inform a reader's agent
but never pose as one of its rules. `check` also refuses the `_*` and `profile/` paths ai-memory
reserves. The owner does not import (the pages are already in their wiki). A published page is
public: the security reviewer reads `memory/` like any other diff.

What is in ai-memory is a lead, not evidence: its own routing puts `AGENTS.md` above it and says to
check a recalled fact against the code and the tests. A fact that matters is copied into one of
the files above, with its `(v)` or `⚠`.

h0ffmann/closed-source has the full readiness list for the same adoption
([its PR #55](https://github.com/h0ffmann/closed-source/pull/55)).
