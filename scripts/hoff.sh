#!/usr/bin/env bash
# hoff — run WAVEWATCH three ways. WW3's Fortran comes only from the WW3/ submodule (the
# h0ffmann/WW3 branch hoff/kokkos-snl1) and WW4's C++ only from the WW4/ submodule (h0ffmann/WW4,
# develop); this repository holds the Kokkos port and nothing of either model's source:
#
#   --ww3    WW3 as NOAA writes it: the lab switch, the Fortran W3SNL1.
#   --hoff   WW3 built with the KOKKOS switch and linked to this repository's Kokkos port
#            (kokkos/, installed as the ww_kokkos package); W3SNL1 runs on the Kokkos backend.
#   --ww4    WAVEWATCH IV, built from the WW4/ submodule. It runs only when every part the case
#            needs is ported to WW4 (scripts/ww4_parts.py): if one is not, it exits 5 and says
#            which, before building anything.
#
#   scripts/hoff.sh --ww3  smoke|full
#   scripts/hoff.sh --hoff smoke|full [--backend openmp|cuda]
#   scripts/hoff.sh --ww4  smoke|full
#   add --build-only to build and stop before the run (a machine without the GPU can still compile)
#   scripts/hoff.sh --self-test
#
# Exit codes: 0 pass; 1 a run or a comparison failed; 3 a submodule is missing or on the wrong
# branch; 4 an NVIDIA GPU whose driver does not answer (--hoff); 5 a part the case needs is not
# ported to WW4 (--ww4); 6 WW4 has every part but this repository has no WW4 input for the case.
#
# Cases:
#   smoke  regtest ww3_ts1 (one point, ST4 + NL1, NK=36 NTH=24, 10 model days; seconds on a CPU)
#   full   kokkos/tests/fixtures/bench_small (120 x 80, 9 520 sea points, NK=24 NTH=24, 12 hours)
#
# --hoff runs the case twice with the same executable, WW_KOKKOS_SNL1=0 (the Fortran DIA) and
# WW_KOKKOS_SNL1=1 (the Kokkos DIA), and compares the gridded output with nccmp-tol: the L2
# replay of kokkos/PORT_STATUS.md. It fails when a judged field fails or "Kokkos DIA (W3SNL1)
# enabled." is missing from the second run's log. --ww3 runs the case once and fails when
# ww3_shel does not reach "End of program".
#
# The backend defaults to cuda when `nvidia-smi -L` lists a GPU, else openmp. The script enters
# the pinned toolchain itself (nix-config/labs/pratico: #ww3, or #cuda for the CUDA backend).
# Builds go to build/<ww3|hoff-BACKEND>/, runs to build/runs/<mode>-<case>[-BACKEND]/.
#
# WW3 is built with its own Release flags in both modes. The two runs of --hoff agree to the
# tolerances of kokkos/tools/nccmp-tol/tolerances.txt; on 2026-10-14 they were not byte-identical
# (ww3_ts1: hs within 1e-7 relative), and compiling the Fortran with -ffp-contract=off changed
# nothing (docs/PORT_ASSESSMENT_202610.md).
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
pratico="$root/nix-config/labs/pratico"

usage() { sed -n '2,/^set -euo/{/^set -euo/!p}' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 64; }

mode="" case_name="" backend="${HOFF_BACKEND:-auto}" build_only=""
while [ $# -gt 0 ]; do
  case "$1" in
    --ww3|--hoff|--ww4) mode="${1#--}" ;;
    smoke|full) case_name="$1" ;;
    --backend) backend="${2:?--backend needs openmp or cuda}"; shift ;;
    --build-only) build_only=--build-only ;;
    --self-test) mode=self-test ;;
    -h|--help) usage ;;
    *) echo "hoff: unknown argument: $1" >&2; usage ;;
  esac
  shift
done

if [ "$mode" = self-test ]; then
  out="$("${BASH_SOURCE[0]}" --help 2>&1 || true)"
  if ! grep -q -- "--ww3" <<<"$out" || ! grep -q -- "--hoff" <<<"$out" || ! grep -q -- "--ww4" <<<"$out"; then echo "hoff: help text lost" >&2; exit 1; fi
  if "${BASH_SOURCE[0]}" --bogus >/dev/null 2>&1; then echo "hoff: accepted a bad argument" >&2; exit 1; fi
  echo "hoff self-test: ok"
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
  echo "hoff: this machine has an NVIDIA GPU, and nvidia-smi cannot reach its driver." >&2
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
  echo "      To run on the CPU anyway: --backend openmp (just smoke-hoff openmp)." >&2
}
has_nvidia_gpu() { lspci 2>/dev/null | grep -qiE 'VGA.*NVIDIA|3D controller.*NVIDIA'; }

# --ww4: what WW4 has decides whether there is anything to build. Checked before the toolchain.
if [ "$mode" = ww4 ] && [ -z "${HOFF_IN_SHELL:-}" ] && [ -z "$build_only" ]; then
  python3 "$root/scripts/ww4_parts.py" "$case_name" || exit $?
fi

# The backend matters to --hoff only: --ww3 and --ww4 run on the CPU.
if [ "$mode" != hoff ]; then backend=openmp
elif [ "$backend" = auto ]; then
  if nvidia-smi -L >/dev/null 2>&1; then backend=cuda
  elif has_nvidia_gpu; then gpu_doctor; exit 4
  else backend=openmp; fi
fi
if [ "$mode" = hoff ] && [ "$backend" = cuda ] && [ -z "${HOFF_IN_SHELL:-}" ] && ! nvidia-smi -L >/dev/null 2>&1 && [ -z "$build_only" ]; then
  gpu_doctor; exit 4
fi
case "$backend" in openmp|cuda) ;; *) echo "hoff: backend is openmp or cuda, not $backend" >&2; exit 64 ;; esac

# Enter the pinned toolchain once, then continue inside it.
if [ -z "${HOFF_IN_SHELL:-}" ]; then
  shell="ww3"; [ "$mode" = hoff ] && [ "$backend" = cuda ] && shell="cuda"
  exec env HOFF_IN_SHELL=1 HOFF_BACKEND="$backend" \
    nix develop "$pratico#$shell" --command bash "${BASH_SOURCE[0]}" "--$mode" "$case_name" --backend "$backend" $build_only
fi

# --- WW4 (--ww4) -----------------------------------------------------------------------------
if [ "$mode" = ww4 ]; then
  ww4="$root/WW4"
  [ -d "$ww4/src/ww4_core" ] || { echo "hoff: WW4/ is empty: git submodule update --init WW4" >&2; exit 3; }
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
  [ -x "$exe" ] || { echo "hoff: WW4 built, but $exe is not there" >&2; exit 1; }
  echo ">> WW4 ($(git -C "$ww4" rev-parse --short HEAD)) built: $exe"
  [ -z "$build_only" ] || exit 0
  # Reached only when scripts/ww4_parts.py found every part ported.
  if [ ! -d "$root/kokkos/tests/fixtures/ww4/$case_name" ]; then
    echo "hoff: WW4 has every part --ww4 $case_name needs, but there is no WW4 input for it yet (kokkos/tests/fixtures/ww4/$case_name/)" >&2
    exit 6
  fi
  d="$root/build/runs/ww4-$case_name"
  [[ "$d" == "$root/build/runs/"* ]] && rm -rf -- "$d"
  mkdir -p "$d" && cp -r "$root/kokkos/tests/fixtures/ww4/$case_name"/. "$d"/
  (cd "$d" && "$exe" > ww4_standalone.out 2>&1) || { tail -20 "$d/ww4_standalone.out" >&2; exit 1; }
  echo ">> --ww4 $case_name: ww4_standalone finished; output in $d"
  exit 0
fi

ww3="$root/WW3"
[ -f "$ww3/model/src/CMakeLists.txt" ] || { echo "hoff: WW3/ is empty: git submodule update --init WW3" >&2; exit 3; }
if [ "$mode" = hoff ] && [ ! -f "$ww3/model/src/w3kokkosmd.F90" ]; then
  echo "hoff: WW3/ is not the hoff/kokkos-snl1 branch of h0ffmann/WW3 (no model/src/w3kokkosmd.F90):" >&2
  echo "      git -C WW3 fetch origin hoff/kokkos-snl1 && git -C WW3 switch hoff/kokkos-snl1" >&2
  exit 3
fi
jobs="${JOBS:-$(nproc)}"

# --- the Kokkos package (--hoff), and nccmp-tol, which every --hoff verdict needs ----------
preset="${backend}-release"
kbuild="$root/kokkos/build/$preset"
kprefix="$kbuild/install"
if [ "$mode" = hoff ]; then
  [ -f "$kbuild/CMakeCache.txt" ] || cmake -S "$root/kokkos" --preset "$preset" >/dev/null
  cmake --build "$kbuild" -j"$jobs" >/dev/null
  cmake --install "$kbuild" --prefix "$kprefix" >/dev/null
  echo ">> ww_kokkos ($preset) installed in $kprefix"
fi

# --- WW3 ------------------------------------------------------------------------------------
if [ "$mode" = ww3 ]; then bdir="$root/build/ww3"; else bdir="$root/build/hoff-$backend"; fi
mkdir -p "$bdir"
switch_file="$bdir/switch"
switch_now="$(cat "$root/switches/switch_lab_shrd")"
[ "$mode" = hoff ] && switch_now="$switch_now KOKKOS"
# A changed switch needs a fresh configure: WW3 bakes it into the preprocessed sources.
if [ ! -f "$switch_file" ] || [ "$(cat "$switch_file")" != "$switch_now" ]; then
  printf '%s\n' "$switch_now" > "$switch_file"
  [ -d "$bdir/ww3" ] && [[ "$bdir/ww3" == "$root/build/"* ]] && rm -rf -- "$bdir/ww3"
fi
if [ ! -f "$bdir/ww3/CMakeCache.txt" ]; then
  args=(-S "$ww3" -B "$bdir/ww3" -DSWITCH="$switch_file" -DCMAKE_BUILD_TYPE=Release)
  if [ "$mode" = hoff ]; then
    args+=(-DWW_KOKKOS=ON -DCMAKE_PREFIX_PATH="$kprefix;${CMAKE_PREFIX_PATH:-}")
    # Kokkos' CUDA package wants the compiler it was built with.
    [ "$backend" = cuda ] && args+=(-DCMAKE_CXX_COMPILER=nvcc_wrapper)
  fi
  cmake "${args[@]}" > "$bdir/configure.log" 2>&1 || { tail -30 "$bdir/configure.log"; exit 1; }
fi
cmake --build "$bdir/ww3" -j"$jobs" > "$bdir/build.log" 2>&1 || { grep -E -A8 "Error|error:" "$bdir/build.log" | head -60; exit 1; }
bin="$bdir/ww3/bin"
echo ">> WW3 --$mode built in $bdir/ww3"
[ -z "$build_only" ] || exit 0

# --- the case ---------------------------------------------------------------------------------
prepare() { # <dir>: copy the case's inputs into <dir> and make their paths local
  local d="$1"
  mkdir -p "$d"
  if [ "$case_name" = smoke ]; then
    cp "$ww3"/regtests/ww3_ts1/input/* "$d"/
    cp "$d/ww3_grid_ST4_T475.nml" "$d/ww3_grid.nml"   # the setup docs/data's profile used
    sed -i 's#\.\./input/#./#g' "$d"/*.nml "$d"/*.inp
  else
    cp "$root"/kokkos/tests/fixtures/bench_small/* "$d"/
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
  grep -q "End of program" "$d/ww3_shel.out" || { echo "hoff: ww3_shel did not finish in $d" >&2; return 1; }
  (cd "$d" && "$bin/ww3_ounf" > ww3_ounf.out 2>&1) || { echo "hoff: ww3_ounf failed in $d:" >&2; tail -20 "$d/ww3_ounf.out" >&2; return 1; }
  python3 -c "print(f'{$t1 - $t0:.2f}')"
}

runs="$root/build/runs"
if [ "$mode" = ww3 ]; then
  d="$runs/ww3-$case_name"
  [[ "$d" == "$runs/"* ]] && rm -rf -- "$d"
  prepare "$d"
  wall="$(run_model "$d" 0)"
  echo ">> --ww3 $case_name: ww3_shel finished in ${wall} s; output: $(cd "$d" && ls ww3.*.nc)"
  exit 0
fi

d0="$runs/hoff-$case_name-$backend/fortran"
d1="$runs/hoff-$case_name-$backend/kokkos"
[[ "$runs/hoff-$case_name-$backend" == "$runs/"* ]] && rm -rf -- "$runs/hoff-$case_name-$backend"
prepare "$d0"; prepare "$d1"
w0="$(run_model "$d0" 0)"
w1="$(run_model "$d1" 1)"
grep -q "Kokkos DIA (W3SNL1) enabled" "$d1/log.ww3" \
  || { echo "hoff: the Kokkos run did not enable the Kokkos DIA (see $d1/log.ww3)" >&2; exit 1; }
nc0="$(cd "$d0" && find . -maxdepth 1 -name 'ww3.*.nc' -printf '%f\n' | sort | head -1)"
echo ">> --hoff $case_name ($backend): ww3_shel ${w0} s with the Fortran DIA, ${w1} s with the Kokkos DIA"
if cmp -s "$d0/$nc0" "$d1/$nc0"; then echo ">> the two outputs are byte-identical"; fi
"$kbuild/tools/nccmp-tol/nccmp-tol" "$d0/$nc0" "$d1/$nc0"
