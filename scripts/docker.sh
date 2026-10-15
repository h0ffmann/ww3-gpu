#!/usr/bin/env bash
# docker — the Ubuntu runtime image of docker/Dockerfile: WW3 (--ww3), WW3 with the Kokkos DIA
# (--hoff) and WW4 (--ww4), compiled in the image's build stage, no Nix. scripts/hoff.sh is its
# entry point, with HOFF_PREBUILT set, so a run in the image is the same run as on the host.
#
#   scripts/docker.sh build [tag]          build ww3-gpu:<tag> (default local) from this checkout
#   scripts/docker.sh run [hoff.sh args]   run hoff.sh in the image, any of its options; runs land in
#                                          build/docker-runs/. The image's /opt/ww3-gpu/BUILD_INFO says
#                                          what it was built from.
#   scripts/docker.sh --dry-run <cmd> ...  print the docker command, run nothing
#   scripts/docker.sh --self-test
#
# DOCKER_IMAGE overrides the image name (ww3-gpu:local). CPU only: the image has no CUDA build,
# and hoff.sh refuses --gpu in it (exit 64).
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
image="${DOCKER_IMAGE:-ww3-gpu:local}"
dry=""

usage() { sed -n '2,/^set -euo/{/^set -euo/!p}' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 64; }
say() { printf '%q ' "$@"; echo; }
do_or_say() { if [ -n "$dry" ]; then say "$@"; else "$@"; fi; }
rev() { git -C "$1" rev-parse --short HEAD 2>/dev/null || echo "?"; }

if [ "${1:-}" = --self-test ]; then
  out="$(bash "${BASH_SOURCE[0]}" --dry-run run --hoff smoke)"
  for want in --user /work/runs "--hoff smoke"; do
    grep -q -- "$want" <<<"$out" || { echo "docker: run lost '$want': $out" >&2; exit 1; }
  done
  out="$(bash "${BASH_SOURCE[0]}" --dry-run build t1)"
  for want in docker/Dockerfile ww3-gpu:t1 WW3_REV=; do
    grep -q -- "$want" <<<"$out" || { echo "docker: build lost '$want': $out" >&2; exit 1; }
  done
  bash "${BASH_SOURCE[0]}" --dry-run bogus >/dev/null 2>&1 && { echo "docker: accepted a bad command" >&2; exit 1; }
  echo "docker self-test: ok"
  exit 0
fi
[ "${1:-}" = --dry-run ] && { dry=1; shift; }
cmd="${1:-}"; [ -n "$cmd" ] || usage; shift

case "$cmd" in
  build)
    tag="${1:-local}"
    # WW4's sources and its yaml-cpp/googletest are compiled in the build stage.
    [ -n "$dry" ] || [ -f "$root/WW4/externals/yaml-cpp/CMakeLists.txt" ] || {
      git -C "$root" submodule update --init --depth 1 WW4
      git -C "$root/WW4" submodule update --init --depth 1 externals/yaml-cpp externals/googletest
    }
    do_or_say docker build -f "$root/docker/Dockerfile" -t "ww3-gpu:$tag" \
      --build-arg "WW3_GPU_REV=$(rev "$root")" --build-arg "WW3_REV=$(rev "$root/WW3")" \
      --build-arg "WW4_REV=$(rev "$root/WW4")" "$root" ;;
  run)
    # As the caller's uid, so the runs on the host belong to whoever started them.
    [ -n "$dry" ] || mkdir -p "$root/build/docker-runs"
    do_or_say docker run --rm --user "$(id -u):$(id -g)" -v "$root/build/docker-runs:/work/runs" "$image" "$@" ;;
  *) echo "docker: unknown command: $cmd" >&2; usage ;;
esac
