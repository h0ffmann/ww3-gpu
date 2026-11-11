#!/usr/bin/env bash
# Clone and build SWAN (TU Delft) alongside your WW3 setup.
#
# usage: 04_get_swan.sh [dest]      (default: $HOME/src/swan)
#
# SWAN source lives in three places:
#   git      https://gitlab.tudelft.nl/citg/wavemodels/swan
#   tarball  https://swanmodel.sourceforge.io/download/download.htm
#   files    https://sourceforge.net/projects/swanmodel/files/swan/
# Use git. The GitHub mirrors you will find are stale snapshots.
set -euo pipefail

DEST="${1:-$HOME/src/swan}"
JOBS="${JOBS:-$(nproc)}"

if [ -d "$DEST/.git" ]; then
  echo ">> $DEST already a git repo; fetching."
  git -C "$DEST" fetch --all --tags
else
  echo ">> Cloning SWAN from TU Delft GitLab into $DEST"
  mkdir -p "$(dirname "$DEST")"
  git clone https://gitlab.tudelft.nl/citg/wavemodels/swan.git "$DEST"
fi

cd "$DEST"

# The implementation manual recommends CMake 3.12+ with Ninja, which is
# faster than GNU make. Fall back to make if ninja is absent.
GEN=""
command -v ninja >/dev/null && GEN="-G Ninja"

echo
echo ">> Configuring (CMake${GEN:+ + Ninja})"
cmake $GEN -B build -S .

echo ">> Building"
cmake --build build -j "$JOBS"

echo
echo ">> Binaries:"
find build -maxdepth 3 -type f -perm -u+x -name 'swan*' 2>/dev/null || ls build

cat <<'NOTE'

Notes
-----
* Compile-time options in SWAN are specially-formatted COMMENTS inside the
  .ftn sources, not a separate switch file like WW3. Examples:
      !/impi   MPI code in swmod1.ftn
      !ADC     ADCIRC coupling hooks, spread across several files
  Same consequence as WW3: change what is enabled, rebuild from clean.

* The older workflow still works if CMake gives you trouble:
      make config
      make ser      # or: make omp   /   make mpi

* Docs:
      user manual + implementation manual
        https://swanmodel.sourceforge.io/download/download.htm
      implementation manual (build details) direct:
        https://swanmodel.sourceforge.io/download/zip/swanimp.pdf
      release notes:
        https://swanmodel.sourceforge.io/modifications/modifications.htm

* SWASH (non-hydrostatic, phase-resolving, same group):
      https://gitlab.tudelft.nl/citg/wavemodels/swash

NOTE
