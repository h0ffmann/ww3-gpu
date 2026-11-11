#!/usr/bin/env bash
# Build pubs/preprint: Markdown -> pandoc (pandoc-crossref, citeproc) -> LaTeX -> XeLaTeX twice,
# with the template and recipe of h0ffmann/closed-source's Paper Fabric (justfile `_pdf` at 6b3aae5).
# Writes pubs/preprint/wave-models-on-gpus.tex (the aiXiv source) and wave-models-on-gpus.pdf, and the pt-BR edition
# wave-models-on-gpus.pt-BR.tex and .pdf from prose-pt-BR/.
# Tools: pandoc, pandoc-crossref, xelatex, as in h0ffmann/picasso's labs/paper shell.
set -euo pipefail

here="$(cd "$(dirname "$0")/../pubs/preprint" && pwd)"
if [ "${1:-}" = "--self-test" ]; then
  for f in metadata.md refs.bib format/template.tex format/crossref.yaml; do
    [ -f "$here/$f" ] || { echo "preprint: missing $f" >&2; exit 1; }
  done
  ls "$here"/prose/*.md > /dev/null
  for f in ww3-gpu-runtimes.pdf ww3-gpu-runtimes-pt.pdf; do
    [ -f "$here/../../docs/archify/$f" ] || { echo "preprint: missing docs/archify/$f" >&2; exit 1; }
  done
  echo "preprint: self-test ok" >&2
  exit 0
fi
for t in pandoc pandoc-crossref xelatex; do
  command -v "$t" > /dev/null || { echo "preprint: $t not found; enter picasso's labs/paper shell" >&2; exit 1; }
done

# The figures are the dark Archify drawings, printed by `scripts/archify.sh pdf`; the copies in
# figures/ go with the .tex to aiXiv, and are made again here so they never trail their source.
archify="$(cd "$here/../../docs/archify" && pwd)"
mkdir -p "$here/figures"
cp "$archify/ww3-gpu-runtimes.pdf" "$here/figures/runtimes.pdf"
cp "$archify/ww3-gpu-runtimes-pt.pdf" "$here/figures/runtimes.pt-BR.pdf"

build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
printf '%s+%s' "$(git -C "$here" rev-parse --short HEAD)" "$(git -C "$here" rev-list --count HEAD)" \
  > "$build/.current-revision"

one() {  # out prose-dir: the edition's .tex and .pdf; prose-pt-BR/00-metadata.md overrides metadata.md
  pandoc --from markdown+fancy_lists+raw_tex -F pandoc-crossref --citeproc \
    --metadata link-citations=true --number-sections --top-level-division=section \
    --fail-if-warnings -s --template "$here/format/template.tex" -t latex \
    --metadata-file "$here/format/crossref.yaml" --bibliography "$here/refs.bib" \
    -o "$here/$1.tex" "$here/metadata.md" "$here/$2"/*.md
  cp "$here/$1.tex" "$build/"
  [ ! -d "$here/figures" ] || cp -r "$here/figures" "$build/"
  (cd "$build" && for _ in 1 2; do
    xelatex -interaction=nonstopmode -halt-on-error "$1.tex" > /dev/null \
      || { tail -n 30 "$1.log" >&2; exit 1; }
  done)
  cp "$build/$1.pdf" "$here/$1.pdf"
  echo "preprint: $here/$1.pdf ($(pdfinfo "$here/$1.pdf" | awk '/^Pages/ {print $2}') pages)" >&2
}

one wave-models-on-gpus prose
# The Brazilian Portuguese edition, in Kayque Rotondo's Português Técnico Controlado (PTC).
[ ! -d "$here/prose-pt-BR" ] || one wave-models-on-gpus.pt-BR prose-pt-BR
