#!/usr/bin/env bash
# Build and check the arXiv source bundle of pubs/preprint (the English edition) from the
# committed wave-models-on-gpus.tex, which `just preprint` writes. arXiv has no submission API
# open to individual authors (SWORD is for approved proxies, info.arxiv.org/help/submit_sword.html
# and third_party_submission.html, read 2026-11-11), so this stops at a tarball the author
# uploads at arxiv.org/submit; nothing here talks to arXiv.
#
#   preprint_arxiv.sh [--out DIR] [--no-compile]   writes DIR/wave-models-on-gpus-arxiv.tar.gz
#   preprint_arxiv.sh --self-test                  checks the inputs and the 00README only
#
# The bundle: the .tex with the revision written in (arXiv drops dot files, so .current-revision
# would not reach it), the figures it includes, DejaVuSans.ttf (the ⚠ glyph's font, loaded by file
# name in metadata.md), and 00README.json asking for xelatex, the engine the PDF is built with.
# The check unpacks the tarball into an empty directory and runs xelatex twice there.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
here="$root/pubs/preprint"
name=wave-models-on-gpus
out="$root/build/arxiv"
compile=1
selftest=0
while [ $# -gt 0 ]; do
  case "$1" in
    --out) out="$2"; shift 2 ;;
    --no-compile) compile=0; shift ;;
    --self-test) selftest=1; shift ;;
    *) echo "preprint_arxiv: unknown argument $1" >&2; exit 64 ;;
  esac
done

die() { echo "preprint_arxiv: $*" >&2; exit 1; }

readme() {  # 00README.json, arXiv's spec_version 1 (info.arxiv.org/help/00README.html, read 2026-11-11)
  cat <<EOF
{
  "spec_version": 1,
  "process": { "compiler": "xelatex" },
  "sources": [ { "filename": "$name.tex", "usage": "toplevel" } ],
  "texlive_version": 2023
}
EOF
}

figures() {  # the files \includegraphics names, relative to pubs/preprint
  grep -o '\\includegraphics\[[^]]*\]{[^}]*}' "$here/$name.tex" | sed 's/.*]{\(.*\)}$/\1/'
}

[ -f "$here/$name.tex" ] || die "missing $name.tex; run just preprint"
grep -q 'IfFileExists{.current-revision}' "$here/$name.tex" || die "$name.tex no longer reads .current-revision; update the revision step"
figs="$(figures)"
[ -n "$figs" ] || die "no \\includegraphics in $name.tex"
for f in $figs; do [ -f "$here/$f" ] || die "missing figure $f"; done
if [ "$selftest" = 1 ]; then
  echo "preprint_arxiv: self-test ok ($(echo "$figs" | wc -l) figure(s))" >&2
  exit 0
fi

font="$(kpsewhich DejaVuSans.ttf 2> /dev/null || true)"
[ -n "$font" ] || font="$(fc-match -f '%{file}' 'DejaVu Sans:style=Book' 2> /dev/null || true)"
case "$font" in */DejaVuSans.ttf) ;; *) die "DejaVuSans.ttf not found (TeX Live's dejavu package or fonts-dejavu-core)" ;; esac

stage="$(mktemp -d)"; chmod 755 "$stage"
check="$(mktemp -d)"
trap 'rm -rf "$stage" "$check"' EXIT
# PREPRINT_REV and PREPRINT_MTIME let CI run this in a TeX Live container that has no git.
rev="${PREPRINT_REV:-$(git -C "$root" rev-parse --short HEAD)+$(git -C "$root" rev-list --count HEAD)}"
sed "s|\\\\IfFileExists{.current-revision}{\\\\input{.current-revision}}{unversioned}|$rev|" \
  "$here/$name.tex" > "$stage/$name.tex"
for f in $figs; do mkdir -p "$stage/$(dirname "$f")"; cp "$here/$f" "$stage/$f"; done
cp "$font" "$stage/DejaVuSans.ttf"
readme > "$stage/00README.json"

mkdir -p "$out"
tarball="$out/$name-arxiv.tar.gz"
# Reproducible: sorted names, the commit's time, no owner.
mtime="@${PREPRINT_MTIME:-$(git -C "$root" log -1 --format=%ct)}"
tar --sort=name --mtime="$mtime" --owner=0 --group=0 --numeric-owner -C "$stage" -cf - . \
  | gzip -n > "$tarball"
size=$(wc -c < "$tarball")
[ "$size" -lt 50000000 ] || die "bundle is $size bytes, over arXiv's 50 MB"

if [ "$compile" = 1 ]; then
  command -v xelatex > /dev/null || die "xelatex not found; enter picasso's labs/paper shell or pass --no-compile"
  tar -xzf "$tarball" -C "$check"
  (cd "$check" && for _ in 1 2; do
    xelatex -interaction=nonstopmode -halt-on-error "$name.tex" > /dev/null 2>&1 \
      || { tail -n 30 "$name.log" >&2; exit 1; }
  done)
  ! grep -q 'Missing character' "$check/$name.log" || die "missing glyphs: $(grep 'Missing character' "$check/$name.log" | sort -u | head -3)"
  ! grep -q 'undefined references' "$check/$name.log" || die "undefined references in the bundle build"
  pages=$(pdfinfo "$check/$name.pdf" 2> /dev/null | awk '/^Pages/ {print $2}' || true)
  cp "$check/$name.pdf" "$out/$name-arxiv.pdf"
  echo "preprint_arxiv: $tarball ($size bytes, rev $rev), compiled from a clean directory: ${pages:-?} pages" >&2
else
  echo "preprint_arxiv: $tarball ($size bytes, rev $rev), not compiled" >&2
fi
