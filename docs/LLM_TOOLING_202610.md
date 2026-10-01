# LLM research tooling (2026-10)

Three tools around the writing side of the repo (the proposal, the course): Consensus for finding
papers, Google Antigravity as a second agent IDE next to Claude Code, and NotebookLM for reading
the whole repo as one notebook. Facts below were checked on 2026-10-01; where an official page was
read only through a search excerpt (the session's network blocks consensus.app, antigravity.google
and support.google.com) the claim is marked ⚠.

## Consensus (MCP)

Consensus searches peer-reviewed literature. Its official MCP server is remote, at
`https://mcp.consensus.app/mcp`, and exposes one tool, `search` (`query`, plus optional `year_min`,
`year_max`, `study_types`, `sample_size_min`, `human`, `duration_max`) ⚠.

- **Claude Code:** `.mcp.json` registers it at project scope (`"type": "http"` is required for a
  URL server, `(v)` code.claude.com/docs/en/mcp). Claude Code asks once to approve project
  servers; then run `/mcp` and sign in (OAuth in the browser; `claude mcp login consensus
  --no-browser` on a headless machine).
- **Antigravity:** `.agents/mcp_config.json`, same server under `serverUrl` (Antigravity's key, not
  `url`) ⚠. If your Antigravity build does not read the project file, paste the entry into the
  global `~/.gemini/config/mcp_config.json` (Agent panel → … → MCP Servers → View raw config).
- **Plan limits:** free 30 searches a month, Pro 500, Deep 2,000 ⚠. MCP and API share the pool.
- **Cloud sessions:** `mcp.consensus.app` is not in this cloud environment's allowed domains, and a
  `claude -p` or cloud session cannot complete the OAuth sign-in anyway. Use it locally, or add the
  host under the environment's Network access settings.
- **How to use it here:** the `find-sources` skill (`.claude/skills/find-sources/`): Consensus finds
  candidates, the DOI or publisher page confirms them, and only then does an entry reach
  `pubs/proposal/refs.bib`. A search result is never cited directly.

## Google Antigravity

Antigravity reads its own layout under `.agents/` ⚠:

| Path | What it is |
|---|---|
| `.agents/rules/proposta.md` | Workspace rule, `trigger: glob` on `pubs/proposal/**`. It replaces the Claude Code reviewer reminder and Stop hook, which Antigravity has no equivalent for. |
| `.agents/skills` → `.claude/skills` | Symlink, so both IDEs load the same skills (`eli5`, `humanizer`, `humanizar`, `find-sources`, `ponytail*`). Antigravity reading skills through a symlink is untested ⚠; if it does not, copy the folders. |
| `.agents/mcp_config.json` | Consensus, as above. |

Workflows (`.agents/workflows/`) are not used: Antigravity deprecates them on 2026-11-01 in
favour of skills ⚠. Antigravity also reads `AGENTS.md`/`GEMINI.md`; the repo has neither, because
CONTRIBUTING.md already holds the conventions and the rule points there.

The CI gate (`.github/workflows/proposal-review.yml`) holds for both IDEs: a proposal change
without a recorded review fails the pull request whichever agent wrote it.

## NotebookLM

```bash
just notebooklm     # -> build/notebooklm/*.md + sources.txt
```

`scripts/notebooklm_bundle.py` joins the repo's prose into five Markdown sources (proposta pt,
proposal en, course, glossary, plans and notes), appends the proposal's bibliography to the two
proposal files, and lists the cited papers' DOI and URL links in `sources.txt`. That is about 15
sources, well under the per-notebook cap (50 on the free plan) and the 500,000-word per-source cap
⚠. Upload the five files and paste the links as web sources in notebooklm.google.com; add
`build/proposal_pt.pdf` too if you want the rendered ABNT reference list.

Uploading is manual on purpose:

- The official API exists only for NotebookLM Enterprise on Google Cloud
  (`discoveryengine.googleapis.com/v1alpha/.../notebooks`, `sources:batchCreate`), which needs a
  Cloud project with Enterprise licences ⚠. Personal and UFRJ accounts cannot use it.
- The community MCP servers work by driving a logged-in browser or by reusing session cookies
  against undocumented endpoints. One of them states that it "may not be aligned with Google's
  Terms of Service" ⚠, and the best known one is archived. Neither is configured here: the account
  at risk would be yours.
