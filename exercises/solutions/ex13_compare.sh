#!/usr/bin/env bash
# Exercise 13 solution -- run the L2 replay on ww3_ts1 and read the table.
#
# usage: ex13_compare.sh <ww3-dir> [regtest=ww3_ts1]
#
# kokkos/tests/L2_replay.sh runs a regtest twice from the same work_lab --
# once with the Fortran W3SNL1 (WW_KOKKOS_SNL1=0) and once with the Kokkos
# kernel (WW_KOKKOS_SNL1=1) -- converts both with ww3_ounf and compares the
# fields with nccmp-tol, appending the table to kokkos/PORT_STATUS.md under
# "L2 replays". This script runs it, pulls that last table back out, and says
# in one line whether every judged field passed. Without the fork's PATCH
# applied to ww3_shel both runs take the Fortran path and the table proves the
# harness, not the kernel -- which is still the first thing to establish.
#
# Prerequisites (inside `just ww3`):
#   just rt <regtest> <switch>         the work_lab this replays (ww3_ts1 ST4)
#   just kokkos-build openmp-release   nccmp-tol
#
# Not executed in the environment this was written in (no WW3 checkout);
# bash -n and shellcheck clean.
#
# SPDX-License-Identifier: MIT
set -euo pipefail

WW3DIR="${1:?usage: ex13_compare.sh <ww3-dir> [regtest]}"
TEST="${2:-ww3_ts1}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
REPLAY="$ROOT/kokkos/tests/L2_replay.sh"
STATUS="$ROOT/kokkos/PORT_STATUS.md"
OUT="$ROOT/exercises/solutions/out/ex13"
mkdir -p "$OUT"

[ -f "$REPLAY" ] || {
  echo "!! $REPLAY not found -- it arrives with the nccmp-tol/L2 tooling (kokkos/tools/nccmp-tol)"
  exit 1
}
[ -d "$WW3DIR/regtests/$TEST/work_lab" ] || { echo "!! no work_lab for $TEST -- run: just rt $TEST <switch>   (ww3_ts1 ST4)"; exit 1; }

echo "############ L2 replay of $TEST"
# L2_replay.sh exits 1 when a judged field is outside tolerance -- that is a
# verdict, and the one this exercise exists to read, not an error. Capture the
# status instead of letting pipefail abort here; 2 (I/O) and anything else
# (missing tool, crash) are real failures.
set +e
bash "$REPLAY" "$WW3DIR" "$TEST" 2>&1 | tee "$OUT/replay.log"
rc=${PIPESTATUS[0]}
set -e
case "$rc" in
  0) echo ">> replay finished: every judged field inside tolerance (exit 0)" ;;
  1) echo ">> replay finished: at least one judged field OUTSIDE tolerance (exit 1)" ;;
  *) echo "!! L2_replay.sh failed with exit $rc (2 = I/O error, else a missing tool or a crash) -- see $OUT/replay.log"
     exit 1 ;;
esac

[ -f "$STATUS" ] || { echo "!! $STATUS was not written -- see $OUT/replay.log"; exit 1; }

# The block the replay just appended. L2_replay.sh writes, under the one
# "## L2 replays" section, a "### <test> -- <date> -- PASS|FAIL" heading per
# replay, a one-row pipe table (switch, wall clocks, files) and nccmp-tol's raw
# stdout in a ``` fence. Every past replay is still there, so take only what
# follows the LAST "### " heading inside that section.
awk '
  /^## L2 replays$/ { insec = 1; next }
  insec && /^## /   { insec = 0 }
  insec && /^### /  { buf = "" }
  insec             { buf = buf $0 "\n" }
  END               { printf "%s", buf }' "$STATUS" > "$OUT/l2_table.md"

echo
echo "############ what PORT_STATUS.md now says"
cat "$OUT/l2_table.md"

# One-line verdict from nccmp-tol's rows inside the fence. Each is
#   <variable>  <n>  <dropped>  <max|d|>  <rms>  <max rel>  <verdict>
# with verdict PASS, FAIL, "FAIL (n=0)" (judged, nothing to compare -- a failure
# by the README's definition) or unlisted (reported, not judged). The match is
# on that shape, case-sensitive, so neither the "### ... -- PASS|FAIL" heading
# nor the "nccmp-tol: FAIL (3 judged, ...)" summary line is counted.
n_pass=$(grep -cE '^[[:alnum:]_]+[[:space:]]+[0-9]+[[:space:]]+[0-9]+.*[[:space:]]PASS$' "$OUT/l2_table.md" || true)
n_fail=$(grep -cE '^[[:alnum:]_]+[[:space:]]+[0-9]+[[:space:]]+[0-9]+.*[[:space:]]FAIL( \(n=0\))?$' "$OUT/l2_table.md" || true)
echo
if [ "$rc" -eq 1 ] || [ "$n_fail" -gt 0 ]; then
  echo "ex13: $n_fail judged field(s) OUTSIDE tolerance ($n_pass inside) -- read the 'max rel' column and lesson 12's tolerance discussion"
elif [ "$n_pass" -gt 0 ]; then
  echo "ex13: $n_pass judged field(s) inside tolerance, none outside -- parity holds for $TEST"
else
  echo "ex13: replay exited 0 but no per-field verdict rows were found -- read $OUT/l2_table.md by hand"
fi
