#!/usr/bin/env bash
# ww3_fortran_ab — does a Fortran change proposed to NOAA keep WW3's bits, and what does it save?
#
#   scripts/ww3_fortran_ab.sh --patch <file> --case smoke|full|coast [--repeat N] [--dry-run]
#   scripts/ww3_fortran_ab.sh --self-test
#
# A is pure NOAA WW3 (WW3-noaa/ at its pin), B the same tree with <file> applied, both built by
# scripts/ww3-gpu.sh --ww3 with the same toolchain and switch (WW3GPU_SWITCH, the lab switch by
# default); B through WW3GPU_NOAA_PATCH, which applies the patch to a scratch copy under build/.
# No submodule is written (AGENTS.md, "WW3 source is read-only"). Runs alternate A, B, A, B, ...
# so a drift of the machine hits both. Every model file of every run is compared with cmp against
# A's first run: B must equal A (the change keeps the bits), and every A must equal A's first run
# (the case is reproducible). The verdict is cmp, never a tolerance.
#
# Writes $WW3GPU_RUNS/fortran-ab-<case>/ (default build/runs): runs/<A|B><i>/ with each run's
# model files and wall time, and summary.tsv with the command, the commits, the walls and the
# medians. Wall times on a shared machine are not a measurement: give the load with the number.
#
# Exit codes: 0 every file identical; 1 a file differs or a run failed; 64 a bad argument.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
usage() { sed -n '2,/^set -euo/{/^set -euo/!p}' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 64; }

median() { # <numbers...>: the median, the mean of the two middle values for an even count
  printf '%s\n' "$@" | sort -g | awk '{ v[NR] = $1 } END {
    if (NR % 2) printf "%.2f\n", v[(NR + 1) / 2]; else printf "%.2f\n", (v[NR / 2] + v[NR / 2 + 1]) / 2 }'
}

model_files() { # <dir>: the files WW3 writes that carry the model state
  (cd "$1" && find . -maxdepth 1 -type f \( -name 'out_grd.*' -o -name 'out_pnt.*' -o -name 'restart*.ww3' \
     -o -name 'nest*.ww3' -o -name 'track_o.*' -o -name 'ww3.*.nc' \) -printf '%f\n' | sort)
}

same_files() { # <reference dir> <dir> <label>: cmp every model file of either; 1 when one differs
  local ref="$1" dir="$2" label="$3" f bad=0
  for f in $( { model_files "$ref"; model_files "$dir"; } | sort -u); do
    if [ ! -f "$ref/$f" ] || [ ! -f "$dir/$f" ]; then
      echo "   $label: $f is missing on one side" >&2; bad=1
    elif ! cmp -s "$ref/$f" "$dir/$f"; then
      echo "   $label: $f differs" >&2; bad=1
    fi
  done
  return "$bad"
}

parse() { # the arguments; sets patch case_name repeat dry_run, or exits 64
  patch="" case_name="" repeat=1 dry_run=""
  while [ $# -gt 0 ]; do
    case "$1" in
      --patch) [ $# -ge 2 ] || { echo "ww3_fortran_ab: --patch needs a file" >&2; exit 64; }
               patch="$2"; shift ;;
      --case) [ $# -ge 2 ] && [[ "$2" =~ ^(smoke|full|coast)$ ]] \
                || { echo "ww3_fortran_ab: --case is smoke, full or coast" >&2; exit 64; }
              case_name="$2"; shift ;;
      --repeat) [ $# -ge 2 ] && [[ "$2" =~ ^[1-9][0-9]?$ ]] \
                  || { echo "ww3_fortran_ab: --repeat needs a count, 1 to 99" >&2; exit 64; }
                repeat="$2"; shift ;;
      --dry-run) dry_run=1 ;;
      -h|--help) usage ;;
      *) echo "ww3_fortran_ab: unknown argument $1" >&2; exit 64 ;;
    esac
    shift
  done
  [ -n "$patch" ] && [ -n "$case_name" ] || { echo "ww3_fortran_ab: --patch and --case are required" >&2; exit 64; }
  [ -f "$patch" ] || { echo "ww3_fortran_ab: no such patch: $patch" >&2; exit 64; }
}

self_test() {
  local t ok=0 d
  [ "$(median 3 1 2)" = 2.00 ] || { echo "self-test: median of 3 1 2" >&2; ok=1; }
  [ "$(median 4 1 3 2)" = 2.50 ] || { echo "self-test: median of 4 1 3 2" >&2; ok=1; }
  d="$(mktemp -d)"
  trap 'rm -rf -- "$d"' RETURN
  touch "$d/p.patch"
  for t in "" "--case smoke" "--patch $d/p.patch" "--patch $d/none --case smoke" \
           "--patch $d/p.patch --case big" "--patch $d/p.patch --case smoke --repeat 0" \
           "--patch $d/p.patch --case smoke --repeat x" "--patch $d/p.patch --case smoke --bogus"; do
    # shellcheck disable=SC2086 # word splitting is the point: each case is an argument list
    if (parse $t) >/dev/null 2>&1; then echo "self-test: accepted '$t'" >&2; ok=1; fi
  done
  (parse --patch "$d/p.patch" --case full --repeat 3 --dry-run) >/dev/null 2>&1 \
    || { echo "self-test: refused a good argument list" >&2; ok=1; }
  mkdir -p "$d/a" "$d/b"
  printf 'x' > "$d/a/out_grd.ww3"; printf 'x' > "$d/b/out_grd.ww3"
  same_files "$d/a" "$d/b" t 2>/dev/null || { echo "self-test: equal files reported different" >&2; ok=1; }
  printf 'y' > "$d/b/out_grd.ww3"
  ! same_files "$d/a" "$d/b" t 2>/dev/null || { echo "self-test: a difference was missed" >&2; ok=1; }
  printf 'y' > "$d/a/out_grd.ww3"; printf 'z' > "$d/b/restart.ww3"
  ! same_files "$d/a" "$d/b" t 2>/dev/null || { echo "self-test: a file missing on one side was missed" >&2; ok=1; }
  [ "$ok" = 0 ] && echo "ww3_fortran_ab: self-test ok"
  return "$ok"
}

[ "${1:-}" = --self-test ] && { self_test; exit $?; }
parse "$@"
patch="$(cd "$(dirname "$patch")" && pwd)/$(basename "$patch")"
runs="${WW3GPU_RUNS:-$root/build/runs}"
out="$runs/fortran-ab-$case_name"
gpu="$root/scripts/ww3-gpu.sh"

if [ -n "$dry_run" ]; then
  echo "ww3_fortran_ab --dry-run: $repeat run(s) each, alternated, case $case_name, results in $out"
  echo "+ bash $gpu --ww3 $case_name                                  # A: WW3-noaa/ as it is"
  echo "+ WW3GPU_NOAA_PATCH=$patch bash $gpu --ww3 $case_name   # B: the patched copy"
  exit 0
fi

[[ "$out" == "$runs/"* ]] && rm -rf -- "$out"
mkdir -p "$out/runs"
bad=0 walls_a=() walls_b=()
for i in $(seq 1 "$repeat"); do
  for side in A B; do
    if [ "$side" = A ]; then
      bash "$gpu" --ww3 "$case_name" || { echo "ww3_fortran_ab: run A$i failed" >&2; exit 1; }
      src="$runs/ww3-$case_name"
    else
      WW3GPU_NOAA_PATCH="$patch" bash "$gpu" --ww3 "$case_name" || { echo "ww3_fortran_ab: run B$i failed" >&2; exit 1; }
      src="$runs/ww3-patched-$case_name"
    fi
    dst="$out/runs/$side$i"
    mkdir -p "$dst"
    for f in $(model_files "$src") ww3_shel.wall; do cp "$src/$f" "$dst/"; done
    if [ "$side" = A ]; then walls_a+=("$(cat "$dst/ww3_shel.wall")"); else walls_b+=("$(cat "$dst/ww3_shel.wall")"); fi
    [ "$side$i" = A1 ] && continue
    same_files "$out/runs/A1" "$dst" "$side$i against A1" || bad=1
  done
done

ma="$(median "${walls_a[@]}")" mb="$(median "${walls_b[@]}")"
{
  printf 'command\tscripts/ww3_fortran_ab.sh --patch %s --case %s --repeat %s\n' "${patch#"$root/"}" "$case_name" "$repeat"
  printf 'ww3_noaa\t%s\n' "$(git -C "$root/WW3-noaa" rev-parse HEAD)"
  printf 'patch_sha256\t%s\n' "$(sha256sum < "$patch" | cut -d' ' -f1)"
  printf 'switch\t%s\n' "${WW3GPU_SWITCH:-switch_lab_ompg}"
  printf 'host\t%s\n' "$(uname -n)"
  printf 'load_1min_end\t%s\n' "$(cut -d' ' -f1 /proc/loadavg)"
  printf 'walls_A\t%s\n' "${walls_a[*]}"
  printf 'walls_B\t%s\n' "${walls_b[*]}"
  printf 'median_A\t%s\nmedian_B\t%s\n' "$ma" "$mb"
  printf 'bits\t%s\n' "$([ "$bad" = 0 ] && echo identical || echo DIFFER)"
} > "$out/summary.tsv"
cat "$out/summary.tsv"
if [ "$bad" = 0 ]; then
  echo ">> every model file of every run is byte-identical to A1: the patch keeps the bits on $case_name"
else
  echo ">> a model file differs: the patch changes the result on $case_name (see the lines above)" >&2
fi
exit "$bad"
