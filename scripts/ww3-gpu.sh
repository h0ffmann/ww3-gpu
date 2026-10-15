#!/usr/bin/env bash
# ww3-gpu — run WAVEWATCH three ways. WW3's Fortran comes only from the WW3/ submodule (the
# h0ffmann/WW3 branch hoff/kokkos-snl1) and WW4's C++ only from the WW4/ submodule (h0ffmann/WW4,
# develop); this repository holds the Kokkos port and nothing of either model's source:
#
#   --ww3    WW3 as NOAA writes it: the lab switch, the Fortran W3SNL1.
#   --ww3-gpu   WW3 built with the KOKKOS switch and linked to this repository's Kokkos port
#            (kokkos/, installed as the ww_kokkos package); W3SNL1 runs on the Kokkos backend.
#   --ww4    WAVEWATCH IV, built from the WW4/ submodule. It runs only when every part the case
#            needs is ported to WW4 (scripts/ww4_parts.py): if one is not, it exits 5 and says
#            which, before building anything.
#
#   scripts/ww3-gpu.sh --ww3  smoke|full [--cpu]
#   scripts/ww3-gpu.sh --ww3-gpu smoke|full [--cpu|--gpu]
#   scripts/ww3-gpu.sh --ww4  smoke|full [--cpu]
#
# --cpu is the default everywhere. --gpu exists for --ww3-gpu only (Kokkos on CUDA): WW3's Fortran
# and WW4 have no GPU path here, so --ww3 --gpu and --ww4 --gpu are refused (exit 64).
# Hard rule (AGENTS.md): --ww3-gpu --gpu runs only under a human's supervision. It asks for a
# confirmation typed at a terminal and refuses without one (exit 7); --build-only compiles for
# the GPU without running and needs none. --ww3-gpu --cpu is the agents' regtest: its Kokkos run is
# compared pair to pair with --ww3, WW3 built from the same source without the KOKKOS switch.
#   add --build-only to build and stop before the run (a machine without the GPU can still compile)
#   scripts/ww3-gpu.sh --self-test
#
# Exit codes: 0 pass; 1 a run or a comparison failed; 3 a submodule is missing or on the wrong
# branch; 4 an NVIDIA GPU whose driver does not answer (--ww3-gpu --gpu); 5 a part the case needs is not
# ported to WW4 (--ww4); 6 WW4 has every part but this repository has no WW4 input for the case;
# 7 --ww3-gpu --gpu without a supervising human; 64 a bad argument or combination.
#
# Cases:
#   smoke  regtest ww3_ts1 (one point, ST4 + NL1, NK=36 NTH=24, 10 model days; seconds on a CPU)
#   full   kokkos/tests/fixtures/bench_small (120 x 80, 9 520 sea points, NK=24 NTH=24, 12 hours)
#
# --ww3-gpu runs the case twice with the same executable, WW_KOKKOS_SNL1=0 (the Fortran DIA) and
# WW_KOKKOS_SNL1=1 (the Kokkos DIA), and compares the gridded output with nccmp-tol: the L2
# replay of kokkos/PORT_STATUS.md. It fails when a judged field fails or "Kokkos DIA (W3SNL1)
# enabled." is missing from the second run's log. --ww3 runs the case once and fails when
# ww3_shel does not reach "End of program".
#
# --cpu is Kokkos' OpenMP backend, --gpu its CUDA backend. The script enters the pinned toolchain
# itself (nix-config/labs/pratico: #ww3, or #cuda for --gpu).
# Builds go to build/<ww3|ww3-gpu-BACKEND>/, runs to build/runs/<mode>-<case>[-BACKEND]/ ($WW3GPU_RUNS).
#
# WW3GPU_PREBUILT=<prefix> runs programs built beforehand and builds nothing, with no Nix: the
# Docker image (docker/Dockerfile) sets it. <prefix> holds ww3/bin and ww3-gpu-openmp/bin (WW3's
# programs), bin/nccmp-tol, cases/ (ww3_ts1, bench_small, ww4/<case>), and ww4/ (ww4_standalone
# and the parts report of each case, recorded when the image was built). CPU only.
#
# WW3 is built with its own Release flags in both modes. The two runs of --ww3-gpu agree to the
# tolerances of kokkos/tools/nccmp-tol/tolerances.txt; since 2026-10-15 they are byte-identical
# (docs/log/2026-10-14-snl1-bitwise-af11.md), and the script says so when they are.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
pratico="$root/nix-config/labs/pratico"
pre="${WW3GPU_PREBUILT:-}"
tols=()

usage() { sed -n '2,/^set -euo/{/^set -euo/!p}' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 64; }

mode="" case_name="" device="" build_only=""
while [ $# -gt 0 ]; do
  case "$1" in
    --ww3|--ww3-gpu|--ww4) mode="${1#--}" ;;
    --hoff) mode="ww3-gpu" ;;  # the old name, as the dated records in docs/log and docs/data cite it
    smoke|full) case_name="$1" ;;
    --cpu|--gpu) [ -z "$device" ] || [ "$device" = "${1#--}" ] || { echo "ww3-gpu: --cpu or --gpu, not both" >&2; exit 64; }
                 device="${1#--}" ;;
    --build-only) build_only=--build-only ;;
    --self-test) mode=self-test ;;
    -h|--help) usage ;;
    *) echo "ww3-gpu: unknown argument: $1" >&2; usage ;;
  esac
  shift
done

if [ "$mode" = self-test ]; then
  out="$("${BASH_SOURCE[0]}" --help 2>&1 || true)"
  if ! grep -q -- "--ww3" <<<"$out" || ! grep -q -- "--ww3-gpu" <<<"$out" || ! grep -q -- "--ww4" <<<"$out"; then echo "ww3-gpu: help text lost" >&2; exit 1; fi
  if "${BASH_SOURCE[0]}" --bogus >/dev/null 2>&1; then echo "ww3-gpu: accepted a bad argument" >&2; exit 1; fi
  for bad in "--ww3 smoke --gpu" "--ww4 smoke --gpu" "--ww3-gpu smoke --cpu --gpu"; do
    # shellcheck disable=SC2086
    "${BASH_SOURCE[0]}" $bad </dev/null >/dev/null 2>&1 && rc=0 || rc=$?
    [ "$rc" = 64 ] || { echo "ww3-gpu: '$bad' gave exit $rc, not 64" >&2; exit 1; }
  done
  # Without a terminal --ww3-gpu --gpu never runs: 7 (no supervisor) or 4 (no driver), before any
  # build; with prebuilt programs (the Docker image, CPU only) it is refused outright, 64.
  "${BASH_SOURCE[0]}" --ww3-gpu smoke --gpu </dev/null >/dev/null 2>&1 && rc=0 || rc=$?
  ok="4|7"; [ -z "${WW3GPU_PREBUILT:-}" ] || ok="64"
  [[ "$rc" =~ ^($ok)$ ]] || { echo "ww3-gpu: --ww3-gpu --gpu without a terminal gave exit $rc" >&2; exit 1; }
  echo "ww3-gpu self-test: ok"
  exit 0
fi
[ -n "$mode" ] && [ -n "$case_name" ] || usage

# gpu_doctor: a machine with an NVIDIA GPU whose driver does not answer is a fault to fix, not a
# reason to fall back to the CPU. The usual cause on Ubuntu: a new kernel was booted and the
# driver's module package for it was not installed (2026-10-14: kernel 7.0.0-38, the 595 modules
# installed for 7.0.0-31 only).
gpu_doctor() {
  local k pkg
  k="$(uname -r)"
  echo "ww3-gpu: this machine has an NVIDIA GPU, and nvidia-smi cannot reach its driver." >&2
  if ! find "/lib/modules/$k" -name 'nvidia.ko*' 2>/dev/null | grep -q .; then
    echo "      There is no nvidia kernel module for the running kernel, $k." >&2
    pkg="$(dpkg-query -W -f '${Package}\n' 'linux-modules-nvidia-*-generic-hwe-*' 2>/dev/null | head -1 || true)"
    if [ -n "$pkg" ]; then
      echo "      Fix: sudo apt install $pkg   (brings the module for $k), then sudo modprobe nvidia" >&2
    else
      echo "      Fix: install the NVIDIA module package for $k (ubuntu-drivers install), then sudo modprobe nvidia" >&2
    fi
  else
    echo "      The module for $k exists but is not loaded. Fix: sudo modprobe nvidia (or reboot)" >&2
  fi
  echo "      To run on the CPU instead: --cpu (just smoke-ww3-gpu)." >&2
}
has_nvidia_gpu() { lspci 2>/dev/null | grep -qiE 'VGA.*NVIDIA|3D controller.*NVIDIA'; }

device="${device:-cpu}"
if [ "$device" = gpu ] && [ "$mode" != ww3-gpu ]; then
  echo "ww3-gpu: --$mode has no GPU path (only --ww3-gpu runs Kokkos on CUDA); use --cpu" >&2
  exit 64
fi
if [ "$device" = gpu ]; then backend=cuda; else backend=openmp; fi
if [ -n "$pre" ] && [ "$device" = gpu ]; then
  echo "ww3-gpu: the prebuilt programs ($pre) are CPU only; there is no CUDA build in them" >&2
  exit 64
fi

# --ww4: what WW4 has decides whether there is anything to build. Checked before the toolchain.
if [ "$mode" = ww4 ] && [ -n "$pre" ]; then
  cat "$pre/ww4/parts-$case_name.txt"
  rc="$(cat "$pre/ww4/parts-$case_name.rc")"
  [ "$rc" = 0 ] || exit "$rc"
elif [ "$mode" = ww4 ] && [ -z "${WW3GPU_IN_SHELL:-}" ] && [ -z "$build_only" ]; then
  python3 "$root/scripts/ww4_parts.py" "$case_name" || exit $?
fi

# Hard rule: --ww3-gpu --gpu runs only with a human watching. Asked once, before the toolchain.
if [ "$device" = gpu ] && [ -z "$build_only" ] && [ -z "${WW3GPU_IN_SHELL:-}" ]; then
  nvidia-smi -L >/dev/null 2>&1 || { gpu_doctor; exit 4; }
  if [ ! -t 0 ]; then
    echo "ww3-gpu: --ww3-gpu --gpu runs only under a human's supervision (AGENTS.md hard rule): run it at a terminal" >&2
    exit 7
  fi
  printf 'ww3-gpu: --ww3-gpu %s --gpu runs the Kokkos port on %s. Are you supervising this run? Type yes: ' \
    "$case_name" "$(nvidia-smi -L | head -1)" >&2
  read -r answer
  [ "$answer" = yes ] || { echo "ww3-gpu: not confirmed; nothing ran" >&2; exit 7; }
fi

# Enter the pinned toolchain once, then continue inside it (not with prebuilt programs).
if [ -z "${WW3GPU_IN_SHELL:-}" ] && [ -z "$pre" ]; then
  shell="ww3"; [ "$mode" = ww3-gpu ] && [ "$backend" = cuda ] && shell="cuda"
  exec env WW3GPU_IN_SHELL=1 \
    nix develop "$pratico#$shell" --command bash "${BASH_SOURCE[0]}" "--$mode" "$case_name" "--$device" $build_only
fi

runs="${WW3GPU_RUNS:-$root/build/runs}"
if [ -n "$pre" ]; then
  smoke_in="$pre/cases/ww3_ts1/input" full_in="$pre/cases/bench_small" ww4_in="$pre/cases/ww4"
else
  smoke_in="$root/WW3/regtests/ww3_ts1/input" full_in="$root/kokkos/tests/fixtures/bench_small"
  ww4_in="$root/kokkos/tests/fixtures/ww4"
fi

# --- WW4 (--ww4) -----------------------------------------------------------------------------
if [ "$mode" = ww4 ] && [ -n "$pre" ]; then
  exe="$pre/ww4/ww4_standalone"
  [ -x "$exe" ] || { echo "ww3-gpu: this image has no ww4_standalone (WW4 did not build)" >&2; exit 1; }
  [ -d "$ww4_in/$case_name" ] || { echo "ww3-gpu: no WW4 input for --ww4 $case_name in $ww4_in" >&2; exit 6; }
  d="$runs/ww4-$case_name"
  rm -rf -- "$d" && mkdir -p "$d" && cp -r "$ww4_in/$case_name"/. "$d"/
  (cd "$d" && "$exe" > ww4_standalone.out 2>&1) || { tail -20 "$d/ww4_standalone.out" >&2; exit 1; }
  echo ">> --ww4 $case_name: ww4_standalone finished; output in $d"
  exit 0
fi
if [ "$mode" = ww4 ]; then
  ww4="$root/WW4"
  [ -d "$ww4/src/ww4_core" ] || { echo "ww3-gpu: WW4/ is empty: git submodule update --init WW4" >&2; exit 3; }
  # yaml-cpp and googletest are WW4's own submodules.
  [ -f "$ww4/externals/yaml-cpp/CMakeLists.txt" ] || git -C "$ww4" submodule update --init --depth 1 externals/yaml-cpp externals/googletest
  wb="$root/build/ww4"
  # yaml-cpp 0.8.0, which WW4 pins, asks for a CMake older than 3.5, which the toolchain's CMake 4
  # refuses; the policy floor lets it configure without a change to WW4's files.
  [ -f "$wb/Makefile" ] || [ -f "$wb/build.ninja" ] || cmake -S "$ww4" -B "$wb" -DCMAKE_BUILD_TYPE=Release \
    -DWW4_ENABLE_TESTING=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5 > "$root/build/ww4.configure.log" 2>&1 \
    || { tail -30 "$root/build/ww4.configure.log"; exit 1; }
  cmake --build "$wb" -j"${JOBS:-$(nproc)}" > "$root/build/ww4.build.log" 2>&1 || { grep -E -A8 "error" "$root/build/ww4.build.log" | head -60; exit 1; }
  # WW4's CMakeLists puts the program in its own exe/, which its .gitignore lists.
  exe="$ww4/exe/ww4_standalone"
  [ -x "$exe" ] || { echo "ww3-gpu: WW4 built, but $exe is not there" >&2; exit 1; }
  echo ">> WW4 ($(git -C "$ww4" rev-parse --short HEAD)) built: $exe"
  [ -z "$build_only" ] || exit 0
  # Reached only when scripts/ww4_parts.py found every part ported.
  if [ ! -d "$ww4_in/$case_name" ]; then
    echo "ww3-gpu: WW4 has every part --ww4 $case_name needs, but there is no WW4 input for it yet (kokkos/tests/fixtures/ww4/$case_name/)" >&2
    exit 6
  fi
  d="$runs/ww4-$case_name"
  [[ "$d" == "$runs/"* ]] && rm -rf -- "$d"
  mkdir -p "$d" && cp -r "$ww4_in/$case_name"/. "$d"/
  (cd "$d" && "$exe" > ww4_standalone.out 2>&1) || { tail -20 "$d/ww4_standalone.out" >&2; exit 1; }
  echo ">> --ww4 $case_name: ww4_standalone finished; output in $d"
  exit 0
fi

if [ -n "$pre" ]; then
  if [ "$mode" = ww3 ]; then bin="$pre/ww3/bin"; else bin="$pre/ww3-gpu-openmp/bin"; fi
  nccmp="$pre/bin/nccmp-tol"
  # nccmp-tol's default tolerance file is a path in the source tree, which the image does not have.
  tols=("$pre/share/tolerances.txt")
  [ -x "$bin/ww3_shel" ] && [ -x "$nccmp" ] || { echo "ww3-gpu: $pre has no $bin/ww3_shel or bin/nccmp-tol" >&2; exit 3; }
  echo ">> WW3 --$mode prebuilt: $bin"
  [ -z "$build_only" ] || exit 0
else
  ww3="$root/WW3"
  [ -f "$ww3/model/src/CMakeLists.txt" ] || { echo "ww3-gpu: WW3/ is empty: git submodule update --init WW3" >&2; exit 3; }
  if [ "$mode" = ww3-gpu ] && [ ! -f "$ww3/model/src/w3kokkosmd.F90" ]; then
    echo "ww3-gpu: WW3/ is not the hoff/kokkos-snl1 branch of h0ffmann/WW3 (no model/src/w3kokkosmd.F90):" >&2
    echo "      git -C WW3 fetch origin hoff/kokkos-snl1 && git -C WW3 switch hoff/kokkos-snl1" >&2
    exit 3
  fi
  jobs="${JOBS:-$(nproc)}"

  # A nix-config bump changes the compilers and libraries a CMake cache has baked in, and a stale
  # cache links the new libraries against the old glibc. Each build directory keeps the toolchain
  # it was configured with and is rebuilt from scratch when that changes.
  toolchain="$(readlink -f "$(command -v gfortran)") $(readlink -f "$(command -v c++)") ${CMAKE_PREFIX_PATH:-}"
  fresh_for_toolchain() { # <dir under build/ or kokkos/build/>
    if [ -d "$1" ] && [ "$(cat "$1/toolchain" 2>/dev/null)" != "$toolchain" ]; then
      [[ "$1" == "$root/build/"* || "$1" == "$root/kokkos/build/"* ]] && rm -rf -- "$1"
    fi
    mkdir -p "$1"
    printf '%s\n' "$toolchain" > "$1/toolchain"
  }

  # --- the Kokkos package (--ww3-gpu), and nccmp-tol, which every --ww3-gpu verdict needs ----------
  preset="${backend}-release"
  kbuild="$root/kokkos/build/$preset"
  kprefix="$kbuild/install"
  if [ "$mode" = ww3-gpu ]; then
    fresh_for_toolchain "$kbuild"
    [ -f "$kbuild/CMakeCache.txt" ] || cmake -S "$root/kokkos" --preset "$preset" >/dev/null
    cmake --build "$kbuild" -j"$jobs" >/dev/null
    cmake --install "$kbuild" --prefix "$kprefix" >/dev/null
    echo ">> ww_kokkos ($preset) installed in $kprefix"
  fi

  # --- WW3 ------------------------------------------------------------------------------------
  if [ "$mode" = ww3 ]; then bdir="$root/build/ww3"; else bdir="$root/build/ww3-gpu-$backend"; fi
  fresh_for_toolchain "$bdir"
  switch_file="$bdir/switch"
  switch_now="$(cat "$root/switches/switch_lab_shrd")"
  [ "$mode" = ww3-gpu ] && switch_now="$switch_now KOKKOS"
  # A changed switch needs a fresh configure: WW3 bakes it into the preprocessed sources.
  if [ ! -f "$switch_file" ] || [ "$(cat "$switch_file")" != "$switch_now" ]; then
    printf '%s\n' "$switch_now" > "$switch_file"
    [ -d "$bdir/ww3" ] && [[ "$bdir/ww3" == "$root/build/"* ]] && rm -rf -- "$bdir/ww3"
  fi
  if [ ! -f "$bdir/ww3/CMakeCache.txt" ]; then
    args=(-S "$ww3" -B "$bdir/ww3" -DSWITCH="$switch_file" -DCMAKE_BUILD_TYPE=Release)
    if [ "$mode" = ww3-gpu ]; then
      args+=(-DWW_KOKKOS=ON -DCMAKE_PREFIX_PATH="$kprefix;${CMAKE_PREFIX_PATH:-}")
      # Kokkos' CUDA package wants the compiler it was built with.
      [ "$backend" = cuda ] && args+=(-DCMAKE_CXX_COMPILER=nvcc_wrapper)
    fi
    cmake "${args[@]}" > "$bdir/configure.log" 2>&1 || { tail -30 "$bdir/configure.log"; exit 1; }
  fi
  cmake --build "$bdir/ww3" -j"$jobs" > "$bdir/build.log" 2>&1 || { grep -E -A8 "Error|error:" "$bdir/build.log" | head -60; exit 1; }
  bin="$bdir/ww3/bin"
  nccmp="$kbuild/tools/nccmp-tol/nccmp-tol"
  echo ">> WW3 --$mode built in $bdir/ww3"
  [ -z "$build_only" ] || exit 0
fi

# --- the case ---------------------------------------------------------------------------------
prepare() { # <dir>: copy the case's inputs into <dir> and make their paths local
  local d="$1"
  mkdir -p "$d"
  if [ "$case_name" = smoke ]; then
    cp "$smoke_in"/* "$d"/
    cp "$d/ww3_grid_ST4_T475.nml" "$d/ww3_grid.nml"   # the setup docs/data's profile used
    sed -i 's#\.\./input/#./#g' "$d"/*.nml "$d"/*.inp
  else
    cp "$full_in"/* "$d"/
    # The fixture writes no field (stride 0: it was made for timing); write HS every hour, so
    # ww3_ounf has a file to compare.
    sed -i "s#^  DATE%FIELD = .*#  DATE%FIELD = '20200101 000000' '3600' '20200101 120000'#" "$d/ww3_shel.nml"
  fi
}
run_model() { # <dir> <WW_KOKKOS_SNL1>: grid, strt, shel, ounf; prints ww3_shel's wall time
  # Called inside $(...): every diagnostic goes to stderr, or it would vanish into the variable.
  local d="$1" k="$2" t0 t1
  (cd "$d" && "$bin/ww3_grid" > ww3_grid.out 2>&1) || { tail -20 "$d/ww3_grid.out" >&2; return 1; }
  if [ -f "$d/ww3_strt.inp" ] || [ -f "$d/ww3_strt.nml" ]; then
    (cd "$d" && "$bin/ww3_strt" > ww3_strt.out 2>&1) || { tail -20 "$d/ww3_strt.out" >&2; return 1; }
  fi
  t0="$(date +%s.%N)"
  (cd "$d" && OMP_PROC_BIND="${OMP_PROC_BIND:-false}" WW_KOKKOS_SNL1="$k" "$bin/ww3_shel" > ww3_shel.out 2>&1) \
    || { grep -h "ww_kokkos:\|Kokkos::" "$d/ww3_shel.out" >&2; tail -20 "$d/ww3_shel.out" >&2; return 1; }
  t1="$(date +%s.%N)"
  grep -q "End of program" "$d/ww3_shel.out" || { echo "ww3-gpu: ww3_shel did not finish in $d" >&2; return 1; }
  (cd "$d" && "$bin/ww3_ounf" > ww3_ounf.out 2>&1) || { echo "ww3-gpu: ww3_ounf failed in $d:" >&2; tail -20 "$d/ww3_ounf.out" >&2; return 1; }
  awk -v a="$t0" -v b="$t1" 'BEGIN { printf "%.2f\n", b - a }'
}

if [ "$mode" = ww3 ]; then
  d="$runs/ww3-$case_name"
  [[ "$d" == "$runs/"* ]] && rm -rf -- "$d"
  prepare "$d"
  wall="$(run_model "$d" 0)"
  echo ">> --ww3 $case_name: ww3_shel finished in ${wall} s; output: $(cd "$d" && ls ww3.*.nc)"
  exit 0
fi

d0="$runs/ww3-gpu-$case_name-$backend/fortran"
d1="$runs/ww3-gpu-$case_name-$backend/kokkos"
[[ "$runs/ww3-gpu-$case_name-$backend" == "$runs/"* ]] && rm -rf -- "$runs/ww3-gpu-$case_name-$backend"
prepare "$d0"; prepare "$d1"
w0="$(run_model "$d0" 0)"
w1="$(run_model "$d1" 1)"
grep -q "Kokkos DIA (W3SNL1) enabled" "$d1/log.ww3" \
  || { echo "ww3-gpu: the Kokkos run did not enable the Kokkos DIA (see $d1/log.ww3)" >&2; exit 1; }
nc0="$(cd "$d0" && find . -maxdepth 1 -name 'ww3.*.nc' -printf '%f\n' | sort | head -1)"
echo ">> --ww3-gpu $case_name ($backend): ww3_shel ${w0} s with the Fortran DIA, ${w1} s with the Kokkos DIA"
if cmp -s "$d0/$nc0" "$d1/$nc0"; then echo ">> the two outputs are byte-identical"; fi
echo ">> the Kokkos DIA against the Fortran DIA, same executable:"
"$nccmp" "$d0/$nc0" "$d1/$nc0" "${tols[@]}"
# --ww3-gpu --cpu is the agents' regtest: pair to pair with --ww3, WW3 from the same source built
# without the KOKKOS switch.
if [ "$device" = cpu ]; then
  bash "${BASH_SOURCE[0]}" --ww3 "$case_name" --cpu
  ref="$runs/ww3-$case_name/$nc0"
  [ -f "$ref" ] || { echo "ww3-gpu: the --ww3 run wrote no $nc0" >&2; exit 1; }
  echo ">> the Kokkos DIA against --ww3 (WW3 as written), pair to pair:"
  "$nccmp" "$ref" "$d1/$nc0" "${tols[@]}"
fi
