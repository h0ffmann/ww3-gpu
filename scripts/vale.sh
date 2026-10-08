#!/usr/bin/env bash
# vale — lint this repo's English Markdown for AI-writing tells (issue #36).
#
#   just vale            # the gate CI runs: .vale.ini, error-level rules only, fails on a hit
#   just vale --report   # every ai-tells rule as suggestions (.vale-report.ini), never fails
#   just vale FILE...    # only those files; other --flags go to vale (--output=line)
#   just vale --self-test
#
# Out of scope: the submodules, .claude/ (vendored skills quote the tells as examples),
# pubs/ (the proposal has its own reviewer and house style) and Portuguese files
# (*.pt.md, *.pt-BR.md), which the English rules would misread.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

scope() {
  git ls-files '*.md' | grep -v -E '^(WW3|WW4|nix-config|bend-lang|\.claude|\.specify|pubs|\.vale)/' \
    | grep -v -E '\.pt(-BR)?\.md$'
}

if [[ "${1:-}" == --self-test ]]; then
  files="$(scope)"
  grep -q '^README.md$' <<<"$files"
  grep -q '^docs/GLOSSARY.md$' <<<"$files"
  if grep -E '^(pubs|\.claude)/|\.pt(-BR)?\.md$' <<<"$files"; then exit 1; fi
  echo "vale.sh self-test: ok ($(wc -l <<<"$files") files in scope)"
  exit 0
fi

config=.vale.ini; opts=(); files=()
for arg in "$@"; do
  case "$arg" in
    --report) config=.vale-report.ini; opts+=(--no-exit) ;;
    -*) opts+=("$arg") ;;
    *) files+=("$arg") ;;
  esac
done
command -v vale >/dev/null || { echo "vale not on PATH (https://vale.sh/docs/install)" >&2; exit 2; }

vale --config="$config" sync >/dev/null
((${#files[@]})) || mapfile -t files < <(scope)
vale --config="$config" "${opts[@]}" "${files[@]}" </dev/null
