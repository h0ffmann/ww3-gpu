#!/usr/bin/env bash
# L2 replay: run one WW3 regtest twice from the same prepared work directory --
# once with the Fortran W3SNL1 (WW_KOKKOS_SNL1=0), once through the Kokkos shim
# (WW_KOKKOS_SNL1=1) -- and compare the gridded ww3_ounf output field by field
# with nccmp-tol. The verdict is appended to kokkos/PORT_STATUS.md under the
# "## L2 replays" heading.
#
# usage: L2_replay.sh [--dry-run] <ww3-dir> <regtest> [switch]
#
#   <ww3-dir>   a WW3 tree whose <ww3-dir>/regtests/<regtest>/work_lab was
#               prepared by `just rt <regtest>` (this script builds nothing)
#   <regtest>   e.g. ww3_ts1
#   [switch]    label recorded in the table; defaults to the SWITCH cached in
#               <ww3-dir>/build/CMakeCache.txt
#
# environment:
#   NCCMP_TOL     path to the nccmp-tol binary
#                 (default: kokkos/build/openmp-release/tools/nccmp-tol/nccmp-tol)
#   WW_L2_LAUNCH  prefix for ww3_shel, e.g. "mpirun -np 4" for an MPI switch
#
# Without the W3SNL1 PATCH applied to the WW3 tree, ww3_shel ignores
# WW_KOKKOS_SNL1 and both runs are bit-identical: the replay then proves the
# harness, not the kernel.
#
# exit: nccmp-tol's status -- 0 all judged fields pass, 1 a field fails,
#       2 an I/O error; 64 on a usage error, 3 when a prerequisite is missing.
#
# SPDX-License-Identifier: MIT
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
tolerances="$root/kokkos/tools/nccmp-tol/tolerances.txt"
status_md="$root/kokkos/PORT_STATUS.md"
nccmp="${NCCMP_TOL:-$root/kokkos/build/openmp-release/tools/nccmp-tol/nccmp-tol}"

usage() {
  # The header comment above, up to the SPDX line, is the help text.
  sed -n '2,/^# SPDX/{/^# SPDX/!p}' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
  exit 64
}

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
work_lab="$test_dir/work_lab"
[ -d "$work_lab" ] || {
  echo "!! $work_lab is missing: run \`just rt $regtest\` first" >&2
  exit 3
}
for prog in ww3_shel ww3_ounf; do
  [ -x "$work_lab/$prog" ] || {
    echo "!! $work_lab/$prog is missing or not executable (is NC4 in the switch?)" >&2
    exit 3
  }
done
if [ -z "$switch" ] && [ -f "$ww3/build/CMakeCache.txt" ]; then
  switch="$(sed -n 's/^SWITCH:[A-Z]*=//p' "$ww3/build/CMakeCache.txt" | head -n1)"
  switch="$(basename "${switch:-unknown}")"
fi
switch="${switch:-unknown}"

# Split WW_L2_LAUNCH on whitespace into a command prefix; empty means "run directly".
read -r -a launch <<< "${WW_L2_LAUNCH:-}"

# Run ww3_shel then ww3_ounf in <work>, with WW_KOKKOS_SNL1 set. Prints the wall
# time of ww3_shel in seconds.
run_pair() {
  local work="$1" snl1="$2" t0 t1
  # ww3_ounf's own outputs from the prepared run; the forcing inputs (*.nc from
  # ww3_prnc's point of view) are not named ww3.*.nc and stay.
  rm -f "$work"/ww3.*.nc "$work"/gmon.out
  t0="$(date +%s)"
  (cd "$work" && WW_KOKKOS_SNL1="$snl1" "${launch[@]}" ./ww3_shel > ww3_shel.out 2>&1)
  t1="$(date +%s)"
  (cd "$work" && ./ww3_ounf > ww3_ounf.out 2>&1)
  echo $((t1 - t0))
}

# Insert the block on stdin under "## L2 replays" in PORT_STATUS.md: at the end
# of that section (before the next "## " heading), or at the end of the file
# after creating the heading. Nothing else in the file is touched.
append_status() {
  local block tmp
  block="$(cat)"
  if [ ! -f "$status_md" ]; then
    printf '# Port status\n\n## L2 replays\n' > "$status_md"
  elif ! grep -q '^## L2 replays$' "$status_md"; then
    printf '\n## L2 replays\n' >> "$status_md"
  fi
  tmp="$(mktemp)"
  # The block travels through the environment: `awk -v` would eat backslashes.
  L2_BLOCK="$block" awk '
    function emit() { if (!blank) print ""; print ENVIRON["L2_BLOCK"]; done = 1 }
    /^## L2 replays$/ { in_section = 1 }
    in_section && !done && /^## / && !/^## L2 replays$/ { emit(); print "" }
    { print; blank = (NF == 0) }
    END { if (!done) emit() }
  ' "$status_md" > "$tmp"
  # Write through the existing file (mv would give it mktemp's 0600 mode).
  cat "$tmp" > "$status_md"
  rm -f "$tmp"
}

echo ">> L2 replay of $regtest (switch $switch): WW_KOKKOS_SNL1=0 vs 1"
echo ">> work_lab: $work_lab"
echo ">> nccmp-tol: $nccmp"
if [ "$dry_run" = 1 ]; then
  echo "-- dry run: would copy work_lab to work_a and work_b, run"
  echo "     WW_KOKKOS_SNL1=0 ${launch[*]:-} ./ww3_shel && ./ww3_ounf   (work_a)"
  echo "     WW_KOKKOS_SNL1=1 ${launch[*]:-} ./ww3_shel && ./ww3_ounf   (work_b)"
  echo "   then \`$nccmp work_a/ww3.<date>.nc work_b/ww3.<date>.nc $tolerances\`"
  echo "   and append the table to $status_md under '## L2 replays'"
  exit 0
fi
[ -x "$nccmp" ] || {
  echo "!! $nccmp is missing: run \`just kokkos-build openmp-release\` (or set NCCMP_TOL)" >&2
  exit 3
}

work_a="$test_dir/work_a"
work_b="$test_dir/work_b"
rm -rf "$work_a" "$work_b"
cp -a "$work_lab" "$work_a"
cp -a "$work_lab" "$work_b"

echo ">> [a] Fortran W3SNL1 ..."
wall_a="$(run_pair "$work_a" 0)"
echo ">> [b] Kokkos W3SNL1 ..."
wall_b="$(run_pair "$work_b" 1)"
echo ">> ww3_shel wall time: a=${wall_a}s b=${wall_b}s"

# Every gridded output file, compared by name (a glob expands sorted).
outputs=()
for f in "$work_a"/ww3.*.nc; do
  [ -e "$f" ] && outputs+=("$(basename "$f")")
done
[ "${#outputs[@]}" -gt 0 ] || {
  echo "!! ww3_ounf wrote no ww3.*.nc in $work_a (see ww3_ounf.out)" >&2
  exit 2
}

rc=0
report=""
for nc in "${outputs[@]}"; do
  [ -f "$work_b/$nc" ] || { echo "!! $work_b/$nc is missing" >&2; rc=2; continue; }
  echo ">> nccmp-tol $nc"
  set +e
  table="$("$nccmp" "$work_a/$nc" "$work_b/$nc" "$tolerances")"
  this_rc=$?
  set -e
  echo "$table"
  [ "$this_rc" -gt "$rc" ] && rc=$this_rc
  report+="$nc"$'\n'"$table"$'\n\n'
done

case $rc in
  0) verdict="PASS" ;;
  1) verdict="FAIL" ;;
  *) verdict="ERROR" ;;
esac

append_status <<EOF
### $regtest -- $(date -u '+%Y-%m-%d %H:%M UTC') -- $verdict

| switch | ww3_shel wall (Fortran) | ww3_shel wall (Kokkos) | files |
|---|---|---|---|
| $switch | ${wall_a}s | ${wall_b}s | ${outputs[*]} |

\`\`\`
${report%$'\n\n'}
\`\`\`
EOF

echo ">> $verdict -- table appended to $status_md"
exit "$rc"
