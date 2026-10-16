# WFIP-0004: Context compaction for Claude Code and Kimi Code

| | |
|---|---|
| **Status** | Draft |
| **Author** | Hoffmann (asked in the Kimi Code session, 2026-10-16) |
| **Created** | 2026-10-16 |
| **Deliverable** | D6 |
| **Related** | `AGENTS.md` (compaction instruction), `CLAUDE.md` (Claude-specific compaction), `.kimi-code/AGENTS.md` (Kimi-specific notes), `MEMORY.md` / ADR-0009 (long-term memory), the `routing` skill (`scripts/route.py`), `kokkos/PORT_STATUS.md` |
| **Effort** | M: one Python tool, one skill, tests, just recipes and a handoff convention; no new toolchain |
| **Gain** | `lab/dev-loop` (smaller, relevant context windows; reusable handoffs between Claude and Kimi); `proposal` (D6 asks for tools and recommendations for running the lab: a shared compaction method is one such tool) |
| **Effort vs Gain** | `do next, in slices`: slice 1 is standard-library only and lands immediately; slice 2 adds local embeddings only if slice 1 proves the retrieval bottleneck |
| **Depends on** | the existing `routing` skill (`scripts/route.py`) `(v)` read 2026-10-16; `MEMORY.md` and the `memory/` layout `(v)` read 2026-10-16 |
| **Blocked by** | none |
| **Risk** | a bad summary drops a `(v)` claim, a gate result or a modified file; the `context check` gate and the skill must make the preserved items explicit |

### Readiness

| | |
|---|---|
| **Manually reviewed** | no |
| **Written by** | Hoffmann, with an agent |
| **Tasks** | `WFIP-0004.tasks.md` |
| **Tests** | `tests/test_context.py` |
| **Spec-kit** | none |
| **Issues** | not filed: Draft |

## 1. Summary

Add a free, local, repo-native context-compaction layer for Claude Code and Kimi Code. A single Python tool (`scripts/context.py`) packs the invariants an agent must keep across compaction, retrieves related documents through the existing `routing` skill, and writes a handoff card that the next agent in either tool can read. The first slice uses only the Python standard library; later slices may add local embeddings (Jina, BGE, GTE) or a Java/Lucene backend if the lab needs them.

## 2. Motivation

Both Claude Code and Kimi Code compact long conversations into a summary, and neither summary is guaranteed to keep the facts this repository requires `(v)` `CLAUDE.md`: "On compaction, preserve: which files were modified, which gates were run and their result, and the `(v)`/`⚠` status of any number you wrote. Everything else can go."

Today the agent must remember that instruction and apply it by hand. The repository already avoids broad context loss in other ways:

- `AGENTS.md`, `CLAUDE.md` and `.kimi-code/AGENTS.md` load the invariants at session start `(v)` read 2026-10-16.
- The `routing` skill answers "where is it" in one bounded `scripts/route.py` call instead of grepping the tree or reading whole files `(v)` read 2026-10-16.
- `MEMORY.md` and `ai-memory` keep long-term memory of what sessions tried `(v)` read 2026-10-16.

What is missing is a **deterministic packer** that assembles the right subset of this material before the prompt is sent, and a **shared handoff format** so a session compacted in Claude Code can be resumed in Kimi Code and vice versa.

## 3. What changes for the reader

Before: an agent compacts a conversation by summarising it and hopes the summary keeps the modified files, gate results and `(v)`/`⚠` claims.

After slice 1:

```bash
# Pack the context for the current goal, preserving the required fields
just context-pack goal/bbf33c2c-7391-context-compaction \
  --files scripts/context.py,tests/test_context.py \
  --gate-results memory/gates/latest.toml
# → writes context/handoff/goal-bbf33c2c-7391-context-compaction.md

# Ask a bounded question instead of grepping
just context-query "tolerance widening rule"
# → returns up to 8 paragraphs from docs/ADRs, docs/WFIPs, docs/log, notes/, kokkos/port_report/

# Validate a handoff card before committing it
just context-check context/handoff/goal-bbf33c2c-7391-context-compaction.md
```

The `/compact` skill tells Claude Code and Kimi Code to call the same commands and to write the handoff card before ending a session that changed files.

## 4. Sources and dependencies reviewed

### Local-first requirement

The owner asked for "free solutions preferred". Free here means no paid API call and no mandatory cloud service. The first slice therefore uses Python's standard library only, re-using the BM25-style retrieval already in `scripts/route.py` `(v)` read 2026-10-16. This keeps the tool runnable in a fresh container, a jail and Kimi Code alike.

### Future local-embedding options (slice 2)

If keyword retrieval becomes the bottleneck, the tool can be extended to use one of the following locally runnable embedding models, none of which requires an API key:

- `jina-embeddings-v3` ⚠
- `BAAI/bge-large-en-v1.5` ⚠
- `Alibaba-NLP/gte-large-en-v1.5` ⚠

A local vector store such as FAISS, `usearch`, `sqlite-vss` or Chroma in local mode would index repo-owned Markdown and code snippets ⚠. These are not added in slice 1 because they introduce a dependency and because `scripts/route.py` already covers the common "where is it" case well enough.

### Java/Lucene option

If the lab later requires a JVM-based backend, Apache Lucene or a self-hosted OpenSearch node can replace the keyword retriever ⚠. This is documented as an alternative in §9 and left for a separate WFIP.

### ai-memory relationship

`ai-memory` captures raw sessions on the owner's machine `(v)` ADR-0009. The handoff cards in `context/handoff/` are not a replacement: they are the public, reviewable summary an agent writes when it ends. A fact that matters is still copied into `AGENTS.md`, an ADR, a WFIP or `docs/log/`.

## 5. Design

### 5.1 Files touched

| File | Purpose |
|---|---|
| `scripts/context.py` | Core tool: `pack`, `query`, `check`, `--self-test`. Standard library only. |
| `tests/test_context.py` | Unit tests for the tool. |
| `.claude/skills/compact/SKILL.md` | `/compact` skill for Claude Code and Kimi Code (loaded through `.agents/skills/` symlink). |
| `context/handoff/README.md` | Convention for handoff cards: what they are and are not. |
| `justfile` | `context-pack`, `context-query`, `context-check`, `context-selftest` recipes. |
| `docs/WFIPs/WFIP-0004-context-compaction.md` | This design doc. |
| `docs/WFIPs/README.md` | Regenerated by `just wfip index`. |

### 5.2 `scripts/context.py`

#### `context pack --goal <id> [--files f1,f2] [--gate-results <file>]`

Writes a Markdown handoff card to `context/handoff/goal-<id>.md` with the following sections, in order:

1. **Goal id**: branch or session identifier.
2. **Modified files**: output of `git status --short`, scoped to repo-owned files; submodules shown only as state lines, never their contents.
3. **Gate results**: parsed from `--gate-results` (TOML/JSON) or from a default `memory/gates/<goal>.toml` if it exists. Each row: command, result, date.
4. **Evidence claims**: every `(v)` and `⚠` string found in the files listed in `--files`, with surrounding sentence for context.
5. **Related context**: up to `--budget` paragraphs from `scripts/route.py why "<goal summary>"`, giving the agent pointers to ADRs, WFIPs, log entries and port reports without reading them whole.
6. **Summary**: a free-text slot the writing agent fills; if empty, the card says so.

The packer never includes:
- e-mail addresses,
- absolute home paths,
- raw session transcripts,
- contents of `WW3/`, `WW3-noaa/`, `WW4/` or `picasso/` submodules.

#### `context query "<question>" [--budget N]`

Runs `scripts/route.py why "<question>"` and, if that returns fewer than `budget` results, falls back to a keyword/BM25 scan of repo-owned Markdown files (`docs/`, `notes/`, `kokkos/port_report/`). Output stops at `budget` lines and reports how many were cut.

#### `context check <pack.md>`

Validates that the pack:
- has all required sections,
- contains no e-mail address or absolute home path,
- does not claim a gate result without a command,
- does not list a `(v)` claim without the surrounding sentence that contains it.

Returns exit 0 if clean, 1 with a fault list otherwise.

#### `context --self-test`

Runs deterministic tests in-process: pack schema validation, query on a temporary tree, check refusing leaks.

### 5.3 `/compact` skill

The skill lives at `.claude/skills/compact/SKILL.md` and is reachable from Kimi Code through `.agents/skills/` symlink. Its front matter carries `name` and a quoted `description`. The body restates the audience rule and instructs the agent:

1. Before a session ends, if files were modified, run `just context-pack <goal> --files <modified>`.
2. Fill the **Summary** section of the handoff card.
3. Run `just context-check` on the card.
4. Preserve in the compaction summary: modified files, gate results, `(v)`/`⚠` claims.
5. Treat the handoff card as a lead, not evidence; cite `file:line` for every claim in the next session.

### 5.4 Handoff directory

`context/handoff/` holds one Markdown card per goal. `context/handoff/README.md` states the convention and warns that cards are not a substitute for `AGENTS.md`, ADRs or `docs/log/`.

### 5.5 Determinism

The tool is deterministic: same repository state and same arguments produce the same pack byte for byte, except for timestamps and the free-text summary. Timestamps are optional and disabled by default in CI.

## 6. Parity and physics impact

None. This WFIP touches tooling, skills and documentation only. No Fortran, C++ or Kokkos code changes; no WW3 source is edited; no GPU run is needed.

## 7. Verification plan and definition of done

- [ ] `scripts/context.py` exists and `python3 scripts/context.py --self-test` passes.
- [ ] `python3 -m unittest tests/test_context.py` passes.
- [ ] `python3 .claude/hooks/check_agent_frontmatter.py` passes (skill front matter is parseable).
- [ ] `just wfip check` passes after `just wfip index` regenerates the WFIP index.
- [ ] `bash -n scripts/context.py` passes.
- [ ] `just vale docs/WFIPs/WFIP-0004-context-compaction.md context/handoff/README.md` passes on English prose.
- [ ] `just context-pack goal/bbf33c2c-7391-context-compaction --files scripts/context.py` writes a valid handoff card.
- [ ] `just context-check` on that card returns 0.

## 8. Risks, limitations, and honest caveats

- A handoff card can only be as good as the agent that writes it. If the agent forgets a `(v)` claim, the next agent must still re-check every number.
- Keyword retrieval misses synonyms. Slice 2 may need embeddings if this becomes a real bottleneck.
- The tool does not integrate with `ai-memory` import/export; that is left for a later slice because `ai-memory` is local to the owner's machine.

## 9. Alternatives considered

| Alternative | Why not |
|---|---|
| Do nothing; rely on `AGENTS.md` and `ai-memory` | `AGENTS.md` loads invariants but does not pack the per-session state; `ai-memory` is local and not reviewed in PRs. |
| Add local embeddings in slice 1 | Adds a dependency (transformers, sentence-transformers, FAISS, or Ollama) before the simpler keyword approach is proven insufficient. |
| Build the consolidation in Java/Lucene | Heavier than needed; the repository's tooling is Python-first (`scripts/*.py`). Java is kept as a documented future alternative. |
| Store handoff cards in `docs/log/` | `docs/log/` is append-only journal of what was tried; handoff cards are per-session, often rewritten, and belong under `memory/`. |

## 11. Open questions

- **Embeddings in slice 2?** Default: no. Revisit when an agent spends more than two turns refining a `context query` because keyword retrieval missed the right paragraph. Hoffmann decides.
- **Java/Lucene backend?** Default: no. Revisit if a JVM shop joins the lab or if Lucene's query language becomes necessary. Hoffmann decides.
- **Should handoff cards be exported by `just memory export`?** Default: no; they are not `publish`-tagged ai-memory pages and they may contain raw local paths before `context check` runs. Revisit after three months of use.

## Appendix

### Checked live

- `scripts/route.py` standard-library implementation and `--budget` behaviour `(v)` read 2026-10-16.
- `MEMORY.md` and `ADR-0009` memory layout and `memory/` conventions `(v)` read 2026-10-16.
- `CLAUDE.md` compaction instruction `(v)` read 2026-10-16.

### Not checked

- Embedding model licences and exact model names for slice 2 (Jina, BGE, GTE): listed as future work.
- Java/Lucene packaging details: listed as future work.
