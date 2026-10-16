#!/usr/bin/env bash
# worktrees — what each worktree of this clone would lose if its directory went away, and the
# removal of the ones whose branch is already merged (CONTRIBUTING.md, "Worktrees and parallel
# sessions").
#   bash scripts/worktrees.sh [list]           every worktree: branch, unpushed commits, changes
#   bash scripts/worktrees.sh clean            what clean would remove (dry run)
#   bash scripts/worktrees.sh clean --apply    remove it, logging each branch tip for recovery
#   bash scripts/worktrees.sh --self-test
# "Merged" is either the branch tip being an ancestor of origin/main, or a merged pull request
# whose head is exactly the branch tip (a squash merge leaves no ancestor). A worktree is removed
# only when its branch is merged, `git status` is empty (submodules included) and no process has
# its cwd inside it, so a session still sitting in one keeps it. Each deleted branch is written
# to <git common dir>/worktrees-clean.log as `git branch <name> <sha>`, the line that restores it.
set -euo pipefail

main_ref=origin/main

merged() { # merged <branch> <sha>: 0 when the work is on main
  git merge-base --is-ancestor "$2" "$main_ref" 2>/dev/null && return 0
  [ -n "${WORKTREES_NO_GH:-}" ] && return 1
  command -v gh >/dev/null || return 1
  local heads
  heads=$(gh pr list --state merged --head "$1" --json headRefOid --jq '.[].headRefOid' 2>/dev/null) || return 1
  grep -qx "$2" <<<"$heads"
}

in_use() { # in_use <dir>: a process (a shell, an agent) has its cwd inside it
  local p d
  for p in /proc/[0-9]*/cwd; do
    d=$(readlink "$p" 2>/dev/null) || continue
    [[ "$d" == "$1" || "$d" == "$1"/* ]] && return 0
  done
  return 1
}

# path<TAB>branch<TAB>sha for every worktree, the main one first
worktrees() {
  git worktree list --porcelain | awk '
    /^worktree /{p=substr($0,10)} /^HEAD /{h=$2} /^branch /{b=substr($2,12)} /^detached/{b=""}
    /^$/{print p "\t" b "\t" h; p=b=h=""} END{if(p!="") print p "\t" b "\t" h}'
}

list() {
  local path branch sha unpushed changes state
  printf '%-58s %-44s %9s %8s  %s\n' worktree branch unpushed changes state
  while IFS=$'\t' read -r path branch sha; do
    [ -d "$path" ] || { printf '%-58s %-44s %9s %8s  %s\n' "$path" "${branch:-—}" - - "missing (git worktree prune)"; continue; }
    unpushed=$(git rev-list --count "$sha" --not --remotes 2>/dev/null || echo "?")
    changes=$(git -C "$path" status --porcelain 2>/dev/null | wc -l)
    if [ "$changes" != 0 ]; then state="LOCAL ONLY: commit and push"
    elif [ -z "$branch" ]; then state=detached
    elif merged "$branch" "$sha"; then state=merged
    elif [ "$unpushed" != 0 ]; then state="LOCAL ONLY: push"
    else state=pushed; fi
    printf '%-58s %-44s %9s %8s  %s\n' "$path" "${branch:-—}" "$unpushed" "$changes" "$state"
  done < <(worktrees)
}

clean() {
  local apply=0 log path branch sha first=1 here
  [ "${1:-}" = "--apply" ] && apply=1
  log="$(git rev-parse --path-format=absolute --git-common-dir)/worktrees-clean.log"
  here=$(git rev-parse --show-toplevel)
  git worktree prune
  declare -A checked_out=()
  while IFS=$'\t' read -r path branch sha; do
    [ -n "$branch" ] && checked_out[$branch]=1
    if [ "$first" = 1 ]; then first=0; continue; fi # the main worktree holds the repository
    [ -n "$branch" ] || continue
    merged "$branch" "$sha" || continue
    if [ "$path" = "$here" ] || in_use "$path"; then echo "keep $path: in use"; continue; fi
    if [ -n "$(git -C "$path" status --porcelain)" ]; then echo "keep $path: uncommitted changes"; continue; fi
    echo "remove worktree $path ($branch, merged)"
    if [ "$apply" = 1 ]; then
      # --force only because git refuses any worktree holding submodules; status was empty above
      git worktree remove --force "$path"
      unset "checked_out[$branch]"
    fi
  done < <(worktrees)
  local b s
  while read -r b s; do
    [ -n "${checked_out[$b]:-}" ] && continue
    [ "$b" = main ] && continue
    merged "$b" "$s" || continue
    echo "delete branch $b ($s, merged)"
    if [ "$apply" = 1 ]; then
      printf '%s\tgit branch %s %s\n' "$(date -u +%FT%TZ)" "$b" "$s" >>"$log"
      git branch -D "$b" >/dev/null
    fi
  done < <(git for-each-ref --format='%(refname:short) %(objectname)' refs/heads)
  if [ "$apply" = 1 ]; then echo "restore a branch with its line in $log"; else echo "dry run: add --apply"; fi
}

self_test() {
  local t fail=0 out
  t=$(mktemp -d); trap 'rm -rf "$t"' RETURN
  export WORKTREES_NO_GH=1 GIT_AUTHOR_NAME=t GIT_AUTHOR_EMAIL=t@t GIT_COMMITTER_NAME=t GIT_COMMITTER_EMAIL=t@t
  git init -q --bare "$t/origin.git"
  git clone -q "$t/origin.git" "$t/main" 2>/dev/null
  (
    cd "$t/main"
    git checkout -q -b main; git commit -q --allow-empty -m root; git push -q origin main
    git worktree add -q "$t/done" -b goal/1-done
    git -C "$t/done" commit -q --allow-empty -m "merged work"
    git merge -q --ff-only goal/1-done; git push -q origin main
    git worktree add -q "$t/dirty" -b goal/2-dirty main; echo x >"$t/dirty/new"
    git worktree add -q "$t/wip" -b goal/3-wip main; git -C "$t/wip" commit -q --allow-empty -m wip
    git branch goal/4-merged-no-tree main
    git fetch -q origin
  )
  out=$(cd "$t/main" && list)
  grep -q "goal/3-wip .* 1 .*LOCAL ONLY" <<<"$out" || { echo "self-test: wip not flagged local-only" >&2; fail=1; }
  out=$(cd "$t/main" && clean --apply)
  [ -d "$t/done" ] && { echo "self-test: merged clean worktree kept" >&2; fail=1; }
  [ -d "$t/dirty" ] || { echo "self-test: dirty worktree removed" >&2; fail=1; }
  [ -d "$t/wip" ] || { echo "self-test: unmerged worktree removed" >&2; fail=1; }
  git -C "$t/main" rev-parse -q --verify refs/heads/goal/4-merged-no-tree >/dev/null && { echo "self-test: merged branch kept" >&2; fail=1; }
  git -C "$t/main" rev-parse -q --verify refs/heads/goal/3-wip >/dev/null || { echo "self-test: unmerged branch deleted" >&2; fail=1; }
  grep -q "git branch goal/1-done " "$t/main/.git/worktrees-clean.log" || { echo "self-test: no recovery line" >&2; fail=1; }
  [ "$fail" -eq 0 ] && echo "worktrees self-test: ok"
  return "$fail"
}

case "${1:-list}" in
  list) git fetch -q origin 2>/dev/null || true; list ;;
  clean) git fetch -q --prune origin; clean "${2:-}" ;;
  --self-test) self_test ;;
  *) echo "usage: $0 [list | clean [--apply] | --self-test]" >&2; exit 64 ;;
esac
