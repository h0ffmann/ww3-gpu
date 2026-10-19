#!/usr/bin/env bash
# restart_pair.sh: does a run restarted from its own restart file give the bytes of the run it
# was cut from, with the resident Kokkos preset, on a SHRD case? Gap 10 of
# docs/PRODUCTION_READINESS_202610.md. Every restart-reproduction entry of WW3's matrix.base at
# the WW3/ pin (rstrt_b4b: ww3_tp2.17, ww3_ufs1.1) is MPI, where no resident part runs
# (ADR-0017), so this is the smallest equivalent on the full case of scripts/ww3-gpu.sh.
#
#   scripts/restart_pair.sh            run the four runs below and compare
#   scripts/restart_pair.sh --self-test check the namelist edits on a scratch copy; runs no model
#
# It needs the executable that `WW3GPU_SWITCH=switch_lab_shrd bash scripts/ww3-gpu.sh --ww3-gpu
# full --cpu --kokkos resident --build-only` builds in build/ww3-gpu-openmp/ww3/bin, and stops
# when that build's switch is not SHRD with KOKKOS. Inside the pinned ww3 shell (just ww3).
#
# Case: kokkos/tests/fixtures/bench_small (120 x 80, 9 520 sea points, 12 hours), with nine
# fields every hour from 06:00 to 12:00 and a restart at 06:00 and at 12:00. Runs, one executable:
#   fortran/a, kokkos/a   00:00 to 12:00 from the cold start
#   fortran/b, kokkos/b   06:00 to 12:00 from <same mode>/a's 06:00 restart, copied to restart.ww3
# The Fortran runs set WW_KOKKOS_ALL=0; the Kokkos runs WW_KOKKOS_PRESET=resident,
# WW_KOKKOS_STRICT=1 and every *_COUNT=1. Both keep WW_KOKKOS_VA_POISON and WW_KOKKOS_VA_CHECK
# from the caller. Compared with cmp: out_grd.ww3 and the 12:00 restart of b against a (restart
# reproduction), and every file of kokkos/x against fortran/x (bit for bit).
#
# Exit codes: 0 every comparison equal; 1 a run failed or a file differs; 3 no SHRD KOKKOS build;
# 64 a bad argument. A Kokkos difference that the Fortran pair shows too is printed as WW3's own.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
case_in="$root/kokkos/tests/fixtures/bench_small"
bdir="${WW3GPU_RESTART_BUILD:-$root/build/ww3-gpu-openmp}"
runs="${WW3GPU_RUNS:-$root/build/runs}/restart-pair"
fields="HS LM T02 T0M1 T01 FP DIR SPR DP"

# edit_case <dir> <start>: the fixture's ww3_shel.nml with <start>, the fields and the restarts.
edit_case() {
  local d="$1" start="$2"
  cp "$case_in"/* "$d"/
  sed -i -e "s#^  DOMAIN%START = .*#  DOMAIN%START = '20200101 $start'#" \
    -e "s#^  TYPE%FIELD%LIST = .*#  TYPE%FIELD%LIST = '$fields'#" \
    -e "s#^  DATE%FIELD = .*#  DATE%FIELD = '20200101 060000' '3600' '20200101 120000'\n  DATE%RESTART = '20200101 060000' '21600' '20200101 120000'#" \
    "$d/ww3_shel.nml"
}

if [ "${1:-}" = --self-test ]; then
  t="$(mktemp -d)"
  trap 'rm -rf -- "$t"' EXIT
  edit_case "$t" 060000
  for want in "  DOMAIN%START = '20200101 060000'" "  TYPE%FIELD%LIST = '$fields'" \
    "  DATE%FIELD = '20200101 060000' '3600' '20200101 120000'" \
    "  DATE%RESTART = '20200101 060000' '21600' '20200101 120000'" "  DOMAIN%STOP  = '20200101 120000'"; do
    grep -qxF "$want" "$t/ww3_shel.nml" || { echo "restart_pair: self-test: no line '$want'" >&2; exit 1; }
  done
  [ "$(grep -c 'DATE%' "$t/ww3_shel.nml")" = 2 ] || { echo "restart_pair: self-test: DATE lines" >&2; exit 1; }
  echo "restart_pair: self-test passed"
  exit 0
fi
[ $# = 0 ] || { sed -n '2,/^set -euo/{/^set -euo/!p}' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 64; }

bin="$bdir/ww3/bin"
sw="$(cat "$bdir/switch" 2>/dev/null || true)"
[ -x "$bin/ww3_shel" ] && [[ " $sw " == *" SHRD "* ]] && [[ " $sw " == *" KOKKOS "* ]] \
  && [[ " $sw " != *" OMPG "* ]] && [[ " $sw " != *" MPI "* ]] || {
  echo "restart_pair: $bin is not a SHRD KOKKOS build; run WW3GPU_SWITCH=switch_lab_shrd bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident --build-only" >&2
  exit 3
}

unset_parts=(-u WW_KOKKOS_PRESET -u WW_KOKKOS_STRICT -u WW_KOKKOS_ALL)
counts=()
for f in "$root"/kokkos/src/fortran_iface/parts.d/*.txt; do
  while read -r key val _; do
    case "$key" in PART) unset_parts+=(-u "WW_KOKKOS_$val") ;; COUNT) counts+=("$val=1") ;; esac
  done < "$f"
done
preset="$(awk '$1 == "PRESET" && $2 == "resident" { print $3 }' "$root/kokkos/src/fortran_iface/presets.txt")"
[ -n "$preset" ] || { echo "restart_pair: no resident preset in presets.txt" >&2; exit 1; }

run() { # <mode> <a|b>
  local d="$runs/$1/$2" start=000000
  local -a flags=(WW_KOKKOS_ALL=0)
  [ "$1" = fortran ] || flags=(WW_KOKKOS_PRESET=resident WW_KOKKOS_STRICT=1 "${counts[@]}")
  mkdir -p "$d"
  if [ "$2" = b ]; then
    start=060000
    cp "$runs/$1/a/restart001.ww3" "$d/restart.ww3"
  fi
  edit_case "$d" "$start"
  (cd "$d" && "$bin/ww3_grid" > ww3_grid.out 2>&1) || { tail -20 "$d/ww3_grid.out" >&2; return 1; }
  (cd "$d" && env "${unset_parts[@]}" OMP_PROC_BIND="${OMP_PROC_BIND:-false}" "${flags[@]}" \
     "$bin/ww3_shel" > ww3_shel.out 2>&1) || { tail -20 "$d/ww3_shel.out" >&2; return 1; }
  grep -q "End of program" "$d/ww3_shel.out" || { echo "restart_pair: ww3_shel did not finish in $d" >&2; return 1; }
  if [ "$1" = kokkos ]; then
    local p line
    line="$(grep -h "Kokkos parts" "$d/log.ww3" | head -1 || true)"
    for p in ${preset//,/ }; do
      [[ "$line" == *"$p=K"* ]] || { echo "restart_pair: $d: $p is not K in '$line'" >&2; return 1; }
    done
  fi
  echo ">> $1/$2: ww3_shel from 20200101 $start finished; $(cd "$d" && find . -maxdepth 1 -name 'restart0*.ww3' -printf '%f ' | sort)"
}

same() { # <file a> <file b> <label>: prints the verdict, returns 1 on a difference
  if [ ! -f "$1" ] || [ ! -f "$2" ]; then echo ">> $3: missing ($1 or $2)"; return 1; fi
  if cmp -s "$1" "$2"; then echo ">> $3: identical ($(stat -c %s "$1") bytes)"; else echo ">> $3: DIFFERS ($(cmp "$1" "$2" | head -1))"; return 1; fi
}

[[ "$runs" == */restart-pair ]] && rm -rf -- "$runs"
for m in fortran kokkos; do run "$m" a; run "$m" b; done

# The 12:00 restart: the last restart a run wrote (a writes 06:00 and 12:00; b may write 06:00 too).
last() { find "$1" -maxdepth 1 -name 'restart0*.ww3' -printf '%f\n' | sort | tail -1; }
bad=0 fbad=0
echo ">> restart reproduction (b against a):"
for m in fortran kokkos; do
  r=0
  same "$runs/$m/a/out_grd.ww3" "$runs/$m/b/out_grd.ww3" "$m out_grd.ww3 06:00-12:00" || r=1
  same "$runs/$m/a/$(last "$runs/$m/a")" "$runs/$m/b/$(last "$runs/$m/b")" "$m 12:00 restart" || r=1
  if [ "$m" = fortran ]; then fbad=$r; else bad=$((bad | r)); fi
done
echo ">> bit for bit (kokkos against fortran, same executable):"
for x in a b; do
  for f in out_grd.ww3 $(find "$runs/fortran/$x" -maxdepth 1 -name 'restart0*.ww3' -printf '%f\n' | sort); do
    same "$runs/fortran/$x/$f" "$runs/kokkos/$x/$f" "$x/$f" || bad=1
  done
done
for x in a b; do
  grep -h "^ww_kokkos: VA \(host-to-device\|device-to-host\) copies\|calls left on the Fortran\|still newer" \
    "$runs/kokkos/$x/ww3_shel.out" | sed "s/^/>> kokkos\/$x: /" || true
done
if [ "$bad" != 0 ] && [ "$fbad" != 0 ]; then
  echo ">> the Fortran pair does not reproduce its restart either: WW3's own behaviour, not the port's"
fi
[ "$bad" = 0 ] && [ "$fbad" = 0 ] || exit 1
echo ">> restart_pair: every comparison identical"
