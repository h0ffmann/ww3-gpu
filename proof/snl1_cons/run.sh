#!/usr/bin/env bash
# Pilot of docs/BITWISE_PROOF_202610.md: W3SNL1 section 1 (CONS), Fortran vs C++ port,
# swept over all 2^32 float32 values of KDMEAN under several flag sets.
#
# usage: proof/snl1_cons/run.sh [config ...]     (default: all five, about 1 min each on 4 cores)
#   parity    gfortran with WW3's GNU release flags  vs  g++ with the openmp-release + ww_kokkos flags
#   fixture   gfortran -O0 -g (the serial-debug build that wrote the fixture)  vs  the same port
#   fma-cxx   negative control: the port without -ffp-contract=off
#   fma-f90   the Fortran reference itself built with -march=x86-64-v3 (FMA available)
#   bend-exp  the port with exp evaluated as (float)exp((double)x), Bend 2's F32.exp lowering
#
# Needs gfortran and g++ (any recent GCC; `just ww3` provides both). No Kokkos, no CMake:
# each side is its own translation unit, so each gets exactly the flags named here.
#
# SPDX-License-Identifier: MIT
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
FIXTURE="$ROOT/kokkos/tests/fixtures/snl1_nk25_nth24.bin"
FC="${FC:-gfortran}"
CXX="${CXX:-g++}"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

# WW3's own flags for gfortran: model/src/CMakeLists.txt, GNU branch, plus CMake's Release -O3.
F_WW3="-g -fno-second-underscore -ffree-line-length-none -O3"
# kokkos/CMakePresets.json openmp-release + kokkos/src/ww_kokkos/CMakeLists.txt.
CXX_PORT="-std=c++20 -O3 -march=x86-64-v3 -ffp-contract=off"

run() {  # run <name> <fortran flags> <c++ flags>
  local name="$1" fflags="$2" cflags="$3"
  # shellcheck disable=SC2086  # the flag strings are meant to split
  "$FC" $fflags -c "$HERE/cons_ref.F90" -o "$OUT/ref_$name.o" -J "$OUT"
  # shellcheck disable=SC2086
  "$CXX" $cflags -c "$HERE/cons_port.cpp" -o "$OUT/port_$name.o"
  "$CXX" -std=c++20 -O2 -fopenmp "$HERE/sweep.cpp" "$OUT/ref_$name.o" "$OUT/port_$name.o" \
    -lgfortran -o "$OUT/sweep_$name"
  echo "== $name"
  echo "   fortran: $FC $fflags"
  echo "   c++:     $CXX $cflags"
  "$OUT/sweep_$name" "$FIXTURE" | sed 's/^/   /' || true
}

configs=("$@")
[ ${#configs[@]} -gt 0 ] || configs=(parity fixture fma-cxx fma-f90 bend-exp)

"$FC" --version | head -1
"$CXX" --version | head -1
ldd --version 2>/dev/null | head -1 || true
for c in "${configs[@]}"; do
  case "$c" in
    parity)   run parity   "$F_WW3"                     "$CXX_PORT" ;;
    fixture)  run fixture  "-O0 -g"                     "$CXX_PORT" ;;
    fma-cxx)  run fma-cxx  "$F_WW3"                     "-std=c++20 -O3 -march=x86-64-v3" ;;
    fma-f90)  run fma-f90  "$F_WW3 -march=x86-64-v3"    "$CXX_PORT" ;;
    bend-exp) run bend-exp "$F_WW3"                     "$CXX_PORT -DWW_EXP_VIA_DOUBLE" ;;
    *) echo "run.sh: unknown config '$c'" >&2; exit 2 ;;
  esac
done
