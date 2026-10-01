#!/usr/bin/env bash
# build_docx — the documents as Word files, for readers who comment or edit rather than read.
#   scripts/build_docx.sh book                -> build/ww3-lab-course.docx
#   scripts/build_docx.sh proposal [pt|en]    -> build/proposal_<lang>.docx
# OUT_DIR overrides build/. Runs inside `nix develop .` or the Nix sandbox (nix build .#*-docx).
#
# Not the PDF path: pandoc's docx writer ignores the LaTeX templates, so the proposal loses the
# DEL cover page and the signature block — it is the text for review in Word, not the document that
# gets signed. Styling would come from a --reference-doc; none is committed, so Word's defaults
# apply. Citations are rendered by citeproc with the same CSL as the PDF, so the reference list and
# the author-date calls match what the advisors see on paper.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
usage() { sed -n '2,4p' "$0" >&2; exit 2; }

target="${1:-book}"; shift || true
case "$target" in
  book) style="${1:-abnt}" ;;
  proposal) lang="${1:-pt}"; style="${2:-abnt}" ;;
  *) usage ;;
esac
case "${lang:-pt}" in pt|en) ;; *) usage ;; esac
case "$style" in abnt|ieee) ;; *) usage ;; esac

out_dir="${OUT_DIR:-$root/build}"
mkdir -p "$out_dir"
csl="$root/pubs/csl/$style.csl"

if [ "$target" = book ]; then
  prep="$out_dir/book-docx"
  rm -rf "$prep"
  python3 "$root/scripts/book_prep.py" "$root/course" "$prep"
  inputs=("$prep"/[0-9][0-9]-*.md) # glob expansion is sorted
  pandoc "${inputs[@]}" \
    --from gfm+tex_math_dollars+footnotes+definition_lists+attributes \
    --to docx \
    --toc --toc-depth=2 --number-sections \
    --resource-path "$root/course" \
    --csl "$csl" \
    --metadata title="WW Lab" \
    --metadata subtitle="A self-paced course on WW3, NOAA's third-generation spectral wave model" \
    --metadata author="Matheus Hoffmann" \
    --metadata lang=en-US \
    -o "$out_dir/ww3-lab-course.docx"
  echo "build_docx: $out_dir/ww3-lab-course.docx"
else
  src="$root/pubs/proposal/$lang"
  inputs=("$src"/[0-9][0-9]-*.md)
  [ -e "${inputs[0]}" ] || { echo "build_docx: no $src/NN-*.md" >&2; exit 1; }
  case "$lang" in pt) plang=pt-BR ;; en) plang=en-US ;; esac
  # Pandoc's own markdown reader, as in build_pdf.sh: the proposal uses citations ([@key]), the
  # ::: {#refs} div and heading attributes ({-}), none of which the gfm reader understands — with
  # gfm the citations reach Word as literal "[@wamdi1988]" and the reference list comes out empty.
  pandoc "${inputs[@]}" \
    --to docx \
    --metadata-file "$root/pubs/proposal/meta.$lang.yaml" \
    --metadata lang="$plang" \
    --top-level-division=section --number-sections \
    --citeproc --bibliography "$root/pubs/proposal/refs.bib" --csl "$csl" \
    -o "$out_dir/proposal_$lang.docx"
  echo "build_docx: $out_dir/proposal_$lang.docx"
fi
