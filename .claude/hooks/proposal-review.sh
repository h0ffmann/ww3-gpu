#!/usr/bin/env bash
# PostToolUse hook on Write/Edit: when a file under pubs/proposal/ changes, tell the agent to run
# the revisor-proposta subagent before it finishes. A reminder in additionalContext, not a block:
# a half-written paragraph should not fail a tool call. The binding gates are the Stop hook
# (.claude/hooks/proposal-review-stop.sh) and the CI workflow.
#
#   .claude/hooks/proposal-review.sh --self-test   # no Claude Code needed
set -euo pipefail

REMINDER='Arquivo de pubs/proposal/ alterado. Antes de concluir (e antes de abrir o PR), execute o
subagente revisor-proposta (Task/Agent, subagent_type: "revisor-proposta") sobre os arquivos
alterados e trate os bloqueadores do parecer. Registre o parecer com
`python3 scripts/proposal_review_gate.py --record <parecer.md>`; sem isso o Stop hook e a CI barram.'

matches() { case "$1" in *pubs/proposal/*) return 0 ;; *) return 1 ;; esac; }

emit() {
  python3 -c '
import json, sys
print(json.dumps({"hookSpecificOutput": {"hookEventName": "PostToolUse",
                                         "additionalContext": sys.argv[1]}}))' "$REMINDER"
}

if [[ "${1:-}" == "--self-test" ]]; then
  fail=0
  for p in "pubs/proposal/pt/03-theme.md" "/repo/pubs/proposal/refs.bib" "pubs/proposal/meta.pt.yaml"; do
    matches "$p" || { echo "should match: $p"; fail=1; }
  done
  for p in "README.md" "course/01-waves.md" "scripts/build_pdf.sh" "pubs/book/defaults.yaml"; do
    matches "$p" && { echo "should not match: $p"; fail=1; }
  done
  out="$(emit)"
  python3 -c 'import json,sys; d=json.loads(sys.argv[1]); assert d["hookSpecificOutput"]["hookEventName"]=="PostToolUse"; assert "revisor-proposta" in d["hookSpecificOutput"]["additionalContext"]' "$out" \
    || { echo "emit: malformed JSON payload"; fail=1; }
  # The agent definitions must parse: an unquoted description containing ": " reads as a nested
  # mapping and Claude Code then ignores the agent (2026-09-16).
  python3 "$(dirname "${BASH_SOURCE[0]}")/check_agent_frontmatter.py" || fail=1
  [[ $fail -eq 0 ]] && echo "proposal-review self-test ok"
  exit $fail
fi

FILE=$(python3 -c 'import json,sys; print(json.load(sys.stdin).get("tool_input",{}).get("file_path",""))' 2>/dev/null || true)
matches "$FILE" && emit
exit 0
