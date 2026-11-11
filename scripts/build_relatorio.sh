#!/usr/bin/env bash
# build_relatorio — the easy pt-BR report for Prof. Pedro Cruz (GTA, DEL/UFRJ), full and short, from two Markdown sources.
#   scripts/build_relatorio.sh          -> build/site/relatorio-gta/{index.html,curto.html,style.css,figures/}
#   scripts/build_relatorio.sh --pdf    -> the same, then pdf/relatorio-gta-pedro-cruz{,-curto}.pdf, printed by Chromium
#   scripts/build_relatorio.sh --self-test
# The pages are built in CI by build_html.sh; the PDFs are committed because
# the Pages runner has no Chromium. OUT_DIR overrides build/; CHROMIUM names the browser.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dir="$root/pubs/relatorio-gta"
out="${OUT_DIR:-$root/build}/site/relatorio-gta"
raw="https://github.com/h0ffmann/ww3-gpu/raw/main/pdf"
# source:page:pdf; the short version has no figures, so only the full one is held to having them.
reports=("relatorio.easy.pt-BR.md:index.html:relatorio-gta-pedro-cruz.pdf"
         "relatorio.curto.pt-BR.md:curto.html:relatorio-gta-pedro-cruz-curto.pdf")

# Every figure a source names, as ../figures/mermaid/<id>.png (relative to the source).
figures() { grep -oE '\]\(\.\./figures/mermaid/[a-z0-9-]+\.png\)' "$1" | sed -E 's|.*/([a-z0-9-]+\.png)\)|\1|' | sort -u || true; }

if [ "${1:-}" = --self-test ]; then
  n="$(figures "$dir/relatorio.easy.pt-BR.md" | wc -l)"
  [ "$n" -gt 0 ] || { echo "build_relatorio: no figure found in the full report" >&2; exit 1; }
  for r in "${reports[@]}"; do
    src="$dir/${r%%:*}"
    [ -f "$src" ] || { echo "build_relatorio: missing $src" >&2; exit 1; }
    for f in $(figures "$src"); do
      [ -f "$root/pubs/figures/mermaid/$f" ] || { echo "build_relatorio: missing render $f" >&2; exit 1; }
    done
  done
  echo "build_relatorio: self-test ok (${#reports[@]} reports, $n figures in the full one)"
  exit 0
fi

rm -rf "$out"
mkdir -p "$out/figures"
cp "$dir/style.css" "$out/style.css"
for r in "${reports[@]}"; do
  IFS=: read -r name page pdf <<<"$r"
  src="$dir/$name"
  for f in $(figures "$src"); do cp "$root/pubs/figures/mermaid/$f" "$out/figures/"; done
  nav="<nav class=\"site-links\"><a href=\"../index.html\">ww3-gpu</a> · <a href=\"../pt-BR/\">português</a> · <a href=\"../adrs/\">decisões</a> · <a href=\"index.html\">versão completa</a> · <a href=\"curto.html\">versão curta</a> · <a href=\"$raw/$pdf\">este relatório em PDF</a></nav>"
  sed 's|](\.\./figures/mermaid/|](figures/|g' "$src" |
    pandoc --from markdown-smart --to html5 --standalone \
      --css style.css --include-before-body <(printf '%s\n' "$nav") \
      --resource-path "$out" --fail-if-warnings -o "$out/$page"
  echo "build_relatorio: $out/$page"
done

if [ "${1:-}" = --pdf ]; then
  chrome="${CHROMIUM:-$(command -v chromium || command -v chromium-browser || echo /opt/pw-browsers/chromium-1194/chrome-linux/chrome)}"
  for r in "${reports[@]}"; do
    IFS=: read -r _ page pdf <<<"$r"
    "$chrome" --headless=new --no-sandbox --disable-gpu --no-pdf-header-footer \
      --print-to-pdf="$root/pdf/$pdf" "file://$out/$page" 2>/dev/null
    [ -s "$root/pdf/$pdf" ] || { echo "build_relatorio: Chromium wrote no $pdf" >&2; exit 1; }
    echo "build_relatorio: $root/pdf/$pdf"
  done
fi
