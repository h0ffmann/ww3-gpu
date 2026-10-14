# ai-memory 3.0.0 installed on the owner's workstation

2026-10-14, the owner with an agent. ADR-0009, `MEMORY.md`.

## Question

ADR-0009 chose Fabio Akita's ai-memory as the long-term memory, and `MEMORY.md` listed the install
as not done. Does it install and capture this repository on the owner's workstation, with nothing
leaving the machine?

## What was done

On the owner's workstation (Linux, systemd user services), branch `claude/ai-memory-install`
from `main`:

```bash
# the release's install.sh, compared with the tagged source (v3.0.0 = dbe928e, 2026-10-13): identical
curl -fsSL https://github.com/akitaonrails/ai-memory/releases/download/v3.0.0/install.sh -o install.sh
AI_MEMORY_VERSION=3.0.0 sh install.sh          # "checksum verified", ai-memory 3.0.0
claude plugin marketplace add akitaonrails/ai-memory --scope project
claude plugin install ai-memory@ai-memory --scope project
ai-memory setup --yes --agent claude-code
ai-memory doctor
ai-memory backfill --dry-run && ai-memory backfill
ai-memory status
```

`.claude/settings.json` declares the marketplace at `ref: v3.0.0`; the `claude plugin` command
writes it without the `ref`, which was added back by hand. `.ai-memory.toml` names the project.

## Result

- (v) `ai-memory --version` prints `ai-memory 3.0.0`; `install.sh` reported "checksum verified".
- (v) `claude plugin list`: `ai-memory@ai-memory`, version 3.0.0, scope project, enabled.
- (v) `setup` self-check: `healthz: ok` on `127.0.0.1:49374`, `memory_status: ok`, bearer token
  in an owner-only file. It staged 18 hook scripts, wrote its hooks into the user's
  `~/.claude/settings.json` and six Agent Skills under `~/.claude/skills/ai-memory-*`.
- (v) `doctor` reads the scope from `.ai-memory.toml` (`default/h0ffmann-ww3-gpu`, "marker valid").
- (v) `backfill` imported one local session (about 74 events). The repository's earlier sessions ran
  in cloud sessions or other paths, so there was only one to import.
- (v) Nothing leaves the machine: `status` shows `llm: disabled` and `embedding: disabled`;
  `capture_assistant` and `reranker` are commented out in `config.toml`, and
  `AI_MEMORY_RERANKER` is unset.
- ⚠ `doctor` still reports the plugin as disabled and "0 captured" for Claude Code: it reads the
  user scope, and the session that ran the install had not been restarted. To be checked after a
  restart with `/ai-memory:status`.

- (v) After the security review, `.ai-memory.toml` gained a `[capture] ignore_paths` list (the
  account file `~/.claude.json`, credentials, ai-memory's own config and data, `.env` files, and
  `docs/ADRs/`, `docs/WFIPs/`). `doctor` still reads "marker valid". A first try with
  `~/.claude/**` made `doctor` report Claude Code capture as "excluded", so the glob was narrowed
  to the credentials file.

## What it changes

`MEMORY.md` says ai-memory is in use on this workstation. Next: restart Claude Code, run
`/ai-memory:status`, and confirm a session is captured.
