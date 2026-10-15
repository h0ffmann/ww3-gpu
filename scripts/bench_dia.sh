#!/usr/bin/env bash
# bench_dia — the DIA (W3SNL1) on the CPU, Serial and OpenMP: WW3's own Fortran, the Kokkos port
# (v2) and ww3-dia-gpu's kernel (v1), kernel alone and whole model. CPU only; GPU columns come
# later and are the owner's to run. AGENTS.md, "No timing without parity": run it on a commit
# whose regtest matrix passed, and name that run in BENCH_REGTEST.
#
#   scripts/bench_dia.sh                 everything, writes bench/results/dia-cpu-*.json
#   scripts/bench_dia.sh --kernel-only   the kernel benchmark only (minutes)
#   scripts/bench_dia.sh --self-test
#
#   BENCH_REGTEST="passed: <command>"    required to write a record (results.py refuses without)
#   BENCH_THREADS="1 32"  BENCH_RUNS=3  BENCH_POINTS=10000  BENCH_CALLS=20
#
# Kernel (bench/dia/bench_dia.F90, one process per run, median of BENCH_RUNS): the grids of
# ww3_ts1 (ww3_grid_ST4_T475.nml) and bench_small, BENCH_POINTS synthetic spectra, 3 warm-up and
# BENCH_CALLS timed calls; columns fortran (W3SNL1 per point, !$OMP over points), v2 batch (one
# shim call for all points), v2 per point (as W3SRCE calls it, in a critical section), v1.
# Model (ww3_shel of one executable with the OMPG and KOKKOS switches, WW_KOKKOS_SNL1=0 and 1,
# median of BENCH_RUNS): ww3_ts1 and bench_small, as scripts/ww3-gpu.sh prepares them.
# Builds go to build/bench-dia/; each run's output to build/bench-dia/runs/.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
pratico="$root/picasso/labs/pratico"
threads="${BENCH_THREADS:-1 32}" runs="${BENCH_RUNS:-3}" points="${BENCH_POINTS:-10000}"
calls="${BENCH_CALLS:-20}" kernel_only=""

median() { sort -g | awk '{ v[NR] = $1 } END { if (NR == 0) exit 1; print v[int((NR + 1) / 2)] }'; }

case "${1:-}" in
  --self-test)
    [ "$(printf '3\n1\n2\n' | median)" = 2 ] || { echo "bench_dia: median" >&2; exit 1; }
    [ "$(printf '5\n' | median)" = 5 ] || { echo "bench_dia: median of one" >&2; exit 1; }
    bash -n "${BASH_SOURCE[0]}"
    echo "bench_dia self-test: ok"; exit 0 ;;
  --kernel-only) kernel_only=1 ;;
  "") ;;
  *) sed -n '2,/^set -euo/{/^set -euo/!p}' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 64 ;;
esac

if [ -z "${BENCH_IN_SHELL:-}" ]; then
  exec env BENCH_IN_SHELL=1 nix develop "$pratico#ww3" --command bash "${BASH_SOURCE[0]}" "$@"
fi

out="$root/build/bench-dia"
jobs="$(nproc)"
mkdir -p "$out/runs"

# --- builds: the port package, WW3 with OMPG and KOKKOS, the driver ------------------------------
kbuild="$root/kokkos/build/openmp-release" kprefix="$root/kokkos/build/openmp-release/install"
[ -f "$kbuild/CMakeCache.txt" ] || cmake -S "$root/kokkos" --preset openmp-release >/dev/null
cmake --build "$kbuild" -j"$jobs" >/dev/null
cmake --install "$kbuild" --prefix "$kprefix" >/dev/null

wb="$out/ww3"
switch_now="$(cat "$root/switches/switch_lab_shrd") OMPG KOKKOS"
toolchain="$(readlink -f "$(command -v gfortran)") $switch_now"
if [ "$(cat "$out/ww3.toolchain" 2>/dev/null)" != "$toolchain" ]; then
  [[ "$wb" == "$root/build/"* ]] && rm -rf -- "$wb" "$out/driver"
  printf '%s\n' "$toolchain" > "$out/ww3.toolchain"
fi
printf '%s\n' "$switch_now" > "$out/switch"
[ -f "$wb/CMakeCache.txt" ] || cmake -S "$root/WW3" -B "$wb" -DSWITCH="$out/switch" -DCMAKE_BUILD_TYPE=Release \
  -DWW_KOKKOS=ON -DCMAKE_PREFIX_PATH="$kprefix;${CMAKE_PREFIX_PATH:-}" > "$out/ww3.configure.log" 2>&1 \
  || { tail -30 "$out/ww3.configure.log"; exit 1; }
cmake --build "$wb" -j"$jobs" > "$out/ww3.build.log" 2>&1 || { grep -E -A8 "Error|error:" "$out/ww3.build.log" | head -40; exit 1; }
[ -f "$out/driver/CMakeCache.txt" ] || cmake -S "$root/bench/dia" -B "$out/driver" -DWW3_BUILD="$wb" \
  -DCMAKE_PREFIX_PATH="$kprefix;${CMAKE_PREFIX_PATH:-}" > "$out/driver.configure.log" 2>&1 \
  || { tail -30 "$out/driver.configure.log"; exit 1; }
cmake --build "$out/driver" -j"$jobs" > "$out/driver.build.log" 2>&1 || { tail -40 "$out/driver.build.log"; exit 1; }
bin="$wb/bin" drv="$out/driver/bench_dia"
echo ">> built: WW3 ($switch_now) in $wb, the driver $drv"

prepare() { # <case> <dir>: the case's inputs, as scripts/ww3-gpu.sh prepares them
  rm -rf -- "$2" && mkdir -p "$2"
  if [ "$1" = ww3_ts1 ]; then
    cp "$root"/WW3/regtests/ww3_ts1/input/* "$2"/
    cp "$2/ww3_grid_ST4_T475.nml" "$2/ww3_grid.nml"
    sed -i 's#\.\./input/#./#g' "$2"/*.nml "$2"/*.inp
  else
    cp "$root"/kokkos/tests/fixtures/bench_small/* "$2"/
    sed -i "s#^  DATE%FIELD = .*#  DATE%FIELD = '20200101 000000' '3600' '20200101 120000'#" "$2/ww3_shel.nml"
  fi
  (cd "$2" && "$bin/ww3_grid" > ww3_grid.out 2>&1) || { tail -20 "$2/ww3_grid.out" >&2; return 1; }
}
binding() { # <threads>: the OpenMP placement of kokkos/PORT_STATUS.md's runs
  if [ "$1" = 1 ]; then echo "OMP_PROC_BIND=false"; else echo "OMP_PROC_BIND=spread OMP_PLACES=cores"; fi
}

tsv="$out/results.tsv"
printf 'kind\tcase\tthreads\tvariant\tmedian\tunit\truns\n' > "$tsv"

# --- the kernel ---------------------------------------------------------------------------------
for c in ww3_ts1 bench_small; do
  g="$out/runs/grid-$c"; prepare "$c" "$g"
  for t in $threads; do
    for r in $(seq "$runs"); do
      # shellcheck disable=SC2046
      (cd "$g" && env $(binding "$t") OMP_NUM_THREADS="$t" "$drv" "$points" "$calls" 3) > "$g/kernel-t$t-r$r.log" 2>&1 \
        || { cat "$g/kernel-t$t-r$r.log" >&2; exit 1; }
    done
    head -1 "$g/kernel-t$t-r1.log"
    grep -h "bits_differing" "$g"/kernel-t"$t"-r*.log | sort -u
    for v in fortran v2_batch v2_point v1; do
      vals="$(awk -v k="${v}_ms" '$1 == k { print $2 }' "$g"/kernel-t"$t"-r*.log)"
      printf 'kernel\t%s\t%s\t%s\t%s\tms\t%s\n' "$c" "$t" "$v" "$(median <<<"$vals")" "$(tr '\n' ' ' <<<"$vals")" >> "$tsv"
    done
  done
done

# --- the whole model ----------------------------------------------------------------------------
if [ -z "$kernel_only" ]; then
  for c in ww3_ts1 bench_small; do
    for t in $threads; do
      for k in 0 1; do
        vals=""
        for r in $(seq "$runs"); do
          d="$out/runs/model-$c-t$t-k$k-r$r"; prepare "$c" "$d"
          if [ -f "$d/ww3_strt.inp" ] || [ -f "$d/ww3_strt.nml" ]; then (cd "$d" && "$bin/ww3_strt" > ww3_strt.out 2>&1); fi
          t0="$(date +%s.%N)"
          # shellcheck disable=SC2046
          (cd "$d" && env $(binding "$t") OMP_NUM_THREADS="$t" WW_KOKKOS_SNL1="$k" "$bin/ww3_shel" > ww3_shel.out 2>&1) \
            || { tail -20 "$d/ww3_shel.out" >&2; exit 1; }
          t1="$(date +%s.%N)"
          grep -q "End of program" "$d/ww3_shel.out" || { echo "bench_dia: ww3_shel did not finish in $d" >&2; exit 1; }
          vals="$vals$(awk -v a="$t0" -v b="$t1" 'BEGIN { printf "%.2f\n", b - a }')"$'\n'
        done
        if [ "$k" = 0 ]; then v=fortran; else v=kokkos; fi
        printf 'model\t%s\t%s\t%s\t%s\ts\t%s\n' "$c" "$t" "$v" "$(median <<<"$vals")" "$(tr '\n' ' ' <<<"$vals")" >> "$tsv"
      done
      echo ">> model $c, $t thread(s): $(awk -F'\t' -v c="$c" -v t="$t" '$1=="model" && $2==c && $3==t { printf "%s %s s  ", $4, $5 }' "$tsv")"
    done
  done
fi

column -t -s $'\t' "$tsv"
echo ">> $tsv"
if [ -n "${BENCH_REGTEST:-}" ]; then
  python3 "$root/scripts/bench_dia_record.py" "$tsv" --regtest "$BENCH_REGTEST" --points "$points" \
    --calls "$calls" --runs "$runs" --switch "$switch_now" --kernel-only "${kernel_only:-0}"
else
  echo ">> no BENCH_REGTEST: no record written (AGENTS.md, No timing without parity)"
fi
