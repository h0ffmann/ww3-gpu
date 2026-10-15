---
description: Check whether the graphifyy CLI (command graphify) and its Claude Code skill are installed here.
---

Report, do not install. Run each check below and answer with one table (Check, Result, Evidence),
each result `(v)` with the command that showed it:

1. `command -v graphify`: on PATH or not, and where.
2. `graphify --version`: the installed version (it prints `graphify X.Y.Z` and exits non-zero; read
   the line, not the exit code).
3. `uv tool list 2>/dev/null | grep -A1 '^graphifyy'` and `pip show graphifyy 2>/dev/null`: which
   installer owns it. The PyPI package is `graphifyy`, the command `graphify`.
4. `test -f ~/.claude/skills/graphify/SKILL.md`: whether the skill is registered for Claude Code.
5. `gh api repos/Graphify-Labs/graphify/releases/latest --jq .tag_name`: the latest release, to say
   whether the installed one is behind. If `gh` fails, say `⚠ latest release not checked`.

If it is missing, say so and name the install the upstream documents
(`uv tool install graphifyy`, then `graphify install`); never run it. graphify is not a Claude Code
plugin (no `.claude-plugin/` in Graphify-Labs/graphify, checked 14 October 2026), so there is
nothing to enable in `.claude/settings.json`.
