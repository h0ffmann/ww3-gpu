#!/usr/bin/env bash
# Stop hook: refuse to end the turn while pubs/proposal/ has uncommitted changes that no review
# covers. The PostToolUse reminder can be ignored by a model; this cannot — exit 2 blocks the stop
# and hands the reason back to the agent.
#
#   .claude/hooks/proposal-review-stop.sh --self-test
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.." || exit 0

dirty() { [ -n "$(git status --porcelain -- pubs/proposal 2>/dev/null | grep -v '\.review\.json' || true)" ]; }

if [[ "${1:-}" == "--self-test" ]]; then
  fail=0
  command -v git >/dev/null || { echo "git missing"; exit 1; }
  python3 scripts/proposal_review_gate.py --self-test >/dev/null || { echo "gate self-test failed"; fail=1; }
  dirty >/dev/null 2>&1 || true   # both outcomes are valid here; the call must not error
  [[ $fail -eq 0 ]] && echo "proposal-review-stop self-test ok"
  exit $fail
fi

# Claude Code sets stop_hook_active when it is already re-prompting after a block; do not loop.
if [ "$(python3 -c 'import json,sys; print(json.load(sys.stdin).get("stop_hook_active", False))' 2>/dev/null || echo False)" = "True" ]; then
  exit 0
fi

dirty || exit 0
if ! reason="$(python3 scripts/proposal_review_gate.py --check 2>&1)"; then
  cat >&2 <<EOF
pubs/proposal/ changed and the review does not cover it.

  $reason

Run the revisor-proposta subagent (Task/Agent, subagent_type: "revisor-proposta") over the changed
files, deal with the blockers it reports, then record it:

  python3 scripts/proposal_review_gate.py --record <parecer.md>

That file is what the CI gate (.github/workflows/proposal-review.yml) checks on the pull request.
EOF
  exit 2
fi
exit 0
