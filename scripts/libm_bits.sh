#!/usr/bin/env bash
# libm_bits — can a device reproduce the libm bits WW3's Fortran gets on this CPU? Builds
# kokkos/tools/libm_bits in the pinned shell (#ww3 for the host checks, #cuda for the device
# check) and runs its checks. docs/GPU_LIBM_BITS_202610.md says what each check means and
# records the results.
#
#   scripts/libm_bits.sh probe            which glibc build (fma or sse2) this CPU gets
#   scripts/libm_bits.sh fingerprint      hashes of this machine's libm bits, to compare machines
#   scripts/libm_bits.sh all [threads]    every host check, one TSV line each (default 4 threads)
#   scripts/libm_bits.sh <libm_bits args> one host check, e.g. `ww expf fma 8`
#   scripts/libm_bits.sh --nvcc           compile device_check.cu for sm_89 and sm_90, run nothing
#   scripts/libm_bits.sh --device [fn]    run device_check on GPU 0 (every transcribed function, or
#                                         one: expf logf sincosf powf mvec_expf mvec_powf tanhf
#                                         log10f atan2f exp tanh): the owner only, at a terminal
#   scripts/libm_bits.sh --self-test      build, the probe, one host check that must pass
#
# Host checks run under nice: they take every thread they are given for tens of minutes.
# GLIBC_TUNABLES set by the caller passes through, e.g. glibc.cpu.hwcaps=-AVX2,-FMA for glibc's
# sse2 build. Exit codes: 0 no difference; 1 a check found a difference; 2 a CUDA error;
# 7 --device without a supervising human; 64 usage.
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
pratico="$root/picasso/labs/pratico"
src="$root/kokkos/tools/libm_bits"
out="$root/build/libm_bits"

if [ -z "${LIBM_BITS_IN_SHELL:-}" ]; then
  [ -f "$pratico/flake.nix" ] || { echo "libm_bits: picasso is missing; run just submodule-init" >&2; exit 64; }
  shell=ww3
  case "${1:-}" in --nvcc|--device) shell=cuda ;; esac
  exec env LIBM_BITS_IN_SHELL=1 nix develop "$pratico#$shell" --command bash "${BASH_SOURCE[0]}" "$@"
fi

mkdir -p "$out"
# libmvec powf's tables, read from the libmvec.so.1 beside the libc this toolchain links, and
# this CPU's rcpps (gen_tables.cpp).
g++ -std=c++20 -O2 -Wall -Wextra -o "$out/gen_tables" "$src/gen_tables.cpp"
libc=$(ldd "$out/gen_tables" | awk '/libc\.so\.6/ {print $3}')
mvec="$(dirname "$libc")/libmvec.so.1"
mvec_off=$(readelf -Ws "$mvec" | awk '$8 == "__svml_spow_data" {print $2}')
[ -n "$mvec_off" ] || { echo "libm_bits: no __svml_spow_data in $mvec" >&2; exit 3; }
nice -n 19 "$out/gen_tables" "$mvec" "$mvec_off" "$out/svml_tables.hpp"

nvcc_build() {
  # --fmad=false: glibc_mathf.hpp fuses only where it calls ::fma.
  nvcc -std=c++20 -O3 --fmad=false -gencode arch=compute_89,code=sm_89 \
    -gencode arch=compute_90,code=sm_90 -include "$out/svml_tables.hpp" \
    -o "$out/device_check" "$src/device_check.cu" -lmvec
}

case "${1:-}" in
  --nvcc) nvcc_build; echo "libm_bits: $out/device_check built (sm_89, sm_90); not run"; exit 0 ;;
  --device)
    # AGENTS.md, "Running on the GPU": a GPU run needs the owner at the terminal. The check sits
    # here, next to the run, so no variable set by the caller skips it.
    if [ ! -t 0 ]; then
      echo "libm_bits: --device runs on the GPU and needs a supervising human at a terminal" >&2
      exit 7
    fi
    printf 'libm_bits: --device runs device_check on GPU 0. Are you supervising this run? Type yes: '
    read -r answer
    [ "$answer" = yes ] || { echo "libm_bits: not confirmed; nothing ran" >&2; exit 7; }
    nvcc_build
    exec "$out/device_check" "${@:2}"
    ;;
esac

# -ffp-contract=off: glibc_mathf.hpp fuses only where it calls ::fma.
g++ -std=c++20 -O2 -ffp-contract=off -Wall -Wextra -Wpedantic -pthread \
  -include "$out/svml_tables.hpp" -o "$out/libm_bits" "$src/libm_bits.cpp" -lquadmath -lmvec -lm

run() { nice -n 19 "$out/libm_bits" "$@"; }

case "${1:-}" in
  --self-test)
    run probe
    run ww logf fma 2 >/dev/null && echo "libm_bits: self-test ok"
    ;;
  all)
    t=${2:-4} rc=0
    run probe
    # The verdict is the fma transcription's and correct rounding's; the sse2 build and libmvec
    # are expected to differ, and their counts are the data.
    for f in expf logf sincosf powf mvec_expf mvec_powf exp; do
      run ww "$f" fma "$t" || rc=1
      run ww "$f" plain "$t" || true
    done
    # Correctly rounded: one build.
    for f in tanhf log10f atan2f tanh; do run ww "$f" fma "$t" || rc=1; done
    for f in tanhf log10f atan2f; do run cr "$f" "$t" || rc=1; done
    for f in expf powf; do run mvec "$f" "$t" || true; done
    run fingerprint
    exit "$rc"
    ;;
  "") sed -n '2,22p' "${BASH_SOURCE[0]}"; exit 64 ;;
  *) run "$@" ;;
esac
