#!/usr/bin/env bash
# build_html — the book as a static website, one page per chapter, for GitHub Pages (issue #76).
#   scripts/build_html.sh   -> build/site/book/index.html (+ one page per chapter, figures, style.css),
#                              build/site/{ww3-lab-course.pdf,andamento/,adrs/,relatorio-gta/}
# The site's front page, build/site/index.html, is the Gleam landing (site/, `just site`), not this.
# OUT_DIR overrides build/. Needs pandoc >= 3.0 (the chunkedhtml writer); no TeX.
#
# Same inputs and filters as the PDF (book_prep.py, mermaid.lua), so a lesson that builds as a PDF
# builds as a page. Math is drawn in the reader's browser by MathJax from a CDN.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

out_dir="${OUT_DIR:-$root/build}"
root_site="$out_dir/site"
# The chapters live at /book/; the PDF, andamento/ and adrs/ keep their URLs at the root.
site="$root_site/book"
prep="$out_dir/book-html"
rm -rf "$prep" "$site" "$root_site/andamento" "$root_site/adrs"
mkdir -p "$root_site"
python3 "$root/scripts/book_prep.py" "$root/course" "$prep"
inputs=("$prep"/[0-9][0-9]-*.md)
pandoc "${inputs[@]}" \
  --from gfm+tex_math_dollars+footnotes+definition_lists+attributes \
  --to chunkedhtml \
  --split-level=1 \
  --lua-filter "$root/pubs/filters/mermaid.lua" \
  --toc --toc-depth=2 \
  --mathml \
  --resource-path "$root/course" \
  --css style.css \
  --metadata-file "$root/pubs/site/meta.yaml" \
  --include-before-body "$root/pubs/site/nav.html" \
  --fail-if-warnings \
  -o "$site"
cp "$root/pubs/site/style.css" "$site/style.css"
# The project progress page (a hand-made snapshot, not built from course/) at /andamento/.
cp -r "$root/pubs/site/andamento" "$root_site/andamento"
# The ADR list at /adrs/, filled from docs/ADRs/ on every build so it cannot go stale.
mkdir -p "$root_site/adrs"
python3 "$root/scripts/adrs.py" --site "$root/pubs/site/adrs/index.html" "$root_site/adrs/index.html"
# The easy pt-BR report for the GTA (DEL/UFRJ) at /relatorio-gta/; its PDF is committed in pdf/.
OUT_DIR="$out_dir" bash "$root/scripts/build_relatorio.sh"
# The PDF is the one pubs.yml rebuilds and commits to pdf/; Pages has no TeX to build it again.
cp "$root/pdf/ww3-lab-course.pdf" "$root_site/ww3-lab-course.pdf"
# mermaid.lua points each figure at its committed render by absolute path, which chunkedhtml keeps
# as is (and --extract-media drops some of them); serve the renders next to the pages instead.
renders="$root/pubs/figures/mermaid"
mkdir -p "$site/figures"
cp "$renders"/*.png "$site/figures/"
sed -i "s|src=\"$renders/|src=\"figures/|g" "$site"/*.html
# A link from a lesson to a file of the repository goes to that file on GitHub, as on Leanpub.
blob="https://github.com/h0ffmann/ww3-gpu/blob/main"
sed -i -E -e "s|href=\"\\.\\./|href=\"$blob/|g" \
  -e "s|href=\"([A-Za-z0-9_.-]+\\.md)\"|href=\"$blob/course/\\1\"|g" "$site"/*.html
# The nav (pubs/site/nav.html) names the site's root as SITE_ROOT, so the rewrite above leaves it.
sed -i 's|href="SITE_ROOT/|href="../|g' "$site"/*.html
if grep -l "src=\"/" "$site"/*.html; then echo "build_html: a page still points at an absolute path" >&2; exit 1; fi
touch "$root_site/.nojekyll" # serve the files as built; Pages would otherwise run Jekyll over them
echo "build_html: $site/index.html"
