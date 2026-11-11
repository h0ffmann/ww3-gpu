#!/usr/bin/env bash
# stage — write a case directory for `just run --case <dir> --wind ifs` (README.md beside this
# file), from inputs the repository already holds. Nothing of WW3 is copied into the repository:
# the triangular case is read from the WW3-metocean/ submodule when this runs.
#
#   examples/03-fresh-forcing/stage.sh triangular|regular <dir> [-n]
#
#   triangular  WW3's regtest ww3_tp2.6 (LIMON, 1 778 nodes of a Gmsh mesh, GRID%TYPE = 'UNST'),
#               one hour, with the regtest's homogeneous wind replaced by INPUT%FORCING%WINDS = 'T'
#   regular     example 02's grid (southern Brazil, 81 x 81 at 0.1 degree), three hours, without its
#               boundary points, on a FLAT 100 m TEST BATHYMETRY: it shows that the wind reaches
#               the model, and its waves mean nothing. For real depths, run example 02's
#               make_bathy and copy its bathy.inp and mask.inp over the two files written here.
#   -n          list the files it would write, write nothing
#
# <dir> must not exist or must be empty. Dates are the inputs' own (2010, 2024): --wind ifs moves
# them to the IFS cycle.
#
# SPDX-License-Identifier: MIT
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
die() { echo "stage: $*" >&2; exit 2; }

kind="${1:-}" dir="${2:-}" dry="${3:-}"
case "$kind" in
    -h|--help|"") sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    triangular|regular) ;;
    *) die "the case is triangular or regular, got '$kind'" ;;
esac
[ -n "$dir" ] || die "name the directory to write"
[ -z "$dry" ] || [ "$dry" = -n ] || die "unknown option '$dry'"
if [ -e "$dir" ] && [ -n "$(ls -A "$dir" 2>/dev/null)" ]; then die "$dir is not empty"; fi

if [ "$kind" = triangular ]; then
    src="$root/WW3-metocean/regtests/ww3_tp2.6/input"
    files=(limon_ll.msh namelists_LIMON.nml points.list ww3_grid.nml ww3_shel.nml ww3_ounf.nml ww3_strt.inp)
    [ -f "$src/limon_ll.msh" ] || die "WW3-metocean/ is empty: git submodule update --init WW3-metocean"
else
    src="$root/examples/02-regional-real-forcing"
    files=(namelists.nml points.list ww3_grid.nml ww3_shel.nml ww3_ounf.nml ww3_ounp.nml ww3_prnc_wind.nml)
fi
if [ "$dry" = -n ]; then
    printf '%s\n' "${files[@]/#/$dir/}"
    [ "$kind" = triangular ] || printf '%s\n' "$dir/bathy.inp" "$dir/mask.inp"
    exit 0
fi

mkdir -p "$dir"
for f in "${files[@]}"; do cp "$src/$f" "$dir/$f"; done
cd "$dir"
if [ "$kind" = triangular ]; then
    # The regtest reads its files from ../input; a case directory is flat.
    sed -i -e "s#'\./\.\./input/#'./#" -e "s#'\.\./input/#'./#" ww3_grid.nml ww3_shel.nml
    sed -i -e "s/^\( *DOMAIN%STOP *= *\).*/\1'20100801 010000'/" \
           -e "s/^\( *INPUT%FORCING%WINDS *= *\)'H'/\1'T'/" \
           -e "s/^\( *DATE%FIELD *= *\).*/\1'20100801 000000' '600' '20100801 010000'/" \
           -e "s/^\( *DATE%POINT *= *\).*/\1'20100801 000000' '600' '20100801 010000'/" \
           -e '/^ *DATE%TRACK/d' -e '/^ *DATE%RESTART/d' \
           -e '/^&HOMOG_COUNT_NML/,/^\//d' -e '/^&HOMOG_INPUT_NML/,/^\//d' ww3_shel.nml
    sed -i -e "s/^\( *FIELD%TIMESTRIDE *= *\).*/\1'600'/" ww3_ounf.nml
    grep -q "INPUT%FORCING%WINDS *= *'T'" ww3_shel.nml || die "ww3_tp2.6's ww3_shel.nml is not the one this script knows"
else
    sed -i -e '/^&INBND_COUNT_NML/,/^\//d' -e '/^&INBND_POINT_NML/,/^\//d' ww3_grid.nml
    sed -i -e "s/'20240708 000000'/'20240701 030000'/g" ww3_shel.nml
    sed -i -e "s/^\( *\(FIELD\|POINT\)%TIMECOUNT *= *\).*/\1'4'/" ww3_ounf.nml ww3_ounp.nml
    for ((j = 0; j < 81; j++)); do
        printf -- '-100.0 %.0s' {1..81} | sed 's/ $//'; echo
    done > bathy.inp
    for ((j = 0; j < 81; j++)); do
        printf -- '1 %.0s' {1..81} | sed 's/ $//'; echo
    done > mask.inp
    echo "stage: bathy.inp is a flat 100 m test bathymetry, not the sea floor" >&2
fi
echo "stage: wrote $dir; run it with: just run --ww3 --case $dir --wind ifs" >&2
