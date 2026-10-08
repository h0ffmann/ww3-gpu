#!/usr/bin/env bash
# build_html — the book as a static website, one page per chapter, for GitHub Pages (issue #76).
#   scripts/build_html.sh   -> build/site/index.html (+ one page per chapter, figures, style.css)
# OUT_DIR overrides build/. Needs pandoc >= 3.0 (the chunkedhtml writer); no TeX.
#
# Same inputs and filters as the PDF (book_prep.py, mermaid.lua), so a lesson that builds as a PDF
# builds as a page. Math is drawn in the reader's browser by MathJax from a CDN.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

out_dir="${OUT_DIR:-$root/build}"
site="$out_dir/site"
prep="$out_dir/book-html"
rm -rf "$prep" "$site"
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
  --fail-if-warnings \
  -o "$site"
cp "$root/pubs/site/style.css" "$site/style.css"
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
if grep -l "src=\"/" "$site"/*.html; then echo "build_html: a page still points at an absolute path" >&2; exit 1; fi
touch "$site/.nojekyll" # serve the files as built; Pages would otherwise run Jekyll over them
echo "build_html: $site/index.html"
