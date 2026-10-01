#!/usr/bin/env bash
# Scaling benchmark for the REAL WW3 on your CPU.
#
# This is the only benchmark in this directory that measures WW3 itself.
# There is no GPU variant because WW3 has no GPU code path -- see
# kernel_bench.f90 for the closest honest proxy.
#
# usage: bench_ww3_cpu.sh <ww3-mpi-build-dir> [case-dir] [max-ranks]
#
# Requires a build made with switches/switch_lab_mpi (DIST MPI).
set -euo pipefail

BIN="${1:?usage: bench_ww3_cpu.sh <ww3-build-dir> [case-dir] [max-ranks]}"
CASE="${2:-case_medium}"
MAXR="${3:-$(nproc)}"
OUT="results_ww3_cpu.csv"

[ -d "$BIN/bin" ] && BIN="$BIN/bin"
[ -x "$BIN/ww3_shel" ] || {
  echo "!! no ww3_shel in $BIN"; exit 1; }

[ -d "$CASE" ] || { echo "!! no case dir $CASE -- run: just bench-case --size medium -o bench/$CASE"; exit 1; }

echo "ranks,wall_s,speedup,efficiency,notes" > "$OUT"

run_one () {
  local n="$1" launcher="$2"
  local work="run_np${n}"
  rm -rf "$work"; mkdir -p "$work"
  cp "$CASE"/* "$work"/
  cp "$BIN"/ww3_grid "$BIN"/ww3_shel "$work"/ 2>/dev/null || true
  (
    cd "$work"
    ./ww3_grid > ww3_grid.out 2>&1 || { echo "!! ww3_grid failed in $work"; exit 1; }
    # time ONLY ww3_shel -- ww3_grid is serial setup and would dilute the scaling
    local t0 t1
    t0=$(date +%s.%N)
    $launcher ./ww3_shel > ww3_shel.out 2>&1
    t1=$(date +%s.%N)
    echo "$t1 - $t0" | bc
  )
}

echo ">> benchmarking $CASE, up to $MAXR ranks"
echo ">> (ww3_grid is excluded from the timing; only ww3_shel is measured)"
echo

BASE=""
N=1
while [ "$N" -le "$MAXR" ]; do
  L=""; [ "$N" -gt 1 ] && L="mpirun --oversubscribe -np $N"
  T=$(run_one "$N" "$L")
  [ -z "$BASE" ] && BASE="$T"
  SPD=$(echo "scale=3; $BASE / $T" | bc)
  EFF=$(echo "scale=3; $SPD / $N" | bc)
  printf '%4d ranks : %8.2f s   speedup %6.2fx   efficiency %5.1f%%\n' \
    "$N" "$T" "$SPD" "$(echo "$EFF * 100" | bc)"
  echo "$N,$T,$SPD,$EFF," >> "$OUT"
  N=$((N * 2))
done

cat <<'NOTE'

Wrote results_ww3_cpu.csv

Reading it
----------
* Efficiency falling below ~60% is where adding ranks stops being worth it.
  WW3's "shuffle" decomposition load-balances the source-term timestepping
  very well but communicates more than a conventional domain decomposition.
  (Replacing it is one of the stated drivers for WW4 -- see course/10.)

* ON AN INTEL i9 THIS NUMBER IS MISLEADING BY DEFAULT. P-cores and E-cores
  have very different throughput, and an MPI job runs at the pace of its
  slowest rank. Rerun pinned to P-cores only and compare:

      mpirun -np 8 --bind-to core --cpu-set 0-15 ./ww3_shel

  (check your own topology with `lscpu -e` first; on most i9s the P-core
  hyperthreads are the low-numbered CPUs). It is common for 8 P-cores to
  beat 24 mixed cores on this kind of workload.

* Hyperthreading usually does NOT help a memory-bound spectral model.
  Test 1 rank per physical core against 1 per logical core.

* If the curve is flat from rank 1, your case is too small -- the
  decomposition overhead dominates. Use a larger --size.
NOTE
