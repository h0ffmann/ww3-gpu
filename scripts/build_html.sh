#!/usr/bin/env bash
# build_html — the static pages GitHub Pages serves next to the Gleam front page.
#   scripts/build_html.sh   -> build/site/{andamento/,adrs/,relatorio-gta/}
# The site's front page, build/site/index.html, is the Gleam landing (site/, `just site`), not this.
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
touch "$root_site/.nojekyll" # serve the files as built; Pages would otherwise run Jekyll over them
echo "build_html: $root_site"
