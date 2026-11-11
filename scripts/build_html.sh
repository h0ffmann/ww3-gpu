#!/usr/bin/env bash
# build_html — the static pages GitHub Pages still serves.
#   scripts/build_html.sh   -> build/site/{andamento/,adrs/,relatorio-gta/} and the redirects to
#                              wave.piabrota.art at /, /pt-BR/, /lineage/ and /pt-BR/lineage/
# The front page itself is built and served by piabrota/nix-os (ww3-site/, ADR-0024).
# OUT_DIR overrides build/. Needs pandoc (for the GTA report); no TeX.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

out_dir="${OUT_DIR:-$root/build}"
root_site="$out_dir/site"
rm -rf "$root_site/andamento" "$root_site/adrs"
mkdir -p "$root_site"
# The project progress page (a hand-made snapshot) at /andamento/.
cp -r "$root/pubs/site/andamento" "$root_site/andamento"
# The ADR list at /adrs/, filled from docs/ADRs.md on every build so it cannot go stale.
mkdir -p "$root_site/adrs"
python3 "$root/scripts/adrs.py" --site "$root/pubs/site/adrs/index.html" "$root_site/adrs/index.html"
# The easy pt-BR report for the GTA (DEL/UFRJ) at /relatorio-gta/; its PDF is committed in pdf/.
OUT_DIR="$out_dir" bash "$root/scripts/build_relatorio.sh"
# The front page and the lineage moved to wave.piabrota.art (piabrota/nix-os, ww3-site/; ADR-0024):
# their old addresses send the reader there.
for page in "" pt-BR/ lineage/ pt-BR/lineage/; do
  mkdir -p "$root_site/$page"
  url="https://wave.piabrota.art/$page"
  printf '<!doctype html>\n<meta charset="utf-8">\n<title>WW3 GPU Lab</title>\n<link rel="canonical" href="%s">\n<meta http-equiv="refresh" content="0; url=%s">\n<p>The site moved to <a href="%s">%s</a>.</p>\n' \
    "$url" "$url" "$url" "$url" >"$root_site/${page}index.html"
done
touch "$root_site/.nojekyll" # serve the files as built; Pages would otherwise run Jekyll over them
echo "build_html: $root_site"
