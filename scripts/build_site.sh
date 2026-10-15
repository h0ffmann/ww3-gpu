#!/usr/bin/env bash
# build_site — the site's front page (Gleam + Lustre, site/) into build/site, next to the book.
#   scripts/build_site.sh            -> build/site/{index.html,pt-BR/index.html,style.css,favicon.svg}
#   scripts/build_site.sh --check    -> the same, and fail unless each page holds every section
# The pt-BR page is the same source, each sentence looked up in site/data/translations/pt-BR.tsv;
# site/content stops the build when a sentence has no row or a row no sentence.
# Run inside `nix develop .#site` (gleam, node); `just site` does that, after the book
# (scripts/build_html.sh, which writes build/site/book/, andamento/, adrs/ and the PDF).
# The facts come from the repository through site/content, which writes the content module;
# nothing in site/src is typed by hand (DESIGN.md).
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="${OUT_DIR:-$root/build}/site"
check=0
[ "${1:-}" = --check ] && check=1

cd "$root/site/content"
gleam format --check src test
gleam test
gleam run
cd "$root/site"
gleam format src/ww3_gpu_site/content.gleam
gleam format --check src test
gleam test
mkdir -p "$out/pt-BR"
gleam run -m ww3_gpu_site/prerender --no-print-progress > "$out/index.html"
gleam run -m ww3_gpu_site/prerender_pt_br --no-print-progress > "$out/pt-BR/index.html"
cp -R static/. "$out/"
touch "$out/.nojekyll"

if [ "$check" -eq 1 ]; then
  for page in index.html pt-BR/index.html; do
    for id in book chapters decisions designs brain cite related; do
      grep -q "id=\"$id\"" "$out/$page" || { echo "build_site: no #$id in $page" >&2; exit 1; }
    done
  done
fi
echo "build_site: $out/index.html $out/pt-BR/index.html"
