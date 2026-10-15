#!/usr/bin/env bash
# hoff_matrix — the Kokkos DIA (W3SNL1) through WW3's own regtest harness (WFIP-0003). Every
# entry of $WW3/regtests/bin/matrix.base whose switch has NL1, none of MPI, OMPG, OMPH, and whose
# line has no -p and no -m, is run twice with run_cmake_test on one executable built with the
# KOKKOS switch: WW_KOKKOS_SNL1=0 (the Fortran DIA) into work_F_<n>, WW_KOKKOS_SNL1=1 (the Kokkos
# DIA) into work_K_<n>. The pair is compared with WW3's test.comp (bytes) and with nccmp-tol
# (kokkos/tools/nccmp-tol/tolerances.txt) on every ww3.*.nc both runs wrote.
#
#   scripts/hoff_matrix.sh [--only <sel>]... [--jobs N] [--dry-run]
#   scripts/hoff_matrix.sh --list         the selected entries, "n test options", one per line
#   scripts/hoff_matrix.sh --self-test    the selection parser and the verdict, on fixtures
#   scripts/hoff_matrix.sh --fetch-data   download WW3's regtest data once into build/ww3-data/
#
# <sel> is an entry number from --list, a test (ww3_ts1) or a test and switch (ww3_ts1:ST4);
# --only is repeatable and takes a comma list. --jobs N runs N entries at once (default 1) and
# gives each OMP_NUM_THREADS=nproc/N unless OMP_NUM_THREADS is set. --dry-run prints the commands.
# CPU only (Kokkos on OpenMP): --gpu is refused (exit 64); GPU runs are the owner's (AGENTS.md).
#
# WW3/ is never written: the regtest inputs are staged in build/hoff-matrix/regtests/, and the
# run fails if `git -C WW3 status --porcelain` is not empty at the end. -o netcdf is added to an
# entry with no -o when its input directory has a ww3_ounf input. Data WW3 keeps off git (wind.nc
# of ww3_tp2.15, ...) comes from the tarball $WW3/model/bin/ww3_from_ftp.sh names: --fetch-data
# downloads it from that URL into build/ww3-data/<version>/, refuses an archive with a member
# outside data_regtests/ or a link, and extracts it there; staging then copies what that script's
# `cp -r data_regtests/... regtests/...` lines copy, for the staged tests only, and warns when a
# staged test needs the data and the cache is missing. The record is
# build/hoff-matrix/summary.tsv: one row per entry run (n, test, switch, built, ran, kokkos_log,
# test_comp as differing/compared files, nccmp_tol PASS/FAIL/n/a, judged variables, wall s, and the
# entry's matrix.base options, which tell apart entries of one test and switch).
# nccmp_tol is n/a when the test has no ww3_ounf input, and MISSING (a failure) when it has one
# and either run wrote no ww3.*.nc.
# test.comp differences are recorded, not fatal; since 2026-10-15 every entry has 0 (W3SNL1_KOKKOS.md).
#
# Exit codes: 0 no nccmp-tol FAIL, and every entry that ran with the Fortran DIA also ran with
# the Kokkos DIA and logged "Kokkos DIA (W3SNL1) enabled."; 1 otherwise, or WW3/ was written;
# 3 WW3/ missing or not the hoff/kokkos-snl1 branch; 64 a bad argument.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
pratico="$root/nix-config/labs/pratico"
ww3="$root/WW3"
out="$root/build/hoff-matrix"
stage="$out/regtests"

usage() { sed -n '2,/^set -euo/{/^set -euo/!p}' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 64; }

# select_entries <matrix.base> <regtests dir>: "test<TAB>options" for every kept entry, in order.
select_entries() {
  local base="$1" rt="$2" line test opts inp sw f prev
  local -a tok
  while IFS= read -r line; do
    tok=()
    read -r -a tok <<<"$line"
    [ "${#tok[@]}" -ge 2 ] || continue
    test="${tok[-1]}"
    unset 'tok[-1]'
    # shellcheck disable=SC2016 # the literal token matrix.base writes
    [ "${tok[-1]}" = '$ww3' ] && unset 'tok[-1]'
    inp=input sw="" prev=""
    for f in "${tok[@]}"; do
      case "$prev" in -i) inp="$f" ;; -s) sw="$f" ;; esac
      case "$f" in -p|-m) continue 2 ;; esac
      prev="$f"
    done
    f="$rt/$test/$inp/switch${sw:+_$sw}"
    [ -f "$f" ] || continue
    grep -qw NL1 "$f" || continue
    grep -qwE 'MPI|OMPG|OMPH' "$f" && continue
    opts="${tok[*]}"
    printf '%s\t%s\n' "$test" "$opts"
  done < <(awk '/^[[:space:]]*#/ {next}
                /echo "\$rtst .*" *>> *matrix\.body/ { s=$0; sub(/.*echo "\$rtst /,"",s); sub(/".*/,"",s); print s }' "$base")
}

# has_ounf <input dir>: the test post-processes to NetCDF (a ww3_ounf input). Not compgen: the
# toolchain's bash is built without programmable completion and has no compgen.
has_ounf() { [ -n "$(find "$1" -maxdepth 1 -name 'ww3_ounf*' -print -quit 2>/dev/null)" ]; }

opt_of() { # <flag> <options...>: the value of <flag>, empty when absent
  local flag="$1" prev="" f; shift
  for f in "$@"; do [ "$prev" = "$flag" ] && { printf '%s' "$f"; return; }; prev="$f"; done
}

# matrix_verdict <summary.tsv>: 0 when no nccmp-tol FAIL and no entry ran with the Fortran DIA
# but not with the Kokkos DIA (or ran without the Kokkos DIA's log line); 1 otherwise.
matrix_verdict() {
  awk -F'\t' '/^#/ || $1 == "n" {next}
              $8 == "FAIL" || $8 == "MISSING" {bad=1}
              $5 == "fortran-only" {bad=1}
              $5 == "yes" && $6 != "yes" {bad=1}
              END {exit bad ? 1 : 0}' "$1"
}

# judged_vars <nccmp-tol output>: the variables it judged, comma-separated.
judged_vars() { awk 'p && $1 != "nccmp-tol:" && $NF != "unlisted" {print $1} /^-+$/ {p=1}' "$1" | paste -sd, -; }

# ftp_copies <ww3_from_ftp.sh>: "src<TAB>dst" for each `cp -r data_regtests/<src> regtests/<dst>`.
ftp_copies() { sed -nE 's|^[[:space:]]*cp -r data_regtests/([^[:space:]]+)[[:space:]]+regtests/([^[:space:]]+)[[:space:]]*$|\1\t\2|p' "$1"; }

# tar_unsafe <tar -tvzf listing>: the members that are links or fall outside data_regtests/.
tar_unsafe() { awk '$1 !~ /^[-d]/ {print; next} {m=$6; for (i = 7; i <= NF; i++) m = m " " $i}
                    m !~ /^(\.\/)?data_regtests(\/|$)/ || m ~ /(^|\/)\.\.(\/|$)/ {print}' "$1"; }

# stage_data <cache> <stage> <test>: what ww3_from_ftp.sh copies into regtests/<test>/.
stage_data() {
  local cache="$1" st="$2" test="$3" src dst f n=0
  while IFS=$'\t' read -r src dst; do
    [ "${dst%%/*}" = "$test" ] || continue
    [ -d "$cache" ] || { echo "hoff_matrix: $test needs WW3's ftp data (ww3_from_ftp.sh); run --fetch-data" >&2; return 0; }
    case "$dst" in */) mkdir -p "$st/$dst" ;; *) mkdir -p "$st/$(dirname "$dst")" ;; esac
    for f in "$cache"/$src; do [ -e "$f" ] && cp -r "$f" "$st/$dst" && n=$((n + 1)); done
  done < <(ftp_copies "$ww3/model/bin/ww3_from_ftp.sh")
  [ "$n" = 0 ] || echo ">> $test: $n file(s) of WW3's ftp data staged"
}

# --- arguments -----------------------------------------------------------------------------
orig_args=("$@")
mode=run only=() dry="" jobs=1
while [ $# -gt 0 ]; do
  case "$1" in
    --list) mode=list ;;
    --fetch-data) mode=fetch ;;
    --self-test) mode=self-test ;;
    --dry-run) dry=1 ;;
    --only) [ $# -ge 2 ] || usage; IFS=, read -r -a sel <<<"$2"; only+=("${sel[@]}"); shift ;;
    --jobs) [[ "${2:-}" =~ ^[1-9][0-9]*$ ]] || { echo "hoff_matrix: --jobs takes a positive integer" >&2; exit 64; }
            jobs="$2"; shift ;;
    --gpu) echo "hoff_matrix: CPU only; --hoff --gpu runs only under the owner's supervision (AGENTS.md)" >&2; exit 64 ;;
    --cpu) ;;
    -h|--help) usage ;;
    *) echo "hoff_matrix: unknown argument: $1" >&2; usage ;;
  esac
  shift
done

if [ "$mode" = self-test ]; then
  t="$(mktemp -d)"; trap 'rm -rf -- "$t"' EXIT
  mkdir -p "$t/rt/bin" "$t/rt/a/input" "$t/rt/a/input2" "$t/rt/b/input"
  echo "F90 SHRD ST4 NL1 FLX0" > "$t/rt/a/input/switch_K1"
  echo "F90 SHRD ST4 NL3 FLX0" > "$t/rt/a/input/switch_N3"
  echo "F90 DIST MPI ST4 NL1"  > "$t/rt/a/input/switch_MP"
  echo "F90 SHRD OMPH ST4 NL1" > "$t/rt/a/input/switch_OH"
  echo "F90 SHRD ST4 NL1X"     > "$t/rt/a/input/switch_NX"
  echo "F90 SHRD ST4 NL1"      > "$t/rt/a/input2/switch"
  echo "F90 SHRD ST4 NL1"      > "$t/rt/b/input/switch_K1"
  cat > "$t/rt/bin/matrix.base" <<'EOF'
    echo "$rtst -s K1 -w work_K1 $ww3 a" >> matrix.body
    echo "$rtst -s K1 -w work_K1g -g G2 -N -o both $ww3 a" >> matrix.body
    echo "$rtst -s N3 -w work_N3 $ww3 a" >> matrix.body
    echo "$rtst -s MP -w work_MP $ww3 a" >> matrix.body
    echo "$rtst -s OH -w work_OH $ww3 a" >> matrix.body
    echo "$rtst -s NX -w work_NX $ww3 a" >> matrix.body
    echo "$rtst -s K1 -p $mpi -n $np -w work_P $ww3 b" >> matrix.body
    echo "$rtst -s K1 -m grdset -w work_M $ww3 b" >> matrix.body
    echo "$rtst -i input2 -w work_I $ww3 a" >> matrix.body
    echo "$rtst -s MISSING -w work_X $ww3 a" >> matrix.body
#   echo "$rtst -s K1 -w work_C $ww3 b" >> matrix.body
    echo "$rtst -s K1 -w work_B $ww3 b" >> matrix.body
EOF
  got="$(select_entries "$t/rt/bin/matrix.base" "$t/rt")"
  want="$(printf 'a\t-s K1 -w work_K1\na\t-s K1 -w work_K1g -g G2 -N -o both\na\t-i input2 -w work_I\nb\t-s K1 -w work_B')"
  [ "$got" = "$want" ] || { printf 'hoff_matrix: selection wrong; got:\n%s\nwant:\n%s\n' "$got" "$want" >&2; exit 1; }
  touch "$t/rt/a/input/ww3_ounf.nml"
  if ! has_ounf "$t/rt/a/input" || has_ounf "$t/rt/b/input"; then echo "hoff_matrix: has_ounf wrong" >&2; exit 1; fi
  [ "$(opt_of -w -s K1 -w work_Z -g G)" = work_Z ] && [ -z "$(opt_of -o -s K1)" ] || { echo "hoff_matrix: opt_of wrong" >&2; exit 1; }
  hdr=$'# fixture\nn\ttest\tswitch\tbuilt\tran\tkokkos_log\ttest_comp\tnccmp_tol\tjudged\twall_s'
  ok=$'1\ta\tK1\tyes\tyes\tyes\t3/9\tPASS\ths,fp\t10'
  na=$'2\ta\tK2\tno\tno\tno\tn/a\tn/a\t-\t5'
  for c in "pass:0:$ok" "build-fail-only:0:$na" "nccmp-fail:1:${ok/PASS/FAIL}" "nc-missing:1:${ok/PASS/MISSING}" \
           "kokkos-missing:1:${ok/$'\tyes\tyes\tyes'/$'\tyes\tfortran-only\tno'}" \
           "no-kokkos-log:1:${ok/$'\tyes\tyes\tyes'/$'\tyes\tyes\tno'}"; do
    name="${c%%:*}" rest="${c#*:}"; want="${rest%%:*}" row="${rest#*:}"
    printf '%s\n%s\n%s\n' "$hdr" "$ok" "$row" > "$t/s.tsv"
    matrix_verdict "$t/s.tsv" && rc=0 || rc=$?
    [ "$rc" = "$want" ] || { echo "hoff_matrix: verdict '$name' gave $rc, not $want" >&2; exit 1; }
  done
  printf '%s\n' "variable   n  dropped  max|d|  rms  max rel  verdict" "-----------" \
    "hs 1 0 0 0 0  PASS" "fp 1 0 0 0 0  FAIL" "lat 1 0 0 0 0  unlisted" "nccmp-tol: FAIL (2 judged, 1 unlisted)" > "$t/n.out"
  [ "$(judged_vars "$t/n.out")" = "hs,fp" ] || { echo "hoff_matrix: judged_vars wrong: $(judged_vars "$t/n.out")" >&2; exit 1; }
  printf '%s\n' 'cp -r data_regtests/a/input/*.nc  regtests/a/input/' \
    'cp -r data_regtests/a/input/w.nc regtests/a/input2/r.nc' 'cp -r data_regtests/z/input/* regtests/z/input/' \
    '# cp -r data_regtests/a/x regtests/a/' > "$t/ftp.sh"
  ww3="$t" && mkdir -p "$t/model/bin" && cp "$t/ftp.sh" "$t/model/bin/ww3_from_ftp.sh"
  [ "$(ftp_copies "$t/ftp.sh" | wc -l)" = 3 ] || { echo "hoff_matrix: ftp_copies wrong" >&2; exit 1; }
  mkdir -p "$t/cache/a/input" && touch "$t/cache/a/input/w.nc" "$t/cache/a/input/v.nc"
  stage_data "$t/cache" "$t/rt" a >/dev/null
  [ -f "$t/rt/a/input/v.nc" ] && [ -f "$t/rt/a/input/w.nc" ] && [ -f "$t/rt/a/input2/r.nc" ] \
    || { echo "hoff_matrix: stage_data wrong" >&2; exit 1; }
  [ -n "$(stage_data "$t/nocache" "$t/rt" a 2>&1 >/dev/null)" ] || { echo "hoff_matrix: stage_data did not warn" >&2; exit 1; }
  [ -z "$(stage_data "$t/nocache" "$t/rt" b 2>&1)" ] || { echo "hoff_matrix: stage_data warned for a test with no data" >&2; exit 1; }
  ww3="$root/WW3"
  printf '%s\n' 'drwxr-xr-x u/g 0 2026-01-01 00:00 data_regtests/' '-rw-r--r-- u/g 9 2026-01-01 00:00 data_regtests/a b.nc' \
    'lrwxrwxrwx u/g 0 2026-01-01 00:00 data_regtests/l -> /etc' '-rw-r--r-- u/g 9 2026-01-01 00:00 data_regtests/../x' \
    '-rw-r--r-- u/g 9 2026-01-01 00:00 /etc/x' > "$t/tv.txt"
  [ "$(tar_unsafe "$t/tv.txt" | wc -l)" = 3 ] || { echo "hoff_matrix: tar_unsafe wrong: $(tar_unsafe "$t/tv.txt")" >&2; exit 1; }
  for bad in --gpu "--jobs 0" --bogus; do
    # shellcheck disable=SC2086
    bash "${BASH_SOURCE[0]}" $bad >/dev/null 2>&1 && rc=0 || rc=$?
    [ "$rc" = 64 ] || { echo "hoff_matrix: '$bad' gave exit $rc, not 64" >&2; exit 1; }
  done
  if [ -f "$ww3/regtests/bin/matrix.base" ]; then
    n="$(select_entries "$ww3/regtests/bin/matrix.base" "$ww3/regtests" | wc -l)"
    [ "$n" = 100 ] || { echo "hoff_matrix: WW3@$(git -C "$ww3" rev-parse --short HEAD) matrix.base gives $n entries, not 100" >&2; exit 1; }
    echo "hoff_matrix: WW3@$(git -C "$ww3" rev-parse --short HEAD) matrix.base: 100 entries"
  fi
  echo "hoff_matrix self-test: ok"
  exit 0
fi

[ -f "$ww3/regtests/bin/matrix.base" ] || { echo "hoff_matrix: WW3/ is empty: git submodule update --init WW3" >&2; exit 3; }
ftpver="$(sed -n 's/^ww3ver=//p' "$ww3/model/bin/ww3_from_ftp.sh")"
ftpurl="$(grep -o 'https://ftp\.emc\.ncep\.noaa\.gov/[^[:space:]]*' "$ww3/model/bin/ww3_from_ftp.sh" | head -1)"
ftpurl="${ftpurl//\$\{ww3ver\}/$ftpver}"
datacache="$root/build/ww3-data/$ftpver/data_regtests"

if [ "$mode" = fetch ]; then
  case "$ftpurl" in https://ftp.emc.ncep.noaa.gov/static_files/public/WW3/ww3_from_ftp.v*.tar.gz) ;;
    *) echo "hoff_matrix: unexpected URL in ww3_from_ftp.sh: '$ftpurl'" >&2; exit 1 ;; esac
  [ -d "$datacache" ] && { echo ">> $datacache already there"; exit 0; }
  dl="$root/build/ww3-data/$ftpver/download" tgz="$root/build/ww3-data/$ftpver/download/${ftpurl##*/}"
  if [ -n "$dry" ]; then echo "curl -fL -o $tgz $ftpurl; tar -xzf $tgz -C ${datacache%/*} data_regtests"; exit 0; fi
  mkdir -p "$dl"
  [ -f "$tgz" ] || { curl -fL --retry 3 -o "$tgz.part" "$ftpurl" && mv "$tgz.part" "$tgz"; }
  echo ">> $ftpurl: $(stat -c %s "$tgz") bytes, sha256 $(sha256sum "$tgz" | cut -d' ' -f1)"
  tar -tvzf "$tgz" > "$dl/listing.txt"
  bad="$(tar_unsafe "$dl/listing.txt")"
  [ -z "$bad" ] || { printf 'hoff_matrix: refusing %s, unsafe members:\n%s\n' "$tgz" "$bad" >&2; exit 1; }
  echo ">> $(wc -l < "$dl/listing.txt") members, all regular files or directories under data_regtests/"
  tar -xzf "$tgz" -C "${datacache%/*}" --no-same-owner --no-same-permissions data_regtests
  echo ">> extracted into $datacache ($(du -sh "$datacache" | cut -f1)); $dl/ can be deleted"
  exit 0
fi

mapfile -t entries < <(select_entries "$ww3/regtests/bin/matrix.base" "$ww3/regtests")

if [ "$mode" = list ]; then
  for i in "${!entries[@]}"; do printf '%d %s %s\n' "$((i + 1))" "${entries[i]%%$'\t'*}" "${entries[i]#*$'\t'}"; done
  exit 0
fi

# Which entries: all, or those --only names.
picked=()
for i in "${!entries[@]}"; do
  n=$((i + 1)) test="${entries[i]%%$'\t'*}"
  read -r -a o <<<"${entries[i]#*$'\t'}"
  sw="$(opt_of -s "${o[@]}")"
  if [ "${#only[@]}" -eq 0 ]; then picked+=("$n"); continue; fi
  for s in "${only[@]}"; do
    if [ "$s" = "$n" ] || [ "$s" = "$test" ] || [ "$s" = "$test:${sw:-default}" ]; then picked+=("$n"); break; fi
  done
done
[ "${#picked[@]}" -gt 0 ] || { echo "hoff_matrix: --only ${only[*]} selects no entry (see --list)" >&2; exit 64; }

[ -f "$ww3/model/src/w3kokkosmd.F90" ] || {
  echo "hoff_matrix: WW3/ is not the hoff/kokkos-snl1 branch of h0ffmann/WW3 (no model/src/w3kokkosmd.F90)" >&2; exit 3; }

kbuild="$root/kokkos/build/openmp-release"
kprefix="$kbuild/install"
nccmp="$kbuild/tools/nccmp-tol/nccmp-tol"
tols="$root/kokkos/tools/nccmp-tol/tolerances.txt"

# entry_cmd <n> <F|K>: the run_cmake_test argument list of entry <n>'s run, one per line.
entry_cmd() {
  local n="$1" kind="$2" test prev="" f sw inp
  local -a o keep=()
  test="${entries[n - 1]%%$'\t'*}"
  read -r -a o <<<"${entries[n - 1]#*$'\t'}"
  sw="$(opt_of -s "${o[@]}")"
  for f in "${o[@]}"; do
    if [ "$f" = -s ] || [ "$f" = -w ]; then prev="$f"; continue; fi
    [ "$prev" = -s ] || [ "$prev" = -w ] || keep+=("$f")
    prev="$f"
  done
  inp="$(opt_of -i "${o[@]}")"
  if [ -z "$(opt_of -o "${o[@]}")" ] && has_ounf "$ww3/regtests/$test/${inp:-input}"; then
    keep+=(-o netcdf)
  fi
  printf '%s\n' "${keep[@]}" -s "${sw:+${sw}_}KOKKOS" -w "work_${kind}_$n" "$ww3/model" "$test"
}

if [ -n "$dry" ]; then
  echo "# cd $stage"
  for n in "${picked[@]}"; do
    for k in F K; do
      mapfile -t a < <(entry_cmd "$n" "$k")
      [ "$k" = F ] && s=0 || s=1
      echo "CMAKE_OPTIONS='-DWW_KOKKOS=ON -DCMAKE_PREFIX_PATH=$kprefix' WW_KOKKOS_SNL1=$s path_build_root=$out/build_$n $ww3/regtests/bin/run_cmake_test ${a[*]}"
    done
    test="${entries[n - 1]%%$'\t'*}"
    echo "$ww3/regtests/bin/test.comp $test work_F_$n work_K_$n; $nccmp work_F_$n/ww3.*.nc work_K_$n/ww3.*.nc $tols"
  done
  exit 0
fi

# Enter the pinned toolchain once, then continue inside it.
if [ -z "${HOFF_IN_SHELL:-}" ]; then
  exec env HOFF_IN_SHELL=1 nix develop "$pratico#ww3" --command bash "${BASH_SOURCE[0]}" "${orig_args[@]}"
fi

# --- the Kokkos package and nccmp-tol (hoff.sh:183-191) -------------------------------------
[ -f "$kbuild/CMakeCache.txt" ] || cmake -S "$root/kokkos" --preset openmp-release >/dev/null
cmake --build "$kbuild" -j"$(nproc)" >/dev/null
cmake --install "$kbuild" --prefix "$kprefix" >/dev/null
echo ">> ww_kokkos (openmp-release) installed in $kprefix"

# --- stage: inputs copied out of WW3/, the KOKKOS switch beside each original ---------------
mkdir -p "$stage" "$out/rows" "$out/logs"
declare -A staged=()
for n in "${picked[@]}"; do
  test="${entries[n - 1]%%$'\t'*}"
  read -r -a o <<<"${entries[n - 1]#*$'\t'}"
  if [ -z "${staged[$test]:-}" ]; then
    mkdir -p "$stage/$test" && cp -a "$ww3/regtests/$test/." "$stage/$test/"
    stage_data "$datacache" "$stage" "$test"
    staged[$test]=1
  fi
  inp="$(opt_of -i "${o[@]}")"; sw="$(opt_of -s "${o[@]}")"
  d="$stage/$test/${inp:-input}"
  printf '%s KOKKOS\n' "$(tr -s ' \n' ' ' < "$d/switch${sw:+_$sw}" | sed 's/ *$//')" > "$d/switch_${sw:+${sw}_}KOKKOS"
  rm -rf -- "${stage:?}/$test/work_F_$n" "${stage:?}/$test/work_K_$n" "$out/rows/$n.tsv"
done

# run_entry <n>: both runs and both comparisons of entry <n>; writes rows/<n>.tsv, never fails.
run_entry() {
  local n="$1" test sw t0 built=no ran=no klog=no tc=n/a nv=n/a judged=- rf=0 rk=0 nc=0 f nd ni j r
  local -a o a
  t0="$(date +%s)"
  test="${entries[n - 1]%%$'\t'*}"
  read -r -a o <<<"${entries[n - 1]#*$'\t'}"
  sw="$(opt_of -s "${o[@]}")"
  local wf="$stage/$test/work_F_$n" wk="$stage/$test/work_K_$n" log="$out/logs/$n"
  for k in F K; do
    mapfile -t a < <(entry_cmd "$n" "$k")
    (cd "$stage" && env CMAKE_OPTIONS="-DWW_KOKKOS=ON -DCMAKE_PREFIX_PATH=$kprefix" \
       WW_KOKKOS_SNL1="$([ "$k" = F ] && echo 0 || echo 1)" path_build_root="$out/build_$n" \
       OMP_PROC_BIND="${OMP_PROC_BIND:-false}" "$ww3/regtests/bin/run_cmake_test" "${a[@]}") \
       > "$log.$k.log" 2>&1 || true
  done
  [ -x "$wf/exe/ww3_shel" ] && built=yes
  grep -qs "End of program" "$wf/ww3_shel.out" && rf=1
  grep -qs "End of program" "$wk/ww3_shel.out" && rk=1
  case "$rf$rk" in 11) ran=yes ;; 10) ran=fortran-only ;; 01) ran=kokkos-only ;; esac
  grep -qs "Kokkos DIA (W3SNL1) enabled" "$wk/log.ww3" "$wk/ww3_shel.out" && klog=yes
  if [ "$rf$rk" = 11 ]; then
    (cd "$stage" && "$ww3/regtests/bin/test.comp" "$test" "work_F_$n" "work_K_$n") > "$log.test_comp.log" 2>&1 || true
    nd="$(grep -c ' is different$' "$log.test_comp.log" || true)"
    ni="$(grep -c ' is identical$' "$log.test_comp.log" || true)"
    tc="$nd/$((nd + ni))"
    : > "$log.nccmp.log"
    while IFS= read -r f; do
      [ -f "$wk/$f" ] || continue
      nc=$((nc + 1))
      echo "== $f" >> "$log.nccmp.log"
      "$nccmp" "$wf/$f" "$wk/$f" "$tols" > "$log.nccmp.$f.out" 2>&1 && r=PASS || r=FAIL
      cat "$log.nccmp.$f.out" >> "$log.nccmp.log"
      j="$(judged_vars "$log.nccmp.$f.out")"
      if [ "$r" = FAIL ]; then nv=FAIL
      elif [ -n "$j" ] && [ "$nv" != FAIL ]; then nv=PASS; fi
      [ -z "$j" ] || judged="$(printf '%s\n' "${judged#-}" "$j" | tr , '\n' | sed '/^$/d' | sort -u | paste -sd, -)"
    done < <(find "$wf" -maxdepth 1 -name 'ww3.*.nc' -printf '%f\n' | sort)
    j="$(opt_of -i "${o[@]}")"
    if [ "$nc" = 0 ] && has_ounf "$ww3/regtests/$test/${j:-input}" \
       && [ "$(opt_of -o "${o[@]}")" != native ]; then nv=MISSING; fi
  fi
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$n" "$test" "${sw:-default}" "$built" "$ran" "$klog" \
    "$tc" "$nv" "${judged:--}" "$(( $(date +%s) - t0 ))" "${o[*]}" > "$out/rows/$n.tsv"
  echo ">> entry $n $test ${sw:-default}: built=$built ran=$ran kokkos_log=$klog test.comp=$tc nccmp-tol=$nv"
}

[ -n "${OMP_NUM_THREADS:-}" ] || { t=$(( $(nproc) / jobs )); export OMP_NUM_THREADS=$(( t > 0 ? t : 1 )); }
for n in "${picked[@]}"; do
  while [ "$(jobs -rp | wc -l)" -ge "$jobs" ]; do wait -n || true; done
  run_entry "$n" &
done
wait

# --- record ---------------------------------------------------------------------------------
summary="$out/summary.tsv"
{
  echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "# ww3-gpu commit: $(git -C "$root" rev-parse --short HEAD)$(git -C "$root" diff --quiet HEAD -- . ':!WW3' 2>/dev/null || echo ' (dirty)')"
  echo "# WW3 commit: $(git -C "$ww3" rev-parse --short HEAD) ($(git -C "$ww3" branch -r --contains HEAD | sed 's/^ *//' | paste -sd, -))"
  echo "# machine: $(lscpu | sed -n 's/^Model name: *//p' | head -1), $(nproc) threads, OMP_NUM_THREADS=$OMP_NUM_THREADS"
  echo "# command: just hoff-matrix ${orig_args[*]}"
  printf 'n\ttest\tswitch\tbuilt\tran\tkokkos_log\ttest_comp\tnccmp_tol\tjudged\twall_s\toptions\n'
  for n in "${picked[@]}"; do
    if [ -f "$out/rows/$n.tsv" ]; then cat "$out/rows/$n.tsv"
    else printf '%s\t%s\t-\tno\tno\tno\tn/a\tn/a\t-\t0\t%s\n' "$n" "${entries[n - 1]%%$'\t'*}" "${entries[n - 1]#*$'\t'}"; fi
  done
} > "$summary"
echo
grep -v '^#' "$summary" | column -t -s $'\t'
echo ">> $summary; logs in $out/logs/"

rc=0
matrix_verdict "$summary" || rc=1
if [ -n "$(git -C "$ww3" status --porcelain)" ]; then
  echo "hoff_matrix: WW3/ was written (git -C WW3 status --porcelain is not empty)" >&2
  rc=1
fi
exit "$rc"
