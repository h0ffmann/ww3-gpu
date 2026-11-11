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
#   scripts/archify.sh png [slug..]     # dark.tsv's drawings -> <slug>.png (committed), the dark look
#   scripts/archify.sh site <dir>       # dark.tsv's drawings -> <dir>/<slug>/index.html, for Pages
#   scripts/archify.sh --dry-run <cmd>  # print the commands instead of running them
#
# The dark look (DESIGN.md, "Pictures"; the manner of piabrota/nix-os's `just figure`) is
# docs/archify/ww3-gpu.css set over Archify's page by scripts/archify_dress.py: only the drawing
# stays, on the site's ground in its lights. docs/archify/dark.tsv lists the drawings redone in it,
# and png.lock records what drew each PNG.
#
# docs/archify/pdf.lock keeps, per drawing, the sha256 of its JSON and the Archify commit that drew
# its PDF, so `check` needs neither node nor Chrome (as pubs/figures/mermaid/index.json does for
# Mermaid). Nothing runs this on a merge: a person refreshes the drawings when a source changes.
set -euo pipefail

root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
sub="vendor/archify"
dir="$root/docs/archify"
lock="$dir/pdf.lock"
pnglock="$dir/png.lock"
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

# dark.tsv: "<slug>\t<height>" for each drawing in the dark look.
dark() { awk -F'\t' '!/^#/ && NF >= 2 {print $1}' "$dir/dark.tsv"; }
want_png() {
    printf '%s\t%s\t%s\n' "$1" "$(cat "$dir/$1.json" "$dir/ww3-gpu.css" "$dir/dark.tsv" \
        "$root/scripts/archify_dress.py" | sha256sum | cut -d' ' -f1)" "$(pin)"
}
stale_png() {
    local s names
    mapfile -t names < <(dark)
    for s in "${names[@]}"; do
        if [ ! -f "$dir/$s.png" ] || ! grep -qxF "$(want_png "$s")" "$pnglock" 2>/dev/null; then echo "$s"; fi
    done
}
kind() { python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["diagram_type"])' "$1"; }
title() { python3 -c 'import html,json,sys; print(html.escape(json.load(open(sys.argv[1]))["meta"]["title"]))' "$1"; }

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
        type="$(kind "$json")"
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

# png: the README's picture of each drawing, in the dark look, at twice its size.
png() {
    local s w h chrome names
    chrome="${CHROME:-$(command -v google-chrome || command -v chromium || true)}"
    [ -n "$chrome" ] || { echo "archify: no google-chrome or chromium; set CHROME" >&2; exit 1; }
    render "$@"
    tmpdir
    for s in "$@"; do
        if [ "$dry" -eq 1 ]; then echo "+ archify_dress.py + archify_shot.py -> $s.png"; continue; fi
        python3 "$root/scripts/archify_dress.py" "$dir/$s.html" "$tmp/$s.frame.html" "$s"
        read -r w h < <(grep -o '<svg viewBox="[^"]*"' "$tmp/$s.frame.html" | head -1 \
            | sed -E 's/.*0 0 ([0-9.]+) ([0-9.]+)"/\1 \2/')
        python3 "$root/scripts/archify_shot.py" "$chrome" "$tmp/$s.frame.html" "$dir/$s.png" "${w%.*}" "${h%.*}"
        echo "archify: docs/archify/$s.png"
    done
    if [ "$dry" -eq 1 ]; then echo "+ rewrite $pnglock"; return; fi
    mapfile -t names < <(dark)
    { echo "# slug, sha256 of its JSON + ww3-gpu.css + dark.tsv + archify_dress.py, vendor/archify commit; written by scripts/archify.sh png"
      for s in "${names[@]}"; do
          if printf '%s\n' "$@" | grep -qxF "$s"; then want_png "$s"
          else awk -F'\t' -v s="$s" '$1 == s' "$pnglock" 2>/dev/null || true; fi
      done; } > "$tmp/png.lock"
    mv "$tmp/png.lock" "$pnglock"
}

# site <dir>: a page for each drawing of dark.tsv at <dir>/<slug>/, and <dir>/index.html listing
# them. Pages runs it (.github/workflows/pages.yml). finalize's exit 2 (its browser check skipped,
# no Chrome) is accepted there; a failure of its other gates is not.
site() {
    local out="$1" s t status items="" q="https://github.com/h0ffmann/ww3-gpu/blob/main/docs/archify" names
    need_archify
    tmpdir
    mapfile -t names < <(dark)
    for s in "${names[@]}"; do
        if [ "$dry" -eq 1 ]; then echo "+ finalize $s; archify_dress.py --bar -> $out/$s/index.html"; continue; fi
        set +e
        env ARCHIFY_UPDATE_CHECK_DISABLED=1 node "$root/$sub/archify/bin/archify.mjs" finalize "$(kind "$dir/$s.json")" \
            "$dir/$s.json" "$tmp/$s.html" --out-dir "$tmp/$s" --repo-root "$root" --quality showcase > "$tmp/$s.log" 2>&1
        status=$?
        set -e
        if [ "$status" != 0 ] && [ "$status" != 2 ]; then cat "$tmp/$s.log" >&2; echo "archify: finalize failed on $s" >&2; exit 1; fi
        t="$(title "$dir/$s.json")"
        cat > "$tmp/$s.bar.html" <<EOF
<nav class="wg-bar" aria-label="Figure"><span><b>$t</b><a href="../">all figures</a> · <a href="../../">ww3-gpu</a> · <a href="$q/$s.json">source</a></span><span><span class="k"><i style="color:#36B7FF"></i>a source read</span> <span class="k"><i style="color:#FFB547"></i>built or run here</span> <span class="k"><i style="color:#3DFF96;border-style:dashed"></i>same bits as</span></span></nav>
EOF
        mkdir -p "$out/$s"
        python3 "$root/scripts/archify_dress.py" "$tmp/$s.html" "$out/$s/index.html" "$s" --bar "$tmp/$s.bar.html"
        items+="<li><a href=\"$s/\">$t</a> <span>· <a href=\"$q/$s.json\">source</a></span></li>"
        echo "archify: $out/$s/index.html"
    done
    [ "$dry" -eq 1 ] && return
    cat > "$out/index.html" <<EOF
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>ww3-gpu · figures</title><link rel="icon" href="../favicon.svg"><link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Bricolage+Grotesque:wght@600;800&amp;family=JetBrains+Mono&amp;family=Newsreader&amp;display=swap"><style>
body{margin:0;padding:48px 16px;background:radial-gradient(60rem 24rem at 100% -20%,color-mix(in srgb,#36B7FF 14%,transparent),transparent 70%),#05090E;color:#E7F2EF;font:400 18px/1.5 Newsreader,Georgia,serif}
main{max-width:720px;margin:0 auto}h1{font:800 40px/1.1 'Bricolage Grotesque',system-ui,sans-serif;color:#36B7FF;margin:0 0 8px}
p{color:#8FA3AD}ul{list-style:none;padding:0;border-top:1px solid #1C2B38}li{padding:14px 0;border-bottom:1px solid #1C2B38}
a{color:#36B7FF}li>a{font:600 20px 'Bricolage Grotesque',system-ui,sans-serif;text-decoration:none}li span{font:400 13px 'JetBrains Mono',monospace;color:#8FA3AD}
a:focus-visible{outline:2px solid #36B7FF;outline-offset:3px;border-radius:6px}
</style></head><body><main><h1>figures</h1><p>Drawn with <a href="https://github.com/tt-a1i/archify">Archify</a> (MIT) from the sources in <a href="https://github.com/h0ffmann/ww3-gpu/tree/main/docs/archify">docs/archify/</a>. Each page is one drawing you can zoom. <a href="../">ww3-gpu</a></p><ul>$items</ul></main></body></html>
EOF
    echo "archify: $out/index.html"
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
[ "$cmd" = site ] || for s in "$@"; do
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
        mapfile -t old < <(stale; stale_png | sed 's/$/ (png)/')
        [ ${#old[@]} -eq 0 ] && { echo "archify: $(slugs | wc -l) drawings current"; exit 0; }
        printf 'archify: stale: %s\n' "${old[@]}"; echo "run: just archify refresh" >&2; exit 1
        ;;
    refresh)
        mapfile -t old < <(stale)
        mapfile -t oldpng < <(stale_png)
        [ ${#old[@]} -eq 0 ] && [ ${#oldpng[@]} -eq 0 ] && { echo "archify: $(slugs | wc -l) drawings current, nothing to redraw"; exit 0; }
        if [ ${#old[@]} -gt 0 ]; then
            printf 'archify: redrawing %s\n' "${old[@]}"
            render "${old[@]}"
            pdf "${old[@]}"
            if [ "$dry" -eq 1 ]; then echo "+ rewrite $lock"; else write_lock; fi
        fi
        [ ${#oldpng[@]} -eq 0 ] || png "${oldpng[@]}"
        ;;
    png)    if [ $# -gt 0 ]; then png "$@"; else mapfile -t sel < <(dark); png "${sel[@]}"; fi ;;
    site)   [ $# -eq 1 ] || { echo "archify: site <dir>" >&2; exit 2; }; site "$1" ;;
    *) sed -n '2,13p' "$0"; exit 2 ;;
esac
