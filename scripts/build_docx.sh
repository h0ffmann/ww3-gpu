#!/usr/bin/env bash
# build_docx — the proposal as a Word file, for readers who comment or edit rather than read.
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
usage() { sed -n '2,3p' "$0" >&2; exit 2; }

target="${1:-}"; shift || true
case "$target" in
  proposal) lang="${1:-pt}"; style="${2:-abnt}" ;;
  *) usage ;;
esac
case "${lang:-pt}" in pt|en) ;; *) usage ;; esac
case "$style" in abnt|ieee) ;; *) usage ;; esac

out_dir="${OUT_DIR:-$root/build}"
mkdir -p "$out_dir"
csl="$root/pubs/csl/$style.csl"

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
  --lua-filter "$root/pubs/filters/cronograma.lua" \
  --top-level-division=section --number-sections \
  --citeproc --bibliography "$root/pubs/proposal/refs.bib" --csl "$csl" \
  -o "$out_dir/proposal_$lang.docx"
echo "build_docx: $out_dir/proposal_$lang.docx"
