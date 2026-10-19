#!/usr/bin/env bash
# profile_case — where WW3's time goes on one of scripts/ww3-gpu.sh's cases, measured with gprof, as
# the profile TSV that scripts/port_score.py ranks (`just port-score`).
#
#   scripts/profile_case.sh smoke|full|pdlib|gfs     (default full)
#
#   smoke  regtest ww3_ts1 with ww3_grid_ST4_T475.nml (one point: only source terms run)
#   full   kokkos/tests/fixtures/bench_small (120 x 80, 9 520 sea points: propagation too)
#   pdlib  regtest ww3_tp2.17 grid c, as matrix.base runs it (`-s PDLIB -g c`): the Shinnecock
#          Inlet unstructured mesh (3 070 nodes), PDLIB domain decomposition, implicit scheme,
#          4 h, with WW3/regtests/ww3_tp2.17/input/switch_PDLIB (PDLIB SCOTCH DIST MPI ST4 NL1)
#          and WW3's ftp data (pratico's ww3-regtest-data)
#   gfs    regtest ww3_ufs1.1 grdset_c, the 5 degree global grid of the GFS-Wave regtests (ww3_multi,
#          the point-output grid, 24 h), as matrix.base runs it (`-s MPI -m grdset_c`), with the
#          regtest's own switch WW3/regtests/ww3_ufs1.1/input/switch_MPI (DIST MPI, SEED, FLD2, no
#          LN1) and WW3's ftp data. One MPI rank. A run takes about 3 s, too few gprof samples, so
#          it runs GFS_RUNS times (default 10) and `gprof -s` sums the gmon files.
#
# Builds WW3 as written (the WW3/ submodule, no KOKKOS switch, switches/switch_lab_shrd; pdlib its
# regtest's switch into build/profile/pdlib/) in
# Release with -pg into build/profile/, runs ww3_shel on one thread, and writes
# docs/data/ww3_<case>_gprof.tsv: every routine of gprof's flat profile with its self time.
# gprof samples every 10 ms, so a routine under a few samples is noise; the shares are this case's
# on this machine, not every model's. pdlib runs ww3_shel as one MPI rank (gprof wants one
# process), so the halo exchange and every other communication cost of an operational run is absent.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
case_name="${1:-full}"
case "$case_name" in smoke|full|pdlib|gfs) ;; *) echo "profile_case: smoke, full, pdlib or gfs, not $case_name" >&2; exit 64 ;; esac

if [ -z "${PROFILE_IN_SHELL:-}" ]; then
  exec env PROFILE_IN_SHELL=1 nix develop "$root/picasso/labs/pratico#ww3" --command bash "${BASH_SOURCE[0]}" "$case_name"
fi

ww3="$root/WW3"
b="$root/build/profile"
sw=switches/switch_lab_shrd
if [ "$case_name" = pdlib ]; then
  b="$b/pdlib"
  sw=WW3/regtests/ww3_tp2.17/input/switch_PDLIB
fi
prog=ww3_shel
if [ "$case_name" = gfs ]; then
  b="$b/gfs"
  sw=WW3/regtests/ww3_ufs1.1/input/switch_MPI
  prog=ww3_multi
fi
mkdir -p "$b"
printf '%s\n' "$(cat "$root/$sw")" > "$b/switch"
[ -f "$b/ww3/CMakeCache.txt" ] || cmake -S "$ww3" -B "$b/ww3" -DSWITCH="$b/switch" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_Fortran_FLAGS=-pg > "$b/configure.log" 2>&1
cmake --build "$b/ww3" -j"${JOBS:-$(nproc)}" > "$b/build.log" 2>&1
bin="$b/ww3/bin"

if [ "$case_name" = gfs ]; then
  # What WW3/regtests/bin/run_cmake_test does for `-m grdset_c` without -N: the .inp decks, from a
  # work directory beside input/ (the decks name ../input/), ww3_grid for the model grid and the
  # point grid, ww3_prnc on each ww3_prnc_glo_5deg_*.inp in ls order (current, ice, wind), then
  # ww3_multi.
  data="$(nix build --no-link --print-out-paths "$root/picasso/labs/pratico#ww3-regtest-data")"
  t="$b/run-gfs"
  [[ "$t" == "$root/build/profile/"* ]] && rm -rf -- "$t"
  mkdir -p "$t/input" "$t/work_c"
  cp "$ww3"/regtests/ww3_ufs1.1/input/* "$data"/data_regtests/ww3_ufs1.1/input/* "$t/input/"
  chmod -R u+w "$t/input"
  d="$t/work_c"
  run() { local p="$1" deck="$2"; cp "$t/input/$deck.inp" "$d/$p.inp"; (cd "$d" && "$bin/$p" > "$deck.out" 2>&1); }
  # run_cmake_test renames each program's output to the grid's name, as ww3_multi.inp reads it.
  run ww3_grid ww3_grid_glo_5deg; mv "$d/mod_def.ww3" "$d/mod_def.glo_5deg"
  run ww3_grid ww3_grid_points; mv "$d/mod_def.ww3" "$d/mod_def.points"
  cp "$d/mod_def.glo_5deg" "$d/mod_def.ww3"
  for f in "$t"/input/ww3_prnc_glo_5deg_*.inp; do
    run ww3_prnc "$(basename "$f" .inp)"
    for v in current ice wind; do [ ! -f "$d/$v.ww3" ] || mv "$d/$v.ww3" "$d/$v.glo_5deg"; done
  done
  rm -f "$d/mod_def.ww3"
  cp "$t/input/ww3_multi_grdset_c.inp" "$d/ww3_multi.inp"
  n_runs="${GFS_RUNS:-10}"
  t0=$(date +%s.%N)
  for i in $(seq "$n_runs"); do
    (cd "$d" && OMP_NUM_THREADS=1 mpirun -np 1 "$bin/ww3_multi" > ww3_multi.out 2>&1 && mv gmon.out "gmon.$i")
    grep -q "End of program" "$d/ww3_multi.out" || { echo "profile_case: ww3_multi run $i did not finish" >&2; exit 1; }
  done
  wall="$(awk -v a="$t0" -v b="$(date +%s.%N)" 'BEGIN { printf "%.1f", b - a }')"
  note=", summed over $n_runs runs"
  (cd "$d" && gprof -s "$bin/ww3_multi" gmon.[0-9]* && mv gmon.sum gmon.out)
elif [ "$case_name" = pdlib ]; then
  # What WW3/regtests/bin/run_cmake_test does for `-g c` without -N: the .inp decks, from a work
  # directory beside input/ (the decks name ../input/), ww3_grid, ww3_bounc, then ww3_prnc on
  # each ww3_prnc_*.inp in ls order (current, level, wind); no ww3_strt deck exists.
  data="$(nix build --no-link --print-out-paths "$root/picasso/labs/pratico#ww3-regtest-data")"
  t="$b/run-pdlib"
  [[ "$t" == "$root/build/profile/"* ]] && rm -rf -- "$t"
  mkdir -p "$t/input" "$t/work_c"
  cp "$ww3"/regtests/ww3_tp2.17/input/* "$data"/data_regtests/ww3_tp2.17/input/* "$t/input/"
  chmod -R u+w "$t/input"
  d="$t/work_c"
  run() { local p="$1" deck="$2"; cp "$t/input/$deck.inp" "$d/$p.inp"; (cd "$d" && "$bin/$p" > "$deck.out" 2>&1); }
  run ww3_grid ww3_grid_c
  run ww3_bounc ww3_bounc
  for f in "$t"/input/ww3_prnc_*.inp; do run ww3_prnc "$(basename "$f" .inp)"; done
  cp "$t/input/ww3_shel.inp" "$d/"
  t0=$(date +%s)
  (cd "$d" && OMP_NUM_THREADS=1 mpirun -np 1 "$bin/ww3_shel" > ww3_shel.out 2>&1)
  wall=$(( $(date +%s) - t0 ))
else
d="$b/run-$case_name"
[[ "$d" == "$root/build/profile/"* ]] && rm -rf -- "$d"
mkdir -p "$d"
if [ "$case_name" = smoke ]; then
  cp "$ww3"/regtests/ww3_ts1/input/* "$d"/
  cp "$d/ww3_grid_ST4_T475.nml" "$d/ww3_grid.nml"
  sed -i 's#\.\./input/#./#g' "$d"/*.nml "$d"/*.inp
else
  cp "$root"/kokkos/tests/fixtures/bench_small/* "$d"/
fi
(cd "$d" && "$bin/ww3_grid" > ww3_grid.out 2>&1)
if [ -f "$d/ww3_strt.inp" ] || [ -f "$d/ww3_strt.nml" ]; then (cd "$d" && "$bin/ww3_strt" > ww3_strt.out 2>&1); fi
t0=$(date +%s)
(cd "$d" && OMP_NUM_THREADS=1 "$bin/ww3_shel" > ww3_shel.out 2>&1)
wall=$(( $(date +%s) - t0 ))
fi
grep -q "End of program" "$d/$prog.out" || { echo "profile_case: $prog did not finish" >&2; exit 1; }
(cd "$d" && gprof -b "$bin/$prog" gmon.out > gprof.txt)

out="$root/docs/data/ww3_${case_name}_gprof.tsv"
rev="$(git -C "$ww3" rev-parse --short HEAD)"
cat > "$out" <<EOF
# gprof self time per routine of $prog, written by scripts/profile_case.sh $case_name; do not edit.
# case	$case_name ($(case "$case_name" in smoke) echo "ww3_ts1, ST4 T475" ;; full) echo "bench_small, 120 x 80, 9 520 sea points, 12 h" ;;
  pdlib) echo "ww3_tp2.17 grid c, Shinnecock Inlet, unstructured, 3 070 nodes, implicit, 4 h" ;;
  gfs) echo "ww3_ufs1.1 grdset_c, glo_5deg 72 x 35, 18 x 12 spectrum, ww3_multi with the points grid, 24 h" ;; esac))
# build	Release -pg, $sw, $( [ "$case_name" = pdlib ] || [ "$case_name" = gfs ] && echo "mpirun -np 1 (no communication cost), ")1 thread, $prog ${wall} s under -pg${note:-}
# ww3_rev	$rev
# date	$(date -u +%Y-%m-%d)
# command	just profile-case $case_name
EOF
python3 "$root/scripts/port_score.py" --from-gprof "$d/gprof.txt" "$out"
echo ">> $out"
