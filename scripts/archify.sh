#!/usr/bin/env bash
# archify — the Archify skill is a pinned submodule (vendor/archify), not a copy; .claude/skills/archify
# is a symlink into it. Git cannot submodule a subdirectory and sparse-checkout is local state, so a
# fresh clone runs `init` once (as picasso does, scripts/ww-lab-tool-setup.sh).
#
#   scripts/archify.sh init             # fetch the pinned commit, shallow, only archify/ on disk
#   scripts/archify.sh render           # docs/archify/<slug>.json -> <slug>.html (git-ignored)
#   scripts/archify.sh --dry-run <cmd>  # print the commands instead of running them
set -euo pipefail

root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
sub="vendor/archify"
dry=0
run() { if [ "$dry" -eq 1 ]; then echo "+ $*"; else "$@"; fi; }

[ "${1:-}" = --dry-run ] && { dry=1; shift; }
case "${1:-}" in
    init)
        run git -C "$root" submodule update --init --depth 1 -- "$sub"
        run git -C "$root/$sub" sparse-checkout set --no-cone /archify/
        ;;
    render)
        [ "$dry" -eq 1 ] || [ -f "$root/$sub/archify/bin/archify.mjs" ] \
            || { echo "archify: $sub is not checked out; run scripts/archify.sh init" >&2; exit 1; }
        for json in "$root"/docs/archify/*.json; do
            type="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["diagram_type"])' "$json")"
            run env ARCHIFY_UPDATE_CHECK_DISABLED=1 node "$root/$sub/archify/bin/archify.mjs" \
                finalize "$type" "$json" "${json%.json}.html" --repo-root "$root" --quality showcase --json
        done
        ;;
    *) sed -n '2,8p' "$0"; exit 2 ;;
esac
