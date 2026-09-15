#!/usr/bin/env bash
# perf phase table for one WW3 regtest: the same "routine | self % |
# cumulative % | phase" table as gprof_table.sh, from `perf record -g` on the
# ww3_shel of an already prepared work_lab (no rebuild: perf samples the
# optimised binary `just rt` built).
#
# usage: perf_table.sh [--dry-run] <ww3-dir> <regtest>
#
#   <ww3-dir>   a WW3 tree whose regtests/<regtest>/work_lab was prepared by
#               `just rt <regtest>`; the run is replayed in work_perf
#
# environment: TOP (rows in the table, 25), PERF_OPTS (extra `perf record` flags)
#
# perf is NOT part of the pratico shell (nix-config/labs/pratico): it is tied to
# the host kernel, so it has to come from the host (Debian: linux-perf). When
# `perf` is not on PATH this script says so and exits 3.
#
# SPDX-License-Identifier: MIT
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
  sed -n '2,/^# SPDX/{/^# SPDX/!p}' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
  exit 64
}

dry_run=0
if [ "${1:-}" = "--dry-run" ]; then
  dry_run=1
  shift
fi
[ $# -eq 2 ] || usage
ww3="$(cd "$1" 2>/dev/null && pwd)" || { echo "!! no such directory: $1" >&2; exit 3; }
regtest="$2"
test_dir="$ww3/regtests/$regtest"
work_lab="$test_dir/work_lab"
work="$test_dir/work_perf"

if ! command -v perf > /dev/null 2>&1; then
  cat >&2 <<'EOF'
!! perf is not on PATH. It is not in the pratico shell (nix-config/labs/pratico):
   perf is tied to the running kernel, so install it on the host (Debian/Ubuntu:
   `apt install linux-perf` or `linux-tools-$(uname -r)`) and run this script
   from a host shell, or use gprof_table.sh, which needs only gfortran.
EOF
  exit 3
fi
[ -d "$work_lab" ] && [ -x "$work_lab/ww3_shel" ] || {
  echo "!! $work_lab/ww3_shel is missing: run \`just rt $regtest\` first" >&2
  exit 3
}

# perf report --stdio, one symbol per line with --no-children and no call
# graph, looks like "    45.00%  ww3_shel  ww3_shel  [.] __w3srcemd_MOD_w3srce".
perf_pairs() {
  awk '/^ *[0-9.]+% / { p = $1; sub(/%$/, "", p); print p, $NF }'
}

echo ">> perf profile of $regtest ($(command -v perf))"
if [ "$dry_run" = 1 ]; then
  echo "-- dry run: would copy $work_lab to $work and run"
  echo "     perf record -g ${PERF_OPTS:-} -o perf.data ./ww3_shel"
  echo "     perf report -i perf.data --stdio --no-children -g none | pairs | gprof_table.sh --pairs"
  echo "   table -> $work/perf_table.md"
  exit 0
fi

rm -rf "$work"
cp -a "$work_lab" "$work"
cd "$work"
rm -f ww3.*.nc perf.data
read -r -a perf_opts <<< "${PERF_OPTS:-}"
echo "=== ww3_shel (perf record -g)"
perf record -g "${perf_opts[@]}" -o perf.data ./ww3_shel > ww3_shel.out 2> perf_record.err
perf report -i perf.data --stdio --no-children -g none 2> /dev/null > perf_report.txt

{
  echo "# perf: $regtest (build/ binaries as run by \`just rt\`)"
  echo
  perf_pairs < perf_report.txt | TOP="${TOP:-25}" "$here/gprof_table.sh" --pairs
} | tee perf_table.md
echo ">> table: $work/perf_table.md  (raw: $work/perf_report.txt)"
