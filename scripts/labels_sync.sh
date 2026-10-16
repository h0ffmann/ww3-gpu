#!/usr/bin/env bash
# labels_sync — reconcile h0ffmann/ww3-gpu's GitHub labels against .github/labels.yml with
# marola-devkit's scripts/issues.sh, the same tool and manifest shape every marola repo uses
# (ISSUE_LABELS.md). This repo does not carry the devkit flake, so the script fetches the devkit at
# DEVKIT_REF into a cache and runs it from the repo root, where issues.sh finds the manifest.
#
#   just labels-sync              # create and edit what differs; orphans are listed, not deleted
#   just labels-sync --dry-run    # parse the manifest, read the repo, print the gh calls only
#   just labels-sync --prune      # also delete labels the manifest lacks (refuses most of the repo)
#
# DEVKIT_REF moves with the `uses: …@` pins in .github/workflows/labels.yml and skills.yml.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

DEVKIT_REF="${DEVKIT_REF:-v0.11.1}"
devkit="${XDG_CACHE_HOME:-$HOME/.cache}/ww3-gpu/marola-devkit-$DEVKIT_REF"
if [[ ! -f "$devkit/scripts/issues.sh" ]]; then
  rm -rf "$devkit"
  git clone --quiet --depth 1 --branch "$DEVKIT_REF" https://github.com/marola-dev/marola-devkit "$devkit"
fi

global=(); sync=()
for arg in "$@"; do
  if [[ "$arg" == --dry-run ]]; then global+=(--dry-run); else sync+=("$arg"); fi
done
exec bash "$devkit/scripts/issues.sh" "${global[@]}" labels sync "${sync[@]}"
