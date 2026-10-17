#!/usr/bin/env bash
# archify — the Archify skill is a pinned submodule (vendor/archify), not a copy; .claude/skills/archify
# is a symlink into it. Git cannot submodule a subdirectory and sparse-checkout is local state, so a
# fresh clone runs `init` once (as picasso does, scripts/ww-lab-tool-setup.sh).
#
#   scripts/archify.sh init             # fetch the pinned commit, shallow, only archify/ on disk
#   scripts/archify.sh render [slug..]  # docs/archify/<slug>.json -> <slug>.html (git-ignored)
#   scripts/archify.sh pdf [slug..]     # rendered <slug>.html -> <slug>.pdf (committed), headless Chrome
#   scripts/archify.sh check            # list drawings whose source or Archify pin moved; exit 1 if any
#   scripts/archify.sh refresh          # render and print only those, then rewrite pdf.lock
#   scripts/archify.sh --dry-run <cmd>  # print the commands instead of running them
#
# docs/archify/pdf.lock keeps, per drawing, the sha256 of its JSON and the Archify commit that drew
# its PDF, so `check` needs neither node nor Chrome (as pubs/figures/mermaid/index.json does for
# Mermaid). Nothing runs this on a merge: a person refreshes the drawings when a source changes.
set -euo pipefail

root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
sub="vendor/archify"
dir="$root/docs/archify"
lock="$dir/pdf.lock"
dry=0
run() { if [ "$dry" -eq 1 ]; then echo "+ $*"; else "$@"; fi; }

# A source is <slug>.json; finalize writes its receipts (<slug>.finalize.json, ...) beside it.
slugs() {
    local json s
    for json in "$dir"/*.json; do
        s="$(basename "$json" .json)"
        case "$s" in *.*) ;; *) echo "$s" ;; esac
    done
}
pin() { git -C "$root" ls-files -s -- "$sub" | awk '{print $2}'; }
want() { printf '%s\t%s\t%s\n' "$1" "$(sha256sum "$dir/$1.json" | cut -d' ' -f1)" "$(pin)"; }
stale() {
    local s names
    mapfile -t names < <(slugs)
    for s in "${names[@]}"; do
        if [ ! -f "$dir/$s.pdf" ] || ! grep -qxF "$(want "$s")" "$lock" 2>/dev/null; then echo "$s"; fi
    done
}

tmp=""
tmpdir() { [ -n "$tmp" ] || { tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT; }; }

need_archify() {
    [ "$dry" -eq 1 ] || [ -f "$root/$sub/archify/bin/archify.mjs" ] \
        || { echo "archify: $sub is not checked out; run scripts/archify.sh init" >&2; exit 1; }
}

# finalize's receipts go to a temporary --out-dir: left beside the page, an old receipt makes the next
# finalize of a changed source fail (viewer/evidence-path-conflict).
render() {
    local s json type
    need_archify
    tmpdir
    for s in "$@"; do
        json="$dir/$s.json"
        type="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["diagram_type"])' "$json")"
        run env ARCHIFY_UPDATE_CHECK_DISABLED=1 node "$root/$sub/archify/bin/archify.mjs" \
            finalize "$type" "$json" "$dir/$s.html" --out-dir "$tmp/$s" --repo-root "$root" --quality showcase --json
    done
}

# A printed copy of each page gets one @page rule sized from the diagram's viewBox, so the whole
# diagram lands on page 1 (the template's own landscape Letter page cuts a tall sequence);
# the template's print stylesheet still sets the light palette and hides the toolbar.
pdf() {
    local s html page chrome
    chrome="${CHROME:-$(command -v google-chrome || command -v chromium || true)}"
    [ -n "$chrome" ] || { echo "archify: no google-chrome or chromium; set CHROME" >&2; exit 1; }
    tmpdir
    for s in "$@"; do
        html="$dir/$s.html"
        [ "$dry" -eq 1 ] || [ -e "$html" ] || { echo "archify: no $s.html; run scripts/archify.sh render $s" >&2; exit 1; }
        page="$tmp/$s.html"
        [ "$dry" -eq 1 ] || python3 - "$html" "$page" <<'PY'
import re, sys
src = open(sys.argv[1], encoding="utf-8").read()
w, h = map(float, re.search(r'<svg viewBox="0 0 ([\d.]+) ([\d.]+)"', src).groups())
rule = f"<style>@page {{ size: {w + 160:.0f}px {h + 260:.0f}px; margin: 40px; }}</style>"
open(sys.argv[2], "w", encoding="utf-8").write(src.replace("</head>", rule + "</head>", 1))
PY
        run "$chrome" --headless --disable-gpu --no-pdf-header-footer --user-data-dir="$tmp/profile" \
            --print-to-pdf="$dir/$s.pdf" "file://$page"
    done
}

write_lock() {
    local s names
    mapfile -t names < <(slugs)
    { echo "# slug, sha256 of docs/archify/<slug>.json, vendor/archify commit; written by scripts/archify.sh refresh"
      for s in "${names[@]}"; do want "$s"; done; } > "$lock"
}

all() { mapfile -t sel < <(slugs); }

[ "${1:-}" = --dry-run ] && { dry=1; shift; }
cmd="${1:-}"; [ $# -gt 0 ] && shift
for s in "$@"; do
    case "$s" in */*|*..*|"") echo "archify: '$s' is not a slug of docs/archify/" >&2; exit 2 ;; esac
done
case "$cmd" in
    init)
        run git -C "$root" submodule update --init --depth 1 -- "$sub"
        run git -C "$root/$sub" sparse-checkout set --no-cone /archify/
        ;;
    render) if [ $# -gt 0 ]; then render "$@"; else all; render "${sel[@]}"; fi ;;
    pdf)    if [ $# -gt 0 ]; then pdf "$@"; else all; pdf "${sel[@]}"; fi ;;
    check)
        mapfile -t old < <(stale)
        [ ${#old[@]} -eq 0 ] && { echo "archify: $(slugs | wc -l) drawings current"; exit 0; }
        printf 'archify: stale: %s\n' "${old[@]}"; echo "run: just archify refresh" >&2; exit 1
        ;;
    refresh)
        mapfile -t old < <(stale)
        [ ${#old[@]} -eq 0 ] && { echo "archify: $(slugs | wc -l) drawings current, nothing to redraw"; exit 0; }
        printf 'archify: redrawing %s\n' "${old[@]}"
        render "${old[@]}"
        pdf "${old[@]}"
        if [ "$dry" -eq 1 ]; then echo "+ rewrite $lock"; else write_lock; fi
        ;;
    *) sed -n '2,11p' "$0"; exit 2 ;;
esac
