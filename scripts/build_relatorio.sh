#!/usr/bin/env bash
# build_relatorio — the easy pt-BR report for Prof. Pedro Cruz (GTA, DEL/UFRJ), from one Markdown source.
#   scripts/build_relatorio.sh          -> build/site/relatorio-gta/{index.html,style.css,figures/}
#   scripts/build_relatorio.sh --pdf    -> the same, then pdf/relatorio-gta-pedro-cruz.pdf, printed by Chromium
#   scripts/build_relatorio.sh --self-test
# The page is built in CI by build_html.sh; the PDF is committed, as pdf/ww3-lab-course.pdf is, because
# the Pages runner has no Chromium. OUT_DIR overrides build/; CHROMIUM names the browser.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
src="$root/pubs/relatorio-gta/relatorio.easy.pt-BR.md"
out="${OUT_DIR:-$root/build}/site/relatorio-gta"
pdf="$root/pdf/relatorio-gta-pedro-cruz.pdf"

# Every figure the page names, as ../figures/mermaid/<id>.png (relative to the source).
figures() { grep -oE '\]\(\.\./figures/mermaid/[a-z0-9-]+\.png\)' "$src" | sed -E 's|.*/([a-z0-9-]+\.png)\)|\1|' | sort -u; }

if [ "${1:-}" = --self-test ]; then
  n="$(figures | wc -l)"
  [ "$n" -gt 0 ] || { echo "build_relatorio: no figure found in $src" >&2; exit 1; }
  for f in $(figures); do
    [ -f "$root/pubs/figures/mermaid/$f" ] || { echo "build_relatorio: missing render $f" >&2; exit 1; }
  done
  echo "build_relatorio: self-test ok ($n figures)"
  exit 0
fi

rm -rf "$out"
mkdir -p "$out/figures"
for f in $(figures); do cp "$root/pubs/figures/mermaid/$f" "$out/figures/"; done
cp "$root/pubs/relatorio-gta/style.css" "$out/style.css"
nav='<nav class="site-links"><a href="../index.html">ww3-gpu</a> · <a href="../pt-BR/">português</a> · <a href="../book/">o livro</a> · <a href="../adrs/">decisões</a> · <a href="https://github.com/h0ffmann/ww3-gpu/raw/main/pdf/relatorio-gta-pedro-cruz.pdf">este relatório em PDF</a></nav>'
sed 's|](\.\./figures/mermaid/|](figures/|g' "$src" |
  pandoc --from markdown-smart --to html5 --standalone \
    --css style.css --include-before-body <(printf '%s\n' "$nav") \
    --resource-path "$out" --fail-if-warnings -o "$out/index.html"
# The tall flow chart gets a height limit in print, so it stays on one page.
sed -i 's|<img src="figures/triton-port-plan.png"|<img class="tall" src="figures/triton-port-plan.png"|' "$out/index.html"
echo "build_relatorio: $out/index.html"

if [ "${1:-}" = --pdf ]; then
  chrome="${CHROMIUM:-$(command -v chromium || command -v chromium-browser || echo /opt/pw-browsers/chromium-1194/chrome-linux/chrome)}"
  "$chrome" --headless=new --no-sandbox --disable-gpu --no-pdf-header-footer \
    --print-to-pdf="$pdf" "file://$out/index.html" 2>/dev/null
  [ -s "$pdf" ] || { echo "build_relatorio: Chromium wrote no PDF" >&2; exit 1; }
  echo "build_relatorio: $pdf"
fi
