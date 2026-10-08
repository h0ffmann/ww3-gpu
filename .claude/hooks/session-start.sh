#!/usr/bin/env bash
# SessionStart: in a Claude Code cloud session only, install what the AGENTS.md gates need
# (scripts/agent_env.sh). Synchronous on purpose: a gate run before the tools exist would fail.
# A local session is left alone; the developer's machine has the Nix shell (`just ww3`).
set -euo pipefail
[ "${CLAUDE_CODE_REMOTE:-}" = true ] || exit 0
bash "${CLAUDE_PROJECT_DIR:-$(cd "$(dirname "$0")/../.." && pwd)}/scripts/agent_env.sh" >&2
