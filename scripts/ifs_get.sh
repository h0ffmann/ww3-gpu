#!/usr/bin/env bash
# ifs_get — fetch one ECMWF IFS cycle from ECMWF Open Data and write a netCDF file ww3_prnc reads
# as wind forcing (u10, v10 on longitude, latitude, time), for ww3-gpu's consumers ondaseventos and
# marola (docs/IFS_FORCING_202611.md).
#
#   scripts/ifs_get.sh [options]
#     -d YYYYMMDD | latest   cycle date (default latest: the newest cycle whose last step is published)
#     -c 00|06|12|18         cycle (default 00)
#     -H HOURS               last forecast step (default 120; 06/18z stop at 144, 00/12z at 360)
#     -p "10u 10v ..."       ECMWF short names, surface level (default "10u 10v"; msl, 10fg also work)
#     -b W,E,S,N             subset box in degrees, -180..180 (default: the whole globe)
#     -o DIR                 output directory (default data/ifs/<YYYYMMDDHH>)
#     -n                     dry run: print the URLs and byte ranges, download nothing
#     --self-test            check argument parsing and the index parser offline
#
# What it does: for every step (3 h to 144 h, then 6 h) it reads the step's .index (one JSON line
# per GRIB message), downloads only the messages asked for with an HTTP range request, concatenates
# them (GRIB2 messages concatenate), converts them with ecCodes' grib_to_netcdf, and subsets with
# `ncks` when -b is given. No key: Open Data is anonymous, licensed CC BY 4.0. IFS_BASE_URL
# points it at a mirror with the same layout. Files already downloaded are kept.
#
# SPDX-License-Identifier: MIT
set -euo pipefail

base="${IFS_BASE_URL:-https://data.ecmwf.int/forecasts}"
date_arg=latest cyc=00 last=120 params="10u 10v" box="" out="" dry=0

die() { echo "ifs_get: $*" >&2; exit 2; }

# step list of a cycle: 0..144 every 3 h, then every 6 h (00/12z only), up to <last>
steps() {
    local last=$1 h
    for ((h = 0; h <= last && h <= 144; h += 3)); do echo "$h"; done
    for ((h = 150; h <= last; h += 6)); do echo "$h"; done
}

# "<param> <offset> <length>" for each surface message of an index whose param is in <params>
pick() {
    local index=$1 wanted=" $2 "
    awk -v wanted="$wanted" '
        {
            p = $0; sub(/.*"param": "/, "", p); sub(/".*/, "", p)
            l = $0; sub(/.*"levtype": "/, "", l); sub(/".*/, "", l)
            o = $0; sub(/.*"_offset": /, "", o); sub(/[^0-9].*/, "", o)
            n = $0; sub(/.*"_length": /, "", n); sub(/[^0-9].*/, "", n)
            if (l == "sfc" && index(wanted, " " p " ")) print p, o, n
        }' "$index"
}

self_test() {
    local t; t="$(mktemp -d)"; trap 'rm -rf "$t"' RETURN
    cat >"$t/i" <<'EOF'
{"domain": "g", "step": "3", "levtype": "sfc", "param": "10u", "_offset": 0, "_length": 10}
{"domain": "g", "step": "3", "levtype": "pl", "levelist": "1000", "param": "u", "_offset": 10, "_length": 5}
{"domain": "g", "step": "3", "levtype": "sfc", "param": "100u", "_offset": 15, "_length": 7}
{"domain": "g", "step": "3", "levtype": "sfc", "param": "10v", "_offset": 22, "_length": 9}
EOF
    [ "$(pick "$t/i" "10u 10v")" = $'10u 0 10\n10v 22 9' ] || { echo "self-test: pick failed" >&2; return 1; }
    [ "$(pick "$t/i" "u")" = "" ] || { echo "self-test: pick took a pressure level" >&2; return 1; }
    [ "$(steps 12 | tr '\n' ' ')" = "0 3 6 9 12 " ] || { echo "self-test: steps 12" >&2; return 1; }
    [ "$(steps 162 | tail -3 | tr '\n' ' ')" = "150 156 162 " ] || { echo "self-test: steps 162" >&2; return 1; }
    [ "$(steps 120 | wc -l)" = 41 ] || { echo "self-test: steps 120" >&2; return 1; }
    echo "ifs_get self-test: ok"
}

while [ $# -gt 0 ]; do
    case "$1" in
        -d) date_arg="${2:?-d needs a date}"; shift 2 ;;
        -c) cyc="${2:?-c needs a cycle}"; shift 2 ;;
        -H) last="${2:?-H needs hours}"; shift 2 ;;
        -p) params="${2:?-p needs short names}"; shift 2 ;;
        -b) box="${2:?-b needs W,E,S,N}"; shift 2 ;;
        -o) out="${2:?-o needs a directory}"; shift 2 ;;
        -n|--dry-run) dry=1; shift ;;
        --self-test) self_test; exit ;;
        -h|--help) sed -n '2,21p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) die "unknown option '$1' (see --help)" ;;
    esac
done

[[ "$cyc" =~ ^(00|06|12|18)$ ]] || die "cycle must be 00, 06, 12 or 18, got '$cyc'"
[[ "$last" =~ ^[0-9]+$ ]] || die "-H must be a whole number of hours, got '$last'"
max=360; [[ "$cyc" =~ ^(06|18)$ ]] && max=144
[ "$last" -le "$max" ] || die "the ${cyc}z cycle stops at ${max} h, asked for ${last} h"
[ -z "$box" ] || [[ "$box" =~ ^-?[0-9.]+,-?[0-9.]+,-?[0-9.]+,-?[0-9.]+$ ]] || die "-b must be W,E,S,N, got '$box'"
need=(curl)
[ "$dry" = 1 ] || need+=(grib_to_netcdf ncdump)
[ -z "$box" ] || [ "$dry" = 1 ] || need+=(ncks)
for tool in "${need[@]}"; do
    command -v "$tool" >/dev/null || die "$tool not found; run inside 'just ww3' or install ecCodes, NCO and netcdf-bin"
done

last_step="$(steps "$last" | tail -1)"
url_of() { echo "${base}/$1/${cyc}z/ifs/0p25/oper/$1${cyc}0000-$2h-oper-fc.$3"; }
published() { curl -fsI --retry 2 -o /dev/null "$(url_of "$1" "$last_step" index)"; }

if [ "$date_arg" = latest ]; then
    for back in 0 1 2; do
        d="$(date -u -d "-${back} day" +%Y%m%d)"
        if published "$d"; then date_arg="$d"; break; fi
    done
    [ "$date_arg" != latest ] || die "no ${cyc}z cycle with step ${last_step} h in the last three days at ${base}"
fi
[[ "$date_arg" =~ ^[0-9]{8}$ ]] || die "date must be YYYYMMDD or latest, got '$date_arg'"

out="${out:-data/ifs/${date_arg}${cyc}}"
nc="$out/ifs_${date_arg}${cyc}_f$(printf '%03d' "$last_step").nc"
echo ">> IFS ${date_arg} ${cyc}z, steps 0..${last_step} h, params: ${params}, box: ${box:-global}"
echo ">> from ${base} (ECMWF Open Data, CC BY 4.0)"
mkdir -p "$out/grib"

n=0
for h in $(steps "$last"); do
    g="$out/grib/f$(printf '%03d' "$h").grib2"
    if [ -s "$g" ] && [ "$dry" = 0 ]; then n=$((n + 1)); continue; fi
    idx="$out/grib/f$(printf '%03d' "$h").index"
    curl -fsSL --retry 3 --retry-delay 5 -o "$idx" "$(url_of "$date_arg" "$h" index)" \
        || die "no index for step ${h} h: is ${date_arg} ${cyc}z still on ${base}? (Open Data keeps a few days)"
    picked="$(pick "$idx" "$params")"
    [ -n "$picked" ] || die "step ${h} h has none of: ${params}"
    rm -f "$g.part"
    while read -r p o len; do
        range="${o}-$((o + len - 1))"
        if [ "$dry" = 1 ]; then echo "   f${h} ${p} bytes ${range}"; continue; fi
        curl -fsSL --retry 3 --retry-delay 5 -r "$range" "$(url_of "$date_arg" "$h" grib2)" >>"$g.part"
    done <<<"$picked"
    # a parameter missing from one step is reported, not filled
    for p in $params; do
        grep -q "^$p " <<<"$picked" || echo "   f${h}: no ${p} in this step"
    done
    [ "$dry" = 1 ] || mv "$g.part" "$g"
    n=$((n + 1))
done
[ "$dry" = 0 ] || exit 0
echo ">> ${n} steps in ${out}/grib"

merged="$out/merged.grib2"
cat "$out"/grib/f???.grib2 >"$merged"
grib_to_netcdf -o "$nc" "$merged" >/dev/null
rm -f "$merged"
if [ -n "$box" ]; then
    # cut after the conversion: grib_to_netcdf wrote no longitudes for a GRIB cut by cdo's sellonlatbox
    # printf: ncks reads a hyperslab bound without a decimal point as an index, not a coordinate
    IFS=, read -r w e s n <<<"$box"
    read -r w e s n < <(printf '%.4f %.4f %.4f %.4f\n' "$w" "$e" "$s" "$n")
    echo ">> subsetting to ${box}"
    ncks -O -d "longitude,${w},${e}" -d "latitude,${s},${n}" "$nc" "$nc.box"
    mv "$nc.box" "$nc"
fi
echo ">> wrote ${nc}"
ncdump -h "$nc" | sed -n '1,30p'
