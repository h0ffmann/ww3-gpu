#!/usr/bin/env bash
# branch_name — refuse a branch name that cannot be traced back to the work that asked for it.
#   bash scripts/branch_name.sh <branch>   exit 0 if allowed, 1 with the reason if not
#   bash scripts/branch_name.sh --self-test
# An agent's branch is goal/<id>-<short-kebab-slug>: <id> is the issue number, or without an
# issue the last 8 characters of the Claude project thread id plus a random 4-digit number
# (AGENTS.md, "Commits and pull requests"). A `claude/` or `claude-project` name is a session
# name, not a goal. Used by `just pr` (scripts/pr.sh) and the `branch` job of pr-body.yml.
set -euo pipefail

usage='push the work to goal/<issue>-<slug>, or goal/<last 8 of the thread id>-<4 random digits>-<slug> without an issue (git push -u origin HEAD:goal/<id>-<slug>), and open the PR from it'

check() {
  local b="$1"
  if grep -Eiq '^claude(/|-project)' <<<"$b"; then
    echo "branch '$b' is a generic session name: $usage" >&2
    return 1
  fi
  if [[ "$b" == goal/* ]] &&
    ! grep -Eq '^goal/([0-9]+|[A-Za-z0-9]{8}-[0-9]{4})-[a-z0-9]+(-[a-z0-9]+)*$' <<<"$b"; then
    echo "branch '$b' starts with goal/ but has no goal id: $usage" >&2
    return 1
  fi
}

if [ "${1:-}" = "--self-test" ]; then
  fail=0
  for b in goal/143-h100-access goal/ke1RYpdP-7316-branch-naming goal/ke1rypdp-7316-branch-naming-kjlqqb \
    kimi/kimi-retrieval dependabot/github_actions/x wfip-0004-matrix main; do
    check "$b" 2>/dev/null || { echo "self-test: '$b' refused, should pass" >&2; fail=1; }
  done
  for b in claude/project-thread-abc claude/143-h100-access claude-project-xyz Claude/x \
    goal/h100-access goal/143 goal/ke1RYpdP-branch goal/143-H100 goal/143-a--b; do
    check "$b" 2>/dev/null && { echo "self-test: '$b' passed, should be refused" >&2; fail=1; }
  done
  [ "$fail" -eq 0 ] && echo "branch_name self-test: ok"
  exit "$fail"
fi

[ $# -eq 1 ] || { echo "usage: $0 <branch> | --self-test" >&2; exit 64; }
check "$1"
