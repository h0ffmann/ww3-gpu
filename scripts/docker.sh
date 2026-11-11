#!/usr/bin/env bash
# docker — the Ubuntu runtime image of docker/Dockerfile: NOAA's WW3 (--ww3-noaa), WW3 with the Kokkos DIA
# (--ww3-gpu) and WW4 (--ww4), compiled in the image's build stage, no Nix. scripts/ww3-gpu.sh is its
# entry point, with WW3GPU_PREBUILT set, so a run in the image is the same run as on the host.
#
#   scripts/docker.sh build [tag]          build ww3-gpu:<tag> (default local) from this checkout
#   scripts/docker.sh run [ww3-gpu.sh args]   run ww3-gpu.sh in the image, any of its options; runs land in
#                                          build/docker-runs/. The image's /opt/ww3-gpu/BUILD_INFO says
#                                          what it was built from.
#   scripts/docker.sh build-cuda [tag]     build ww3-gpu-cuda:<tag> from docker/Dockerfile.cuda
#   scripts/docker.sh run-cuda [args]      run ww3-gpu.sh in the GPU image with --gpus all
#   scripts/docker.sh build-dev [tag]      build ww3-gpu-dev:<tag>, the dev stage of docker/Dockerfile
#   scripts/docker.sh run-dev [cmd]        a shell (or cmd) in the dev image, this checkout mounted
#   scripts/docker.sh submodules           fetch WW3, WW3-noaa and WW4 at their pins over https
#   scripts/docker.sh check <image>        a runtime image holds both WW3s compiled and no source file,
#                                          and its entry point starts (--build-only, no run)
#   scripts/docker.sh push <image> <ref>...   tag a local image as each ref and push it (by hand only)
#   scripts/docker.sh --dry-run <cmd> ...  print the docker command, run nothing
#   scripts/docker.sh --self-test
#
# DOCKER_IMAGE overrides the image name (ww3-gpu:local, ww3-gpu-cuda:local, ww3-gpu-dev:local). The CPU image has no
# CUDA build, and ww3-gpu.sh refuses --gpu in it (exit 64); the GPU image runs only --ww3-gpu --gpu.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
image="${DOCKER_IMAGE:-ww3-gpu:local}"
dry=""

usage() { sed -n '2,/^set -euo/{/^set -euo/!p}' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 64; }
say() { printf '%q ' "$@"; echo; }
do_or_say() { if [ -n "$dry" ]; then say "$@"; else "$@"; fi; }
rev() { git -C "$1" rev-parse --short HEAD 2>/dev/null || echo "?"; }
# A submodule's pin as the superproject records it, checked out or not.
pin() { git -C "$root" rev-parse --short "HEAD:$1" 2>/dev/null || echo "?"; }
# OCI labels: the source label links a GHCR package to this repository; the pins say which WW3
# (NOAA's and the fork) the binaries were compiled from without the sources being in the image.
# $1 the image's title, $2 its description (else the base image's "ubuntu" shows on GHCR).
labels() {
  printf -- '--label\n%s\n' \
    "org.opencontainers.image.title=$1" "org.opencontainers.image.description=$2" \
    "org.opencontainers.image.source=https://github.com/h0ffmann/ww3-gpu" \
    "org.opencontainers.image.revision=$(git -C "$root" rev-parse HEAD 2>/dev/null || echo "?")" \
    "org.opencontainers.image.licenses=MIT" \
    "io.github.h0ffmann.ww3-gpu.ww3-noaa=$(pin WW3-noaa)" \
    "io.github.h0ffmann.ww3-gpu.ww3=$(pin WW3)"
}

if [ "${1:-}" = --self-test ]; then
  out="$(bash "${BASH_SOURCE[0]}" --dry-run run --ww3-gpu smoke)"
  for want in --user /work/runs "--ww3-gpu smoke"; do
    grep -q -- "$want" <<<"$out" || { echo "docker: run lost '$want': $out" >&2; exit 1; }
  done
  out="$(bash "${BASH_SOURCE[0]}" --dry-run build t1)"
  for want in docker/Dockerfile ww3-gpu:t1 WW3_REV=; do
    grep -q -- "$want" <<<"$out" || { echo "docker: build lost '$want': $out" >&2; exit 1; }
  done
  out="$(bash "${BASH_SOURCE[0]}" --dry-run run-cuda --ww3-gpu smoke --gpu </dev/null)"
  for want in "--gpus all" ww3-gpu-cuda:local "--gpu"; do
    grep -q -- "$want" <<<"$out" || { echo "docker: run-cuda lost '$want': $out" >&2; exit 1; }
  done
  out="$(bash "${BASH_SOURCE[0]}" --dry-run build-cuda t1)"
  grep -q -- "docker/Dockerfile.cuda" <<<"$out" || { echo "docker: build-cuda lost its Dockerfile: $out" >&2; exit 1; }
  out="$(bash "${BASH_SOURCE[0]}" --dry-run build-dev t1)"
  for want in "--target dev" ww3-gpu-dev:t1 BUILDKIT_INLINE_CACHE=1 org.opencontainers.image.source=; do
    grep -q -- "$want" <<<"$out" || { echo "docker: build-dev lost '$want': $out" >&2; exit 1; }
  done
  out="$(bash "${BASH_SOURCE[0]}" --dry-run run-dev just --list </dev/null)"
  for want in /workspaces/ww3-gpu ww3-gpu-dev:local "just --list"; do
    grep -q -- "$want" <<<"$out" || { echo "docker: run-dev lost '$want': $out" >&2; exit 1; }
  done
  out="$(bash "${BASH_SOURCE[0]}" --dry-run push ww3-gpu:t1 ghcr.io/x/ww3-gpu:a ghcr.io/x/ww3-gpu:b)"
  for want in "tag ww3-gpu:t1 ghcr.io/x/ww3-gpu:a" "push ghcr.io/x/ww3-gpu:b"; do
    grep -q -- "$want" <<<"$out" || { echo "docker: push lost '$want': $out" >&2; exit 1; }
  done
  out="$(bash "${BASH_SOURCE[0]}" --dry-run check ww3-gpu:t1)"
  for want in "--entrypoint sh ww3-gpu:t1" BUILD_INFO "binaries only" "--ww3-noaa smoke --build-only" "--ww3-gpu smoke --build-only"; do
    grep -q -- "$want" <<<"$out" || { echo "docker: check lost '$want': $out" >&2; exit 1; }
  done
  bash "${BASH_SOURCE[0]}" --dry-run push ww3-gpu:t1 >/dev/null 2>&1 && { echo "docker: push without a ref" >&2; exit 1; }
  bash "${BASH_SOURCE[0]}" --dry-run bogus >/dev/null 2>&1 && { echo "docker: accepted a bad command" >&2; exit 1; }
  echo "docker self-test: ok"
  exit 0
fi
[ "${1:-}" = --dry-run ] && { dry=1; shift; }
cmd="${1:-}"; [ -n "$cmd" ] || usage; shift

case "$cmd" in
  build)
    tag="${1:-local}"
    # --ww3-noaa is built from pure NOAA WW3 (WW3-noaa/), --ww3-gpu from the fork (WW3/).
    [ -n "$dry" ] || [ -f "$root/WW3-noaa/model/src/CMakeLists.txt" ] \
      || git -C "$root" submodule update --init --depth 1 WW3-noaa
    # WW4's sources and its yaml-cpp/googletest are compiled in the build stage.
    [ -n "$dry" ] || [ -f "$root/WW4/externals/yaml-cpp/CMakeLists.txt" ] || {
      git -C "$root" submodule update --init --depth 1 WW4
      git -C "$root/WW4" submodule update --init --depth 1 externals/yaml-cpp externals/googletest
    }
    mapfile -t lab < <(labels ww3-gpu "WW3 (NOAA, --ww3-noaa) and ww3-gpu (Kokkos OpenMP, --ww3-gpu) compiled side by side, and WW4; binaries only")
    do_or_say docker build -f "$root/docker/Dockerfile" -t "ww3-gpu:$tag" "${lab[@]}" \
      --build-arg "WW3_GPU_REV=$(rev "$root")" --build-arg "WW3_REV=$(rev "$root/WW3")" \
      --build-arg "WW3_NOAA_REV=$(rev "$root/WW3-noaa")" \
      --build-arg "WW4_REV=$(rev "$root/WW4")" "$root" ;;
  run)
    # As the caller's uid, so the runs on the host belong to whoever started them.
    # WW3GPU_ROUTINES picks the Kokkos routines of --ww3-gpu (scripts/ww3-gpu.sh); unset, the script
    # chooses (SNL1 on the CPU, --kokkos gpu on the GPU).
    [ -n "$dry" ] || mkdir -p "$root/build/docker-runs"
    do_or_say docker run --rm --user "$(id -u):$(id -g)" -e "WW3GPU_ROUTINES=${WW3GPU_ROUTINES:-}" \
      -v "$root/build/docker-runs:/work/runs" "$image" "$@" ;;
  build-cuda)
    # The GPU image (docker/Dockerfile.cuda): --ww3-gpu on Kokkos' CUDA backend, and --ww3-noaa from
    # pure NOAA WW3 (WW3-noaa/) on the CPU as its reference.
    [ -n "$dry" ] || [ -f "$root/WW3-noaa/model/src/CMakeLists.txt" ] \
      || git -C "$root" submodule update --init --depth 1 WW3-noaa
    mapfile -t lab < <(labels ww3-gpu-cuda "ww3-gpu on Kokkos CUDA (--ww3-gpu --gpu) and WW3 (NOAA, --ww3-noaa) on the CPU; binaries only")
    do_or_say docker build -f "$root/docker/Dockerfile.cuda" -t "ww3-gpu-cuda:${1:-local}" "${lab[@]}" \
      --build-arg "WW3_GPU_REV=$(rev "$root")" --build-arg "WW3_REV=$(rev "$root/WW3")" \
      --build-arg "WW3_NOAA_REV=$(rev "$root/WW3-noaa")" "$root" ;;
  run-cuda)
    # --gpus all needs the NVIDIA Container Toolkit on the host. -it when there is a terminal: the
    # script asks the supervising human for "yes" before any --gpu run (AGENTS.md hard rule).
    [ -n "$dry" ] || mkdir -p "$root/build/docker-runs"
    tty=(); [ -t 0 ] && tty=(-it)
    do_or_say docker run --rm "${tty[@]}" --gpus all --user "$(id -u):$(id -g)" \
      -e "WW3GPU_ROUTINES=${WW3GPU_ROUTINES:-}" -v "$root/build/docker-runs:/work/runs" \
      "${DOCKER_IMAGE:-ww3-gpu-cuda:local}" "$@" ;;
  build-dev)
    # The dev stage copies no source, so no submodule is needed. The inline cache lets
    # .devcontainer/devcontainer.json (cacheFrom) reuse a pushed image instead of rebuilding Kokkos.
    mapfile -t lab < <(labels ww3-gpu-dev "Development image of ww3-gpu: compilers, netCDF, Kokkos, gate tools, Nix; mount the checkout")
    do_or_say docker build -f "$root/docker/Dockerfile" --target dev -t "ww3-gpu-dev:${1:-local}" \
      "${lab[@]}" --build-arg BUILDKIT_INLINE_CACHE=1 "$root" ;;
  run-dev)
    # The checkout is mounted where the dev container mounts it; the user is the image's ubuntu
    # (uid 1000), who owns /nix, so files written there belong to uid 1000 on the host.
    tty=(); [ -t 0 ] && tty=(-it)
    do_or_say docker run --rm "${tty[@]}" -v "$root:/workspaces/ww3-gpu" -w /workspaces/ww3-gpu \
      "${DOCKER_IMAGE:-ww3-gpu-dev:local}" "${@:-bash}" ;;
  submodules)
    # .gitmodules uses ssh URLs, hence https; a pin on a topic branch may not be fetched by
    # `submodule update`, so the fallback fetches the SHA itself.
    export GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=url.https://github.com/.insteadOf GIT_CONFIG_VALUE_0=git@github.com:
    for sm in WW3 WW3-noaa WW4; do
      if [ -n "$dry" ]; then say git submodule update --init --depth 1 "$sm"; continue; fi
      git -C "$root" submodule update --init --depth 1 "$sm" || {
        sha=$(git -C "$root" ls-tree HEAD "$sm" | awk '{print $3}')
        git -C "$root/$sm" fetch --depth 1 origin "$sha" && git -C "$root/$sm" checkout -q "$sha"
      }
    done
    do_or_say git -C "$root/WW4" submodule update --init --depth 1 externals/yaml-cpp externals/googletest ;;
  check)
    # Runs no case: --ww3-noaa (NOAA's WW3, the fork's base) and --ww3-gpu must both be compiled
    # in (MetOcean's WW3, --ww3, is not in the images yet), and nothing in the
    # image's tree may be a Fortran or C/C++ source, so the image is a binary distribution.
    img="${1:-}"; [ -n "$img" ] || { echo "docker: check <image>" >&2; exit 64; }
    # shellcheck disable=SC2016  # expanded by the container's sh
    do_or_say docker run --rm --entrypoint sh "$img" -c '
      set -e; cat /opt/ww3-gpu/BUILD_INFO; P=/opt/ww3-gpu/prebuilt
      test -x "$P/ww3/bin/ww3_shel" || { echo "no NOAA ww3_shel" >&2; exit 1; }
      ls -d "$P"/ww3-gpu-*/bin/ww3_shel || { echo "no ww3-gpu ww3_shel" >&2; exit 1; }
      src=$(find /opt/ww3-gpu /work -type f \( -name "*.[fF]90" -o -name "*.ftn" -o -name "*.[ch]" \
            -o -name "*.[ch]pp" -o -name "*.cu" -o -name CMakeLists.txt \) | head)
      [ -z "$src" ] || { echo "sources in the image:" >&2; echo "$src" >&2; exit 1; }
      echo "binaries only: ok"'
    # The entry point must get past its argument and file checks in both modes; --build-only
    # stops before any run (and --gpu --build-only needs neither a GPU nor a person).
    gpu=()
    if [ -z "$dry" ] && docker run --rm --entrypoint test "$img" -d /opt/ww3-gpu/prebuilt/ww3-gpu-cuda; then gpu=(--gpu); fi
    do_or_say docker run --rm "$img" --ww3-noaa smoke --build-only
    do_or_say docker run --rm "$img" --ww3-gpu smoke "${gpu[@]}" --build-only ;;
  push)
    # Never called by CI on its own: .github/workflows/docker-*.yml run it only from a manual
    # workflow_dispatch with push set.
    src="${1:-}"; shift || true
    [ -n "$src" ] && [ "$#" -gt 0 ] || { echo "docker: push <image> <ref>..." >&2; exit 64; }
    for ref in "$@"; do do_or_say docker tag "$src" "$ref"; do_or_say docker push "$ref"; done ;;
  *) echo "docker: unknown command: $cmd" >&2; usage ;;
esac
