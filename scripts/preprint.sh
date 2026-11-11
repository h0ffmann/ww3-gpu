#!/usr/bin/env bash
# Build pubs/preprint: Markdown -> pandoc (pandoc-crossref, citeproc) -> LaTeX -> XeLaTeX twice,
# with the template and recipe of h0ffmann/closed-source's Paper Fabric (justfile `_pdf` at 6b3aae5).
# Writes pubs/preprint/two-ports.tex (the aiXiv source) and two-ports.pdf.
# Tools: pandoc, pandoc-crossref, xelatex, as in h0ffmann/picasso's labs/paper shell.
set -euo pipefail

here="$(cd "$(dirname "$0")/../pubs/preprint" && pwd)"
if [ "${1:-}" = "--self-test" ]; then
  for f in metadata.md refs.bib format/template.tex format/crossref.yaml; do
    [ -f "$here/$f" ] || { echo "preprint: missing $f" >&2; exit 1; }
  done
  ls "$here"/prose/*.md > /dev/null
  echo "preprint: self-test ok" >&2
  exit 0
fi
for t in pandoc pandoc-crossref xelatex; do
  command -v "$t" > /dev/null || { echo "preprint: $t not found; enter picasso's labs/paper shell" >&2; exit 1; }
done

build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
printf '%s+%s' "$(git -C "$here" rev-parse --short HEAD)" "$(git -C "$here" rev-list --count HEAD)" \
  > "$build/.current-revision"

pandoc --from markdown+fancy_lists+raw_tex -F pandoc-crossref --citeproc \
  --metadata link-citations=true --number-sections --top-level-division=section \
  --fail-if-warnings -s --template "$here/format/template.tex" -t latex \
  --metadata-file "$here/format/crossref.yaml" --bibliography "$here/refs.bib" \
  -o "$here/two-ports.tex" "$here/metadata.md" "$here"/prose/*.md

cp "$here/two-ports.tex" "$build/"
(cd "$build" && for _ in 1 2; do
  xelatex -interaction=nonstopmode -halt-on-error two-ports.tex > /dev/null \
    || { tail -n 30 two-ports.log >&2; exit 1; }
done)
cp "$build/two-ports.pdf" "$here/two-ports.pdf"
echo "preprint: $here/two-ports.pdf ($(pdfinfo "$here/two-ports.pdf" | awk '/^Pages/ {print $2}') pages)" >&2
