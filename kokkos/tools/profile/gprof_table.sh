#!/usr/bin/env bash
# gprof phase table for one WW3 regtest: rebuild WW3 with -pg into a separate
# build directory, run the regtest's programs up to ww3_shel, and reduce
# `gprof -b` to "routine | self % | cumulative %" for the top routines, each
# tagged with the model phase its name belongs to, plus a per-phase total.
#
# usage:
#   gprof_table.sh [--dry-run] <ww3-dir> <regtest> [switch]
#   gprof_table.sh --render <gprof-b-output>   re-render a saved `gprof -b` text
#   gprof_table.sh --pairs                     render "<self %> <symbol>" lines from stdin
#
#   <ww3-dir>   a WW3 source tree; the -pg build goes to <ww3-dir>/build-pg and
#               <ww3-dir>/build is left alone
#   <regtest>   e.g. ww3_tp1.1; runs in <ww3-dir>/regtests/<regtest>/work_pg
#   [switch]    switch file or name; defaults to the SWITCH cached in
#               <ww3-dir>/build/CMakeCache.txt (what `just rt` built with),
#               else regtests/<regtest>/input/switch_PR3_UQ
#
# environment: TOP (rows in the table, 25), BUILD_TYPE (Debug), JOBS (nproc)
#
# The -pg build passes -DCMAKE_Fortran_FLAGS=-pg: WW3's CMake honours that
# variable on top of its own per-build-type flags, and CMake also puts it on the
# Fortran link line, which is where -pg pulls in the profiling start-up. A
# separate build directory is used because the switch is baked into the
# preprocessed sources and a -pg build must never be mistaken for a timing one.
#
# Phase map (on the routine name, module prefix stripped):
#   w3srce*            source terms
#   w3pro* | w3uqck*   propagation
#   w3gath | w3scat | mpi_*   communication
#   w3io*              I/O
#   everything else    other
#
# SPDX-License-Identifier: MIT
set -euo pipefail

TOP="${TOP:-25}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
JOBS="${JOBS:-$(nproc)}"

usage() {
  sed -n '2,/^# SPDX/{/^# SPDX/!p}' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
  exit 64
}

# stdin: "<self %> <symbol>" lines, sorted by self share as gprof and perf emit
# them. stdout: the markdown tables.
phase_table() {
  awk -v top="$TOP" '
    function strip(sym,    s) {
      s = tolower(sym)
      sub(/^__/, "", s)
      sub(/_mod_/, ":", s)              # gfortran: __w3srcemd_MOD_w3srce -> w3srcemd:w3srce
      sub(/_$/, "", s)                  # external symbols carry a trailing underscore
      return s
    }
    # A pattern matches at the start of the module or of the routine: w3pro3md
    # holds w3xyp3, w3iogomd holds w3outg, while w3gath lives in w3parall.
    function phase(sym,    s) {
      s = strip(sym)
      if (s ~ /(^|:)w3srce/) return "source terms"
      if (s ~ /(^|:)w3pro/ || s ~ /(^|:)w3uqck/) return "propagation"
      if (s ~ /(^|:)w3gath/ || s ~ /(^|:)w3scat/ || s ~ /(^|:)p?mpi_/) return "communication"
      if (s ~ /(^|:)w3io/) return "I/O"
      return "other"
    }
    NF >= 2 && $1 ~ /^[0-9.]+$/ {
      n++; pct[n] = $1 + 0; sym[n] = $2
      p = phase($2); if (!(p in seen)) { seen[p] = 1; order[++np] = p }
      total[p] += $1
    }
    END {
      if (n == 0) { print "no samples" > "/dev/stderr"; exit 1 }
      print "| # | routine | self % | cumulative % | phase |"
      print "|---|---|---|---|---|"
      cum = 0
      for (i = 1; i <= n && i <= top; i++) {
        cum += pct[i]
        printf "| %d | %s | %.2f | %.2f | %s |\n", i, strip(sym[i]), pct[i], cum, phase(sym[i])
      }
      print ""
      print "| phase | self % (all routines) |"
      print "|---|---|"
      for (i = 1; i <= np; i++) printf "| %s | %.2f |\n", order[i], total[order[i]]
    }'
}

# stdin: `gprof -b` text; stdout: "<self %> <symbol>" lines from the flat profile.
gprof_pairs() {
  awk '
    /^ *% +cumulative/ { in_flat = 1; getline; next }   # two header lines
    in_flat && NF == 0 { exit }
    in_flat { print $1, $NF }'
}

case "${1:-}" in
  --pairs) phase_table; exit ;;
  --render) [ $# -eq 2 ] || usage; gprof_pairs < "$2" | phase_table; exit ;;
esac

dry_run=0
if [ "${1:-}" = "--dry-run" ]; then
  dry_run=1
  shift
fi
[ $# -ge 2 ] && [ $# -le 3 ] || usage
ww3="$(cd "$1" 2>/dev/null && pwd)" || { echo "!! no such directory: $1" >&2; exit 3; }
regtest="$2"
switch="${3:-}"
test_dir="$ww3/regtests/$regtest"
[ -d "$test_dir" ] || { echo "!! no such regtest: $test_dir" >&2; exit 3; }

if [ -z "$switch" ] && [ -f "$ww3/build/CMakeCache.txt" ]; then
  switch="$(sed -n 's/^SWITCH:[A-Z]*=//p' "$ww3/build/CMakeCache.txt" | head -n1)"
fi
if [ -z "$switch" ] && [ -f "$test_dir/input/switch_PR3_UQ" ]; then
  switch="$test_dir/input/switch_PR3_UQ"
fi
[ -n "$switch" ] || { echo "!! no switch: pass one as the third argument" >&2; exit 3; }
if [ -f "$switch" ]; then
  switch="$(cd "$(dirname "$switch")" && pwd)/$(basename "$switch")"
fi

build="$ww3/build-pg"
work="$test_dir/work_pg"
configure=(cmake -S "$ww3" -B "$build" -DSWITCH="$switch" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
           -DCMAKE_Fortran_FLAGS=-pg -DCMAKE_EXE_LINKER_FLAGS=-pg)

echo ">> gprof profile of $regtest (switch $switch, $BUILD_TYPE -pg)"
if [ "$dry_run" = 1 ]; then
  echo "-- dry run: would run"
  echo "     ${configure[*]}"
  echo "     cmake --build $build -j$JOBS"
  echo "     (in $work) ww3_grid, ww3_strt, ww3_prnc, ww3_bounc, then ww3_shel -> gmon.out"
  echo "     gprof -b ./ww3_shel gmon.out > gprof.txt; table -> $work/gprof_table.md"
  exit 0
fi

"${configure[@]}"
cmake --build "$build" -j"$JOBS"
bin="$build/bin"
[ -d "$bin" ] || bin="$build"
[ -x "$bin/ww3_shel" ] || { echo "!! $bin/ww3_shel was not built" >&2; exit 3; }

rm -rf "$work"
mkdir -p "$work"
cp "$test_dir"/input/* "$work"/ 2>/dev/null || true
cp "$bin"/ww3_* "$work"/
cd "$work"

# Each program drops its own gmon.out in the cwd; only ww3_shel's is wanted.
run() {
  local prog="$1"
  if [ -x "./$prog" ] && { [ -e "$prog.nml" ] || [ -e "$prog.inp" ]; }; then
    echo "=== $prog"
    "./$prog" > "$prog.out" 2>&1
    rm -f gmon.out
  fi
}
run ww3_grid
run ww3_strt
run ww3_prnc
run ww3_bounc
echo "=== ww3_shel (profiled)"
./ww3_shel > ww3_shel.out 2>&1
[ -f gmon.out ] || { echo "!! ww3_shel left no gmon.out: was -pg applied?" >&2; exit 3; }
gprof -b ./ww3_shel gmon.out > gprof.txt

{
  echo "# gprof: $regtest ($BUILD_TYPE -pg, switch $(basename "$switch"))"
  echo
  gprof_pairs < gprof.txt | phase_table
} | tee gprof_table.md
echo ">> table: $work/gprof_table.md  (raw: $work/gprof.txt)"
