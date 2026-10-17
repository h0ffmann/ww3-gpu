#!/usr/bin/env bash
# archify — the Archify skill is a pinned submodule (vendor/archify), not a copy; .claude/skills/archify
# is a symlink into it. Git cannot submodule a subdirectory and sparse-checkout is local state, so a
# fresh clone runs `init` once (as picasso does, scripts/ww-lab-tool-setup.sh).
#
#   scripts/archify.sh init             # fetch the pinned commit, shallow, only archify/ on disk
#   scripts/archify.sh render           # docs/archify/<slug>.json -> <slug>.html (git-ignored)
#   scripts/archify.sh pdf              # each rendered <slug>.html -> <slug>.pdf (committed), headless Chrome
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
        # A source is <slug>.json, never <slug>.delivery.json. finalize's receipts go to a temporary
        # --out-dir: left beside the page, an old receipt makes the next finalize of a changed source
        # fail (viewer/evidence-path-conflict).
        tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
        for json in "$root"/docs/archify/*.json; do
            case "$(basename "$json" .json)" in *.*) continue ;; esac
            type="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["diagram_type"])' "$json")"
            run env ARCHIFY_UPDATE_CHECK_DISABLED=1 node "$root/$sub/archify/bin/archify.mjs" \
                finalize "$type" "$json" "${json%.json}.html" --out-dir "$tmp" --repo-root "$root" --quality showcase --json
        done
        ;;
    pdf)
        # A printed copy of each page gets one @page rule sized from the diagram's viewBox, so the whole
        # diagram lands on page 1 (the template's own landscape Letter page cuts a tall sequence);
        # the template's print stylesheet still sets the light palette and hides the toolbar.
        chrome="${CHROME:-$(command -v google-chrome || command -v chromium || true)}"
        [ -n "$chrome" ] || { echo "archify: no google-chrome or chromium; set CHROME" >&2; exit 1; }
        tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
        for html in "$root"/docs/archify/*.html; do
            [ -e "$html" ] || { echo "archify: no pages; run scripts/archify.sh render first" >&2; exit 1; }
            page="$tmp/$(basename "$html")"
            python3 - "$html" "$page" <<'PY'
import re, sys
src = open(sys.argv[1], encoding="utf-8").read()
w, h = map(float, re.search(r'<svg viewBox="0 0 ([\d.]+) ([\d.]+)"', src).groups())
rule = f"<style>@page {{ size: {w + 160:.0f}px {h + 260:.0f}px; margin: 40px; }}</style>"
open(sys.argv[2], "w", encoding="utf-8").write(src.replace("</head>", rule + "</head>", 1))
PY
            run "$chrome" --headless --disable-gpu --no-pdf-header-footer --user-data-dir="$tmp/profile" \
                --print-to-pdf="${html%.html}.pdf" "file://$page"
        done
        ;;
    *) sed -n '2,9p' "$0"; exit 2 ;;
esac
