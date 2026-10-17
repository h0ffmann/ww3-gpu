#!/usr/bin/env bash
# gpu_supervised — the owner's confirmation before a command that runs WW3 on the GPU (AGENTS.md,
# "Running on the GPU"): the same check as scripts/ww3-gpu.sh --gpu, for the commands that do not
# go through that script (just ww3-gpu-matrix-cuda).
#
#   scripts/gpu_supervised.sh "<what will run>"   exit 0 after a typed "yes" at a terminal
#   scripts/gpu_supervised.sh --self-test
#
# Exit: 0 confirmed; 4 nvidia-smi cannot reach a GPU; 7 no terminal or no "yes"; 64 no description.
set -euo pipefail

if [ "${1:-}" = --self-test ]; then
  "${BASH_SOURCE[0]}" </dev/null >/dev/null 2>&1 && rc=0 || rc=$?
  [ "$rc" = 64 ] || { echo "gpu_supervised: no description gave exit $rc, not 64" >&2; exit 1; }
  "${BASH_SOURCE[0]}" "a GPU run" </dev/null >/dev/null 2>&1 && rc=0 || rc=$?
  [[ "$rc" =~ ^(4|7)$ ]] || { echo "gpu_supervised: no terminal gave exit $rc, not 4 or 7" >&2; exit 1; }
  echo "gpu_supervised self-test: ok"
  exit 0
fi

what="${1:-}"
[ -n "$what" ] || { echo "gpu_supervised: say what will run on the GPU" >&2; exit 64; }
nvidia-smi -L >/dev/null 2>&1 || { echo "gpu_supervised: nvidia-smi cannot reach a GPU" >&2; exit 4; }
if [ ! -t 0 ]; then
  echo "gpu_supervised: $what runs only under a human's supervision (AGENTS.md hard rule): run it at a terminal" >&2
  exit 7
fi
printf 'gpu_supervised: %s runs the Kokkos port on %s. Are you supervising this run? Type yes: ' \
  "$what" "$(nvidia-smi -L | head -1)" >&2
read -r answer
[ "$answer" = yes ] || { echo "gpu_supervised: not confirmed; nothing ran" >&2; exit 7; }
