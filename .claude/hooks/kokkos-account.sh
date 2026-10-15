#!/usr/bin/env bash
# PreToolUse hook on Write/Edit: new Kokkos code is never written under the owner's personal Max
# account, only under the UFSC team account on Claude, the enterprise one (AGENTS.md, "Which
# account writes Kokkos code", the owner's hard rule of 2026-10-14). When the file is Kokkos code
# (C++ or CMake under kokkos/) and the logged-in account is a personal Max plan, the edit is denied.
# The account is read from ~/.claude.json (oauthAccount.organizationType); this hook never prints
# anything else from that file. An account it cannot read is let through with a reminder: the
# value the UFSC account shows has not been recorded yet.
# TEAM_PROTECTION=off (the "env" of .claude/settings.json, or the shell) turns the guard off, so
# the owner can switch accounts; unset or any other value keeps it on. Off, the rule still holds.
#
#   .claude/hooks/kokkos-account.sh --self-test   # no Claude Code needed
set -euo pipefail

is_kokkos_code() {
  case "$1" in
    */kokkos/*.cpp|*/kokkos/*.hpp|*/kokkos/*.h|*/kokkos/*.cu|*/kokkos/*CMakeLists.txt|*/kokkos/*.cmake|*/kokkos/*CMakePresets.json) return 0 ;;
    kokkos/*.cpp|kokkos/*.hpp|kokkos/*.h|kokkos/*.cu|kokkos/*CMakeLists.txt|kokkos/*.cmake|kokkos/*CMakePresets.json) return 0 ;;
    *) return 1 ;;
  esac
}

account_type() { # prints organizationType, or nothing
  python3 - "${HOME}/.claude.json" <<'PY' 2>/dev/null || true
import json, sys
try:
    print(json.load(open(sys.argv[1])).get("oauthAccount", {}).get("organizationType", ""))
except Exception:
    pass
PY
}

decide() { # <file path> <account type> -> the hook's JSON, or nothing
  local f="$1" t="$2"
  [ "${TEAM_PROTECTION:-on}" = off ] && return 0
  is_kokkos_code "$f" || return 0
  if [ "$t" = claude_max ]; then
    python3 -c '
import json, sys
print(json.dumps({"hookSpecificOutput": {"hookEventName": "PreToolUse", "permissionDecision": "deny",
  "permissionDecisionReason": "Hard rule (AGENTS.md): no new Kokkos code under the owner'"'"'s personal Max account. "
  "This session is on a claude_max account; write " + sys.argv[1] + " from the UFSC team (enterprise) account."}}))' "$f"
  elif [ -z "$t" ]; then
    python3 -c '
import json, sys
print(json.dumps({"hookSpecificOutput": {"hookEventName": "PreToolUse", "additionalContext":
  "Kokkos code (" + sys.argv[1] + "): the account of this session could not be read. AGENTS.md hard rule: "
  "only the UFSC team (enterprise) account writes Kokkos code; confirm it before going on."}}))' "$f"
  fi
}

if [ "${1:-}" = --self-test ]; then
  fail=0
  TEAM_PROTECTION=on # the cases below test the guard, whatever the session sets
  for p in kokkos/src/ww_kokkos/snl1_dia.cpp /r/kokkos/CMakeLists.txt kokkos/tests/bench_snl1.cpp kokkos/CMakePresets.json; do
    is_kokkos_code "$p" || { echo "should match: $p"; fail=1; }
  done
  for p in scripts/ww3-gpu.sh README.md kokkos/PORT_STATUS.md kokkos/ports.tsv docs/x.cpp; do
    is_kokkos_code "$p" && { echo "should not match: $p"; fail=1; }
  done
  decide kokkos/src/a.cpp claude_max | grep -q '"deny"' || { echo "Max on Kokkos code: not denied"; fail=1; }
  [ -z "$(decide kokkos/src/a.cpp claude_enterprise)" ] || { echo "enterprise on Kokkos code: not allowed"; fail=1; }
  [ -z "$(decide README.md claude_max)" ] || { echo "Max on a README: not allowed"; fail=1; }
  decide kokkos/src/a.cpp "" | grep -q additionalContext || { echo "unknown account: no reminder"; fail=1; }
  [ -z "$(TEAM_PROTECTION=off decide kokkos/src/a.cpp claude_max)" ] || { echo "TEAM_PROTECTION=off: still denied"; fail=1; }
  TEAM_PROTECTION=on decide kokkos/src/a.cpp claude_max | grep -q '"deny"' || { echo "TEAM_PROTECTION=on: not denied"; fail=1; }
  [ $fail -eq 0 ] && echo "kokkos-account self-test ok"
  exit $fail
fi

f="$(python3 -c 'import json,sys; print(json.load(sys.stdin).get("tool_input", {}).get("file_path", ""))')"
decide "$f" "$(account_type)"
