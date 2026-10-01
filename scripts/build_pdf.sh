#!/usr/bin/env bash
# build_pdf — the one pandoc + xelatex invocation for every PDF in this repo.
#   scripts/build_pdf.sh book [abnt|ieee]              -> build/ww3-lab-course.pdf
#   scripts/build_pdf.sh proposal [pt|en] [abnt|ieee]  -> build/proposal_<lang>.pdf
# OUT_DIR overrides build/. Runs inside `nix develop .` (pandoc, xelatex on PATH).
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
usage() { sed -n '2,5p' "$0" >&2; exit 2; }

target="${1:-}"; shift || true
case "$target" in
  book) lang=en; style="${1:-abnt}" ;;
  proposal) lang="${1:-pt}"; style="${2:-abnt}" ;;
  *) usage ;;
esac
case "$lang" in pt|en) ;; *) usage ;; esac
case "$style" in abnt|ieee) ;; *) usage ;; esac

out_dir="${OUT_DIR:-$root/build}"
mkdir -p "$out_dir"
csl="$root/pubs/csl/$style.csl"

if [ "$target" = book ]; then
  prep="$out_dir/book"
  # Start from an empty prep dir: a lesson renamed or removed in course/ would
  # otherwise leave its old copy behind and pandoc would see both (duplicate labels).
  rm -rf "$prep"
  python3 "$root/scripts/book_prep.py" "$root/course" "$prep"
  inputs=("$prep"/[0-9][0-9]-*.md)
  pandoc "${inputs[@]}" \
    --defaults "$root/pubs/book/defaults.yaml" \
    --template "$root/pubs/book/template.tex" \
    --resource-path "$root/course" \
    --csl "$csl" \
    --fail-if-warnings \
    -o "$out_dir/ww3-lab-course.pdf"
  echo "build_pdf: $out_dir/ww3-lab-course.pdf"
else
  src="$root/pubs/proposal/$lang"
  inputs=("$src"/[0-9][0-9]-*.md)
  [ -e "${inputs[0]}" ] || { echo "build_pdf: no $src/NN-*.md" >&2; exit 1; }
  case "$lang" in pt) plang=pt-BR ;; en) plang=en-US ;; esac
  export TEXINPUTS="$root/pubs/proposal/shared:${TEXINPUTS:-}"   # pagina.sty, portland.sty
  pandoc "${inputs[@]}" \
    --template "$root/pubs/proposal/template.tex" \
    --metadata-file "$root/pubs/proposal/meta.$lang.yaml" \
    --metadata lang="$plang" \
    --top-level-division=section --number-sections \
    --citeproc --bibliography "$root/pubs/proposal/refs.bib" --csl "$csl" \
    --pdf-engine=pdflatex \
    --fail-if-warnings \
    -o "$out_dir/proposal_$lang.pdf"
  echo "build_pdf: $out_dir/proposal_$lang.pdf"
fi
