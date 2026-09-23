#!/usr/bin/env bash
# pr — from a finished commit to a pushed PR with a filled description, in one command:
#   1. refuse on main or with a dirty working tree (tracked changes; untracked files are ignored)
#   2. push the branch
#   3. scripts/uprd.sh — create the PR (or rewrite its body) from the commits
#   just pr --dry-run   prints what would happen.
set -euo pipefail
dry_run=0
for a in "$@"; do [ "$a" = "--dry-run" ] && dry_run=1; done
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

branch="$(git branch --show-current)"
[ -n "$branch" ] || { echo "pr: detached HEAD — check out a branch first" >&2; exit 1; }
[ "$branch" != main ] || { echo "pr: refusing to open a PR from main" >&2; exit 1; }
if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
  echo "pr: working tree is dirty — commit or stash first" >&2
  exit 1
fi
git fetch -q origin main 2>/dev/null || true
if [ -z "$(git log --format=%H origin/main..HEAD)" ]; then
  echo "pr: no commits ahead of origin/main on $branch — nothing to open a PR for" >&2
  exit 1
fi

if [ "$dry_run" -eq 1 ]; then
  echo "pr --dry-run on $branch:"
  echo "+ git push -u origin $branch"
  echo "+ just uprd"
  "$script_dir/uprd.sh" --dry-run
  exit 0
fi

git push -u origin "$branch"
"$script_dir/uprd.sh"
