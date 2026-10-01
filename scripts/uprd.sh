#!/usr/bin/env bash
# uprd — update the current branch's pull-request description on GitHub from its commits.
#
#   just uprd                 # rewrite the PR body and print its URL — creates the PR if none exists
#   just uprd --dry-run       # print the body that would be written, change nothing
#   just uprd 84              # that PR by number (also `#84`): head and base come from GitHub,
#                             # so it works from any checkout — needs gh
#   just uprd path/to/body.md # use that file as the body instead of generating one
#   BASE=main just uprd       # base branch (default: the PR's base, else main)
#   BRANCH=x just uprd        # another branch than the checked-out one
#
# The generated body follows .github/PULL_REQUEST_TEMPLATE.md: a Summary (the first commit's
# body paragraph, ~2 sentences), Tested and Cost rows (the commits' `Tested:`/`Cost:` trailers), and one
# "What changed" bullet per commit. Its first line is a marker that lets pr-body.yml regenerate
# the body on every push; delete that line to hand-edit the description and keep it.
set -euo pipefail
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/lib/uprd_title.sh
source "$script_dir/lib/uprd_title.sh"

dry_run=0; body_file=""; pr_arg=""
for arg in "$@"; do
  case "$arg" in
    --dry-run) dry_run=1 ;;
    -h|--help) sed -n '2,15p' "$0"; exit 0 ;;
    [0-9]*|\#[0-9]*) pr_arg="${arg#\#}" ;;
    *) body_file="$arg" ;;
  esac
done

# A PR number resolves the branch and base from GitHub — the checkout can be on anything.
pr_number=""; pr_url=""; base="${BASE:-}"
if [ -n "$pr_arg" ]; then
  if ! pr_tsv="$(gh pr view "$pr_arg" --json number,url,baseRefName,headRefName,state \
        --jq '[.number, .url, .baseRefName, .headRefName, .state] | @tsv' 2>/dev/null)"; then
    echo "uprd: cannot resolve PR #$pr_arg — gh not logged in (gh auth status), or no such PR" >&2
    exit 1
  fi
  IFS=$'\t' read -r pr_number pr_url pr_base pr_head state <<<"$pr_tsv"
  [ "$state" = "OPEN" ] || { echo "uprd: PR #$pr_arg is ${state:-unknown} — only open PRs are rewritten" >&2; exit 1; }
  [ -n "$pr_head" ] || { echo "uprd: PR #$pr_arg has no head branch in gh's answer: $pr_tsv" >&2; exit 1; }
  [ -n "$base" ] || base="$pr_base"
  BRANCH="$pr_head"
  git fetch -q origin "$BRANCH" 2>/dev/null || true
fi

branch="${BRANCH:-$(git branch --show-current)}"
[ -n "$branch" ] || { echo "uprd: detached HEAD — check out the PR branch first" >&2; exit 1; }
guard_base="${BASE:-main}"
[ "$branch" != "$guard_base" ] || { echo "uprd: you are on '$branch' — check out the PR branch first, or BRANCH=<branch> just uprd" >&2; exit 1; }
if git rev-parse --verify -q "$branch" >/dev/null; then head_ref="$branch"; else head_ref="origin/$branch"; fi

if [ -z "$pr_number" ] && pr_tsv="$(gh pr view "$branch" --json number,url,baseRefName \
      --jq '[.number, .url, .baseRefName] | @tsv' 2>/dev/null)"; then
  IFS=$'\t' read -r pr_number pr_url pr_base <<<"$pr_tsv"
  [ -n "$base" ] || base="$pr_base"
fi
[ -n "$base" ] || base=main
create_pr=0
if [ -z "$pr_number" ] && [ "$dry_run" -eq 0 ]; then
  gh auth status >/dev/null 2>&1 || { echo "uprd: gh is not logged in — run: gh auth login" >&2; exit 1; }
  create_pr=1
fi

git fetch -q origin "$base" 2>/dev/null || true
range="origin/$base..$head_ref"

# Title: the first (oldest) commit's subject on the branch, capped at 70 chars.
# `|| true`: under pipefail a long `git log` closed early by `head` dies with SIGPIPE.
first_subject="$(git log --reverse --format=%s "$range" 2>/dev/null | head -1 || true)"
title=""
[ -n "$first_subject" ] && title="$(cap_title "$first_subject")"

generate_summary() {
  local first_sha body
  first_sha="$(git log --reverse --format=%H "$range" | head -1 || true)"
  [ -n "$first_sha" ] || { echo "<!-- fill: one or two sentences — what changed and why -->"; return; }
  body="$(git log -1 --format=%b "$first_sha")"
  python3 - "$body" <<'PY'
import re
import sys

body = sys.argv[1]
lines = []
for line in body.splitlines():
    if line.strip() == "":
        break
    if re.match(r"^(Co-Authored-By|Claude-Session|Tested|Cost|Signed-off-by):", line):
        continue
    lines.append(line)
para = " ".join(" ".join(lines).split())
if not para:
    print("<!-- fill: one or two sentences — what changed and why -->")
    sys.exit(0)
sentences = re.split(r"(?<=[.!?]) +", para)
summary = " ".join(sentences[:2]).strip()
if not summary.endswith((".", "!", "?")):
    summary += "."
print(summary)
PY
}

generate_tested() {
  # The newest commit's `Tested:` trailer, plus a count of earlier ones (they are one click away).
  local trailers n first
  trailers="$(git log --format='%(trailers:key=Tested,valueonly,unfold)' "$range" 2>/dev/null | sed '/^[[:space:]]*$/d')"
  if [ -z "$trailers" ]; then
    echo "not recorded — add a \`Tested:\` trailer to the commit"
    return
  fi
  n="$(wc -l <<<"$trailers")"
  first="$(head -1 <<<"$trailers")"
  if [ "$n" -gt 1 ]; then echo "$first (+$((n - 1)) earlier notes in the commits)"; else echo "$first"; fi
}

generate_cost() {
  # One entry per commit, oldest first, joined with `<br>` (a table cell can't hold a newline):
  # the commit's own `Cost:` trailer, else "not recorded".
  local sha short line text out="" sep=""
  while IFS= read -r sha; do
    [ -n "$sha" ] || continue
    short="$(git rev-parse --short "$sha")"
    line="$(git log -1 --format='%(trailers:key=Cost,valueonly,unfold)' "$sha" | sed '/^[[:space:]]*$/d' | head -1 || true)"
    text="${line:-not recorded — add a \`Cost:\` trailer}"
    out="${out}${sep}${text} (\`${short}\`)"
    sep="<br>"
  done < <(git log --reverse --format=%H "$range" 2>/dev/null)
  printf '%s\n' "$out"
}

generate_body() {
  echo "<!-- uprd: generated from the branch's commits — delete this line to stop pr-body.yml from regenerating it -->"
  echo "**Summary** — $(generate_summary)"
  echo
  echo "| | |"
  echo "|---|---|"
  echo "| **Tested** | $(generate_tested) |"
  echo "| **Cost** | $(generate_cost) |"
  echo
  echo "**What changed**"
  # shellcheck disable=SC2016 # literal markdown backticks
  git log --reverse --format='- %s (`%h`)' "$range" 2>/dev/null
  if [ -n "${EXTRA_FILE:-}" ] && [ -s "$EXTRA_FILE" ]; then echo; cat "$EXTRA_FILE"; fi
}

tmp="$(mktemp)"; trap 'rm -f "$tmp"' EXIT
if [ -n "$body_file" ]; then cp "$body_file" "$tmp"; else generate_body | cat -s > "$tmp"; fi

if [ "$dry_run" -eq 1 ]; then
  echo "----- uprd dry run: branch=$branch base=$base pr=${pr_number:-none} title=\"${title:-<none>}\" -----"
  cat "$tmp"
  exit 0
fi

if [ "$create_pr" -eq 1 ]; then
  git rev-parse --verify -q "origin/$branch" >/dev/null || git push -q -u origin "$branch"
  pr_url="$(gh pr create --base "$base" --head "$branch" --title "$title" --body-file "$tmp")"
  echo "created PR: $pr_url"
else
  gh pr edit "$pr_number" --body-file "$tmp" >/dev/null
  echo "updated PR #$pr_number description: $pr_url"
fi
