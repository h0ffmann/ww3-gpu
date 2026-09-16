#!/usr/bin/env bash
# build_docx — the course book as a Word document, for readers who want to edit or comment it
# rather than read a PDF.  scripts/build_docx.sh  ->  build/ww3-lab-course.docx
# OUT_DIR overrides build/. Runs inside `nix develop .` or the Nix sandbox (nix build .#book-docx).
#
# Not the PDF path: pandoc's docx writer ignores pubs/book/template.tex and the LaTeX-only filter,
# so this reads the same prepared chapters and sets the metadata itself. Styling would come from a
# --reference-doc; none is committed, so Word's defaults apply.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out_dir="${OUT_DIR:-$root/build}"
prep="$out_dir/book-docx"
mkdir -p "$out_dir"
rm -rf "$prep"

python3 "$root/scripts/book_prep.py" "$root/course" "$prep"
inputs=("$prep"/[0-9][0-9]-*.md) # glob expansion is sorted
pandoc "${inputs[@]}" \
  --from gfm+tex_math_dollars+footnotes+definition_lists+attributes \
  --to docx \
  --toc --toc-depth=2 --number-sections \
  --resource-path "$root/course" \
  --metadata title="WW Lab" \
  --metadata subtitle="A self-paced course on WW3, NOAA's third-generation spectral wave model" \
  --metadata author="Matheus Hoffmann" \
  --metadata lang=en-US \
  -o "$out_dir/ww3-lab-course.docx"
echo "build_docx: $out_dir/ww3-lab-course.docx"
