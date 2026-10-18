#!/usr/bin/env bash
# regtest_selected.sh -- a few of WW3's own regtests as a guardrail for the resident Kokkos preset.
#
#   scripts/regtest_selected.sh [--jobs N] [--cases selected|matrix|smoke] [--dry-run]
#   scripts/regtest_selected.sh --self-test
#
# It adds no test and no verdict of its own (ADR-0010). It runs two existing commands, with their
# own acceptance criteria unchanged (WW3GPU_STRICT stays ON, test.comp and nccmp-tol decide):
#
#   matrix  WW3GPU_KOKKOS=resident WW3GPU_LAB_SWITCH=ON just ww3-gpu-matrix N serial -R <regex>
#           The 5 matrix.base entries of ww3_tp2.15 and ww3_ts2 that the resident preset can run
#           (regtests/README.md, "Selected regtests").
#   smoke   scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos resident
#           ww3_ts1 pair to pair with the Fortran of the same executable and with --ww3.
#
# Sized for the owner's i9-14900 (32 hardware threads, 184 GiB seen): 5 jobs of 6 threads, every
# selected matrix entry at once (regtests/README.md, "Threads and parallel entries").
# Never the GPU: no --gpu, and WW3GPU_GPU_CONFIRMED is unset. The log and the matrix summary go to
# build/regtest-selected/ (REGTEST_SELECTED_OUT overrides). The workflow regtest-selected.yml runs
# this on the owner's self-hosted runner; `just regtest-selected` runs it on any machine.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Match by test name, not by entry number: the numbers move when the WW3 pin changes matrix.base.
REGEX=':(ww3_tp2\.15|ww3_ts2):'
SUMMARY_DIR="build/ww3-gpu-matrix-serial-kokkos-resident-lab"

usage() { sed -n '2,/^set -euo/{/^set -euo/!p}' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 64; }

valid_jobs()  { [[ "$1" =~ ^[1-9][0-9]?$ ]]; }
# Threads per entry are nproc/jobs (the ww3-gpu-matrix recipe), so jobs * threads stays within the
# machine. Above nproc every entry would get 1 thread and the runs would compete for cores.
fits_cpus() { [ "$1" -le "$2" ]; }
valid_cases() { [[ "$1" =~ ^(selected|matrix|smoke)$ ]]; }

matrix_cmd() {  # jobs -> the command, one word per line
  printf '%s\n' env WW3GPU_KOKKOS=resident WW3GPU_LAB_SWITCH=ON just ww3-gpu-matrix "$1" serial -R "$REGEX"
}
smoke_cmd() {
  printf '%s\n' bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos resident
}

self_test() {
  local f=0
  t() { if [ "$2" = "$3" ]; then echo "  ok   $1"; else echo "  FAIL $1: got '$2', want '$3'" >&2; f=$((f + 1)); fi; }
  t "jobs 4 is valid" "$(valid_jobs 4 && echo y || echo n)" y
  t "jobs 0 is not" "$(valid_jobs 0 && echo y || echo n)" n
  t "jobs with a shell word is not" "$(valid_jobs '4;id' && echo y || echo n)" n
  t "5 jobs fit 32 hardware threads" "$(fits_cpus 5 32 && echo y || echo n)" y
  t "16 jobs do not fit 8" "$(fits_cpus 16 8 && echo y || echo n)" n
  t "cases matrix is valid" "$(valid_cases matrix && echo y || echo n)" y
  t "cases selected is valid" "$(valid_cases selected && echo y || echo n)" y
  t "'all' is not a case: it would read as WW3's whole matrix" "$(valid_cases all && echo y || echo n)" n
  t "an unknown case is not" "$(valid_cases gpu && echo y || echo n)" n
  t "the matrix runs the resident preset" "$(matrix_cmd 4 | grep -c '^WW3GPU_KOKKOS=resident$')" 1
  t "with the lab switch's source terms" "$(matrix_cmd 4 | grep -c '^WW3GPU_LAB_SWITCH=ON$')" 1
  t "the serial selection" "$(matrix_cmd 4 | grep -c '^serial$')" 1
  t "the regex reaches ctest as one word" "$(matrix_cmd 4 | tail -1)" "$REGEX"
  t "the regex takes ww3_tp2.15 entries" "$(grep -cE "$REGEX" <<<'003:ww3_tp2.15:default')" 1
  t "and ww3_ts2" "$(grep -cE "$REGEX" <<<'007:ww3_ts2:ST4_PR3_UQ')" 1
  t "not the unstructured ww3_tp2.6, which the preset refuses by design" "$(grep -cE "$REGEX" <<<'001:ww3_tp2.6:ST4' || true)" 0
  t "nor a look-alike name" "$(grep -cE "$REGEX" <<<'009:ww3_tp2_15:x' || true)" 0
  t "the matrix never names the GPU" "$(matrix_cmd 4 | grep -ci 'gpu_confirmed\|--gpu$' || true)" 0
  t "the smoke run is --cpu" "$(smoke_cmd | grep -c '^--cpu$')" 1
  t "and never --gpu" "$(smoke_cmd | grep -c '^--gpu$' || true)" 0
  echo "regtest_selected self-test: $([ "$f" -eq 0 ] && echo ok || echo "$f FAILED")"
  [ "$f" -eq 0 ]
}

jobs=5 cases=selected dry=0
while [ $# -gt 0 ]; do
  case "$1" in
    --self-test) self_test; exit $? ;;
    --jobs) [ $# -ge 2 ] || usage; jobs="$2"; shift ;;
    --cases) [ $# -ge 2 ] || usage; cases="$2"; shift ;;
    --dry-run) dry=1 ;;
    -h|--help) usage ;;
    *) echo "regtest_selected: unknown argument '$1'" >&2; usage ;;
  esac
  shift
done
valid_jobs "$jobs" || { echo "regtest_selected: --jobs is 1 to 99, not '$jobs'" >&2; exit 64; }
valid_cases "$cases" || { echo "regtest_selected: --cases is selected, matrix or smoke, not '$cases'" >&2; exit 64; }
ncpu=$(nproc --all)
fits_cpus "$jobs" "$ncpu" || { echo "regtest_selected: --jobs $jobs is more than the $ncpu hardware threads; every entry would get 1 thread and the runs would compete for cores" >&2; exit 64; }
unset WW3GPU_GPU_CONFIRMED

cd "$root"
out="${REGTEST_SELECTED_OUT:-$root/build/regtest-selected}"
mapfile -t mcmd < <(matrix_cmd "$jobs")
mapfile -t scmd < <(smoke_cmd)
if [ "$dry" = 1 ]; then
  if [ "$cases" != smoke ]; then printf '%q ' "${mcmd[@]}"; echo; fi
  if [ "$cases" != matrix ]; then printf '%q ' "${scmd[@]}"; echo; fi
  echo "log and summary: $out"
  exit 0
fi

for tool in nix just git; do
  command -v "$tool" >/dev/null || { echo "regtest_selected: '$tool' is not on PATH" >&2; exit 3; }
done
mkdir -p "$out"
log="$out/regtest-selected.log"
{
  echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "# ww3-gpu commit: $(git rev-parse --short HEAD)$(git diff --quiet || echo ' (dirty)')"
  echo "# WW3 commit: $(git -C WW3 rev-parse --short HEAD 2>/dev/null || echo '?')"
  echo "# machine: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ //'), $(nproc --all) hardware threads"
  echo "# cases: $cases, jobs: $jobs"
} | tee "$log"

rc=0
if [ "$cases" != smoke ]; then
  echo "== matrix: ${mcmd[*]}" | tee -a "$log"
  "${mcmd[@]}" 2>&1 | tee -a "$log" || rc=1
  [ ! -f "$SUMMARY_DIR/summary.tsv" ] || cp "$SUMMARY_DIR/summary.tsv" "$out/matrix-summary.tsv"
fi
if [ "$cases" != matrix ]; then
  echo "== smoke: ${scmd[*]}" | tee -a "$log"
  "${scmd[@]}" 2>&1 | tee -a "$log" || rc=1
fi
echo "regtest_selected: $([ "$rc" = 0 ] && echo passed || echo FAILED); log $log" | tee -a "$log"
exit "$rc"
