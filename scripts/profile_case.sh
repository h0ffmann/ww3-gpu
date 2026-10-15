#!/usr/bin/env bash
# profile_case — where WW3's time goes on one of scripts/ww3-gpu.sh's cases, measured with gprof, as
# the profile TSV that scripts/port_score.py ranks (`just port-score`).
#
#   scripts/profile_case.sh smoke|full     (default full)
#
#   smoke  regtest ww3_ts1 with ww3_grid_ST4_T475.nml (one point: only source terms run)
#   full   kokkos/tests/fixtures/bench_small (120 x 80, 9 520 sea points: propagation too)
#
# Builds WW3 as written (the WW3/ submodule, no KOKKOS switch, switches/switch_lab_shrd) in
# Release with -pg into build/profile/, runs ww3_shel on one thread, and writes
# docs/data/ww3_<case>_gprof.tsv: every routine of gprof's flat profile with its self time.
# gprof samples every 10 ms, so a routine under a few samples is noise; the shares are this case's
# on this machine, not every model's.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
case_name="${1:-full}"
case "$case_name" in smoke|full) ;; *) echo "profile_case: smoke or full, not $case_name" >&2; exit 64 ;; esac

if [ -z "${PROFILE_IN_SHELL:-}" ]; then
  exec env PROFILE_IN_SHELL=1 nix develop "$root/nix-config/labs/pratico#ww3" --command bash "${BASH_SOURCE[0]}" "$case_name"
fi

ww3="$root/WW3"
b="$root/build/profile"
mkdir -p "$b"
printf '%s\n' "$(cat "$root/switches/switch_lab_shrd")" > "$b/switch"
[ -f "$b/ww3/CMakeCache.txt" ] || cmake -S "$ww3" -B "$b/ww3" -DSWITCH="$b/switch" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_Fortran_FLAGS=-pg > "$b/configure.log" 2>&1
cmake --build "$b/ww3" -j"${JOBS:-$(nproc)}" > "$b/build.log" 2>&1
bin="$b/ww3/bin"

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
grep -q "End of program" "$d/ww3_shel.out" || { echo "profile_case: ww3_shel did not finish" >&2; exit 1; }
(cd "$d" && gprof -b "$bin/ww3_shel" gmon.out > gprof.txt)

out="$root/docs/data/ww3_${case_name}_gprof.tsv"
rev="$(git -C "$ww3" rev-parse --short HEAD)"
cat > "$out" <<EOF
# gprof self time per routine of ww3_shel, written by scripts/profile_case.sh $case_name; do not edit.
# case	$case_name ($( [ "$case_name" = smoke ] && echo "ww3_ts1, ST4 T475" || echo "bench_small, 120 x 80, 9 520 sea points, 12 h"))
# build	Release -pg, switches/switch_lab_shrd, 1 thread, ww3_shel ${wall} s under -pg
# ww3_rev	$rev
# date	$(date -u +%Y-%m-%d)
# command	just profile-case $case_name
EOF
python3 "$root/scripts/port_score.py" --from-gprof "$d/gprof.txt" "$out"
echo ">> $out"
