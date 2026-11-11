#!/usr/bin/env bash
# cmems_get — fetch sea surface temperature from the Copernicus Marine Service and write it as a
# netCDF file beside a wave run (docs/RUN_COMMAND_202611.md). The companion of ifs_get.sh: the
# same options where they mean the same thing, and `--print-path` for the runner.
#
#   scripts/cmems_get.sh [options]
#     -s YYYYMMDD[HH]        first time, UTC (default today 00)
#     -H HOURS               length of the span (default 120)
#     -b W,E,S,N             box in degrees, -180..180 (required: the product is global at 1/12°)
#     -v "thetao ..."        variables of the dataset (default "thetao", potential temperature)
#     -o DIR                 output directory (default data/cmems/<YYYYMMDDHH>)
#     -n                     dry run: print the copernicusmarine command, fetch nothing
#     --print-path           print only the path of the netCDF file on stdout, the rest on stderr:
#                            `scripts/ww3-gpu.sh --temp "$(just temp --print-path -b W,E,S,N)"`
#     --self-test            check argument parsing and the request offline
#
# The product is GLOBAL_ANALYSISFORECAST_PHY_001_024 (Global Ocean Physics Analysis and Forecast,
# 1/12°), dataset cmems_mod_glo_phy-thetao_anfc_0.083deg_PT6H-i: potential temperature every 6 h,
# instantaneous; CMEMS_DATASET names another. Only the shallowest level is taken (0 to 1 m), which
# is what "sea surface temperature" means here. It calls the Copernicus Marine Toolbox
# (`copernicusmarine subset`, `pip install copernicusmarine`), which needs a free account: run
# `copernicusmarine login` once, or set COPERNICUSMARINE_SERVICE_USERNAME and
# COPERNICUSMARINE_SERVICE_PASSWORD in the environment. This script reads neither and stores
# nothing. A file already fetched for the same request is kept (<file>.args); the box is in the
# file's name, so two cases never overwrite each other's file.
#
# SPDX-License-Identifier: MIT
set -euo pipefail

dataset="${CMEMS_DATASET:-cmems_mod_glo_phy-thetao_anfc_0.083deg_PT6H-i}"
start="" hours=120 vars="thetao" box="" out="" dry=0 print_path=0

die() { echo "cmems_get: $*" >&2; exit 2; }

# request <YYYYMMDDHH> <hours> <W,E,S,N> <vars> <dir> <file>: the copernicusmarine command, one word per line
request() {
    local s=$1 h=$2 w e so n v t0 t1
    IFS=, read -r w e so n <<<"$3"
    t0="$(date -u -d "${s:0:8} ${s:8:2}:00:00" +%s)"
    t1=$((t0 + h * 3600))
    printf '%s\n' copernicusmarine subset --dataset-id "$dataset"
    for v in $4; do printf '%s\n' --variable "$v"; done
    printf '%s\n' --minimum-longitude "$w" --maximum-longitude "$e" --minimum-latitude "$so" --maximum-latitude "$n" \
        --minimum-depth 0 --maximum-depth 1 \
        --start-datetime "$(date -u -d "@$t0" +%Y-%m-%dT%H:%M:%S)" --end-datetime "$(date -u -d "@$t1" +%Y-%m-%dT%H:%M:%S)" \
        --output-directory "$5" --output-filename "$6" --overwrite --disable-progress-bar
}

self_test() {
    local r
    r="$(request 2026111000 48 -52,-44,-32,-24 "thetao" d f.nc | tr '\n' ' ')"
    [[ "$r" == *"--dataset-id $dataset "* ]] || { echo "self-test: dataset lost" >&2; return 1; }
    [[ "$r" == *"--start-datetime 2026-11-10T00:00:00 --end-datetime 2026-11-12T00:00:00 "* ]] \
        || { echo "self-test: time span: $r" >&2; return 1; }
    [[ "$r" == *"--minimum-longitude -52 --maximum-longitude -44 --minimum-latitude -32 --maximum-latitude -24 "* ]] \
        || { echo "self-test: box: $r" >&2; return 1; }
    [[ "$r" == *"--minimum-depth 0 --maximum-depth 1 "* ]] || { echo "self-test: depth" >&2; return 1; }
    if "$0" -b 1,2,3 -n >/dev/null 2>&1; then echo "self-test: took a bad box" >&2; return 1; fi
    if "$0" -n >/dev/null 2>&1; then echo "self-test: ran without a box" >&2; return 1; fi
    if "$0" -b -52,-44,-32,-24 -s 2026 -n >/dev/null 2>&1; then echo "self-test: took a bad date" >&2; return 1; fi
    "$0" -b -52,-44,-32,-24 -s 2026111000 -H 48 -n | grep -q -- "--variable thetao" || { echo "self-test: dry run" >&2; return 1; }
    echo "cmems_get self-test: ok"
}

while [ $# -gt 0 ]; do
    case "$1" in
        -s) start="${2:?-s needs a date}"; shift 2 ;;
        -H) hours="${2:?-H needs hours}"; shift 2 ;;
        -v) vars="${2:?-v needs variable names}"; shift 2 ;;
        -b) box="${2:?-b needs W,E,S,N}"; shift 2 ;;
        -o) out="${2:?-o needs a directory}"; shift 2 ;;
        -n|--dry-run) dry=1; shift ;;
        --print-path) print_path=1; shift ;;
        --self-test) self_test; exit ;;
        -h|--help) sed -n '2,26p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) die "unknown option '$1' (see --help)" ;;
    esac
done
# --print-path: stdout carries the path alone, so `$(...)` takes it.
if [ "$print_path" = 1 ]; then exec 3>&1 1>&2; fi

start="${start:-$(date -u +%Y%m%d)00}"
[[ "$start" =~ ^[0-9]{8}$ ]] && start="${start}00"
[[ "$start" =~ ^[0-9]{8}([01][0-9]|2[0-3])$ ]] || die "-s must be YYYYMMDD or YYYYMMDDHH, got '$start'"
date -u -d "${start:0:8}" >/dev/null 2>&1 || die "-s is not a date: '$start'"
[[ "$hours" =~ ^[1-9][0-9]{0,3}$ ]] || die "-H must be a whole number of hours, got '$hours'"
[[ "$box" =~ ^-?[0-9.]+,-?[0-9.]+,-?[0-9.]+,-?[0-9.]+$ ]] || die "-b W,E,S,N is required, got '${box}'"
[[ "$vars" =~ ^[A-Za-z0-9_]+(\ [A-Za-z0-9_]+)*$ ]] || die "-v takes variable names, got '$vars'"
[[ "$dataset" =~ ^[A-Za-z0-9._-]+$ ]] || die "CMEMS_DATASET is a dataset id, got '$dataset'"

out="${out:-data/cmems/${start}}"
IFS=, read -r bw be bs bn <<<"$box"
name="cmems_${start}_h$(printf '%03d' "$hours")$(printf '_box_%g_%g_%g_%g' "$bw" "$be" "$bs" "$bn").nc"
nc="$out/$name"
mapfile -t cmd < <(request "$start" "$hours" "$box" "$vars" "$out" "$name")
args="$dataset|$vars|$box|$start|$hours"

if [ "$dry" = 1 ]; then printf '%s ' "${cmd[@]}"; echo; exit 0; fi
if [ -s "$nc" ] && [ "$(cat "$nc.args" 2>/dev/null)" = "$args" ]; then
    echo ">> kept ${nc} (same request)"
else
    command -v copernicusmarine >/dev/null \
        || die "copernicusmarine not found: pip install copernicusmarine (the Copernicus Marine Toolbox), then copernicusmarine login"
    echo ">> ${dataset}: ${vars}, ${start} + ${hours} h, box ${box}, surface level"
    echo ">> from the Copernicus Marine Service (an account is needed; its licence is the service's)"
    mkdir -p "$out"
    rm -f "$nc.args"
    "${cmd[@]}" || die "copernicusmarine failed: is this machine logged in (copernicusmarine login), and does the forecast reach ${start} + ${hours} h?"
    [ -s "$nc" ] || die "copernicusmarine wrote no ${nc}"
    printf '%s\n' "$args" > "$nc.args"
    echo ">> wrote ${nc}"
fi
if [ "$print_path" = 1 ]; then echo "$nc" >&3; fi
