#!/usr/bin/env bash
# coast_case — write the generated inputs of the coast case of scripts/ww3-gpu.sh into a
# directory that already holds its namelists (kokkos/tests/fixtures/coast/):
#
#   depth.inp, mask.inp   80 x 50 cells of 5 km (ww3_grid.nml): a shelf from 100 m at the west
#                         edge to 8 m at x = 370 km, an intertidal strip 1 m deep (x = 375, 380
#                         km) that dries when the level falls below -1 m, land from x = 385 km,
#                         an island of nine cells at (200 km, 125 km) on a shoal, and the west
#                         column as boundary input points (mask 2)
#   current.nc            hourly, 0 to 12 h: a tidal along-shore jet near the coast, a weaker
#                         cross-shelf tide and a steady eddy round the island that strengthens
#   level.nc              hourly: a tide whose amplitude grows from 0.8 m offshore to 1.5 m at
#                         the coast, its phase moving along the coast
#   ice.nc                hourly: ice concentration growing from the north-east corner
#   bound_{s,c,n}_spec.nc spectra at x = 0, y = 0, 125, 245 km, hourly: a JONSWAP swell
#                         (fp 0.075 Hz, Hs 1.5 to 2.9 m) travelling east-north-east
#
# Each file is a CDL text written by awk and turned into NetCDF by ncgen (netCDF's own tool, in
# the pinned toolchain), so the forcing is reproducible from this script and a namelist.
# None of it is physics to learn from: it is a case that switches on the current, wavenumber
# shift, water-level, ice and boundary paths of W3WAVE for the bit-for-bit comparison.
#
#   scripts/coast_case.sh <dir>
#   scripts/coast_case.sh --self-test
set -euo pipefail

nx=80 ny=50 dx=5000 nt=13

if [ "${1:-}" = --self-test ]; then
  t="$(mktemp -d)"; trap 'rm -rf -- "$t"' EXIT
  "${BASH_SOURCE[0]}" "$t" >/dev/null
  for f in depth.inp mask.inp current.nc level.nc ice.nc bound_s_spec.nc bound_c_spec.nc bound_n_spec.nc; do
    [ -s "$t/$f" ] || { echo "coast_case: $f not written" >&2; exit 1; }
  done
  [ "$(wc -l < "$t/depth.inp")" = "$ny" ] || { echo "coast_case: depth.inp is not $ny rows" >&2; exit 1; }
  [ "$(awk '{ n += NF } END { print n }' "$t/mask.inp")" = $((nx * ny)) ] || { echo "coast_case: mask.inp is not $nx x $ny" >&2; exit 1; }
  # Nine cells of the island, three land columns, and the west column as boundary points.
  [ "$(tr -s ' ' '\n' < "$t/mask.inp" | grep -c '^0$')" = $((3 * ny + 9)) ] || { echo "coast_case: land count changed" >&2; exit 1; }
  [ "$(tr -s ' ' '\n' < "$t/mask.inp" | grep -c '^2$')" = "$ny" ] || { echo "coast_case: boundary count changed" >&2; exit 1; }
  mkdir "$t/again" && "${BASH_SOURCE[0]}" "$t/again" >/dev/null
  for f in depth.inp mask.inp current.nc level.nc ice.nc bound_c_spec.nc; do
    cmp -s "$t/$f" "$t/again/$f" || { echo "coast_case: $f differs between two runs" >&2; exit 1; }
  done
  echo "coast_case self-test: ok"
  exit 0
fi
[ $# = 1 ] && [ -d "$1" ] || { echo "usage: scripts/coast_case.sh <dir> | --self-test" >&2; exit 64; }
command -v ncgen >/dev/null || { echo "coast_case: no ncgen (enter the toolchain: just ww3)" >&2; exit 3; }
d="$1"

# The geometry, shared by every awk program below. depth(ix, iy) in m, positive down; land < 0.
geom='
function depth(ix, iy,   x, y, r2, h) {
  x = (ix - 1) * DX; y = (iy - 1) * DX
  r2 = (x - 200000) ^ 2 + (y - 125000) ^ 2
  if (ix >= 78 || r2 < 7600 ^ 2) return -5
  if (ix >= 76) return 1
  h = 8 + 92 * (1 - x / 375000) ^ 1.5
  return h * (1 - 0.6 * exp(-r2 / 20000 ^ 2))
}'

# depth.inp and mask.inp: IDLA = 1, one row per y from the south, x from the west.
awk -v NX="$nx" -v NY="$ny" -v DX="$dx" -v D="$d" "$geom"'
BEGIN {
  fd = D "/depth.inp"; fm = D "/mask.inp"
  for (iy = 1; iy <= NY; iy++) {
    for (ix = 1; ix <= NX; ix++) printf "%s%.2f", (ix > 1 ? " " : ""), depth(ix, iy) > fd
    printf "\n" > fd
    for (ix = 1; ix <= NX; ix++) {
      m = depth(ix, iy) < 0 ? 0 : (ix == 1 ? 2 : 1)
      printf "%s%d", (ix > 1 ? " " : ""), m > fm
    }
    printf "\n" > fm
  }
}' </dev/null

# One gridded field file in CDL: <name> <var>[,<var>] <long names>; values from field(v, ix, iy, t).
field_cdl() { # <file stem> <var list> <awk body defining field(v, ix, iy, t)>
  awk -v NX="$nx" -v NY="$ny" -v NT="$nt" -v DX="$dx" -v STEM="$1" -v VARS="$2" "$geom $3"'
BEGIN {
  nv = split(VARS, v, ",")
  printf "netcdf %s {\ndimensions:\n\ttime = %d ;\n\ty = %d ;\n\tx = %d ;\nvariables:\n", STEM, NT, NY, NX
  printf "\tdouble time(time) ;\n\t\ttime:units = \"hours since 2020-01-01 00:00:00\" ;\n\t\ttime:calendar = \"standard\" ;\n"
  printf "\tfloat x(x) ;\n\t\tx:units = \"m\" ;\n\tfloat y(y) ;\n\t\ty:units = \"m\" ;\n"
  for (k = 1; k <= nv; k++) printf "\tfloat %s(time, y, x) ;\n\t\t%s:_FillValue = 9.96921e+36f ;\n", v[k], v[k]
  printf "data:\n\ttime ="
  for (t = 0; t < NT; t++) printf "%s %d", (t ? "," : ""), t
  printf " ;\n\tx ="
  for (ix = 1; ix <= NX; ix++) printf "%s %d", (ix > 1 ? "," : ""), (ix - 1) * DX
  printf " ;\n\ty ="
  for (iy = 1; iy <= NY; iy++) printf "%s %d", (iy > 1 ? "," : ""), (iy - 1) * DX
  printf " ;\n"
  for (k = 1; k <= nv; k++) {
    printf "\t%s =\n", v[k]; sep = ""
    for (t = 0; t < NT; t++) for (iy = 1; iy <= NY; iy++) {
      printf "%s", sep
      for (ix = 1; ix <= NX; ix++) printf "%s%.5f", (ix > 1 ? ", " : "  "), field(v[k], ix, iy, t)
      sep = ",\n"
    }
    printf " ;\n"
  }
  printf "}\n"
}' </dev/null
}

# Tide of 12.42 h. Currents in m/s: along-shore jet, cross-shelf tide, eddy round the island.
field_cdl current ucur,vcur '
function field(var, ix, iy, t,   x, y, w, r2, om, u, v) {
  x = (ix - 1) * DX; y = (iy - 1) * DX; w = 2 * 3.141592653589793 / 12.42
  r2 = (x - 200000) ^ 2 + (y - 125000) ^ 2
  om = 0.6 * sqrt(2) * exp(0.5) / 30000 * (1 + 0.3 * t / 12)
  u = 0.2 * sin(w * t) * x / 400000 - om * (y - 125000) * exp(-r2 / 30000 ^ 2)
  v = 0.8 * cos(w * t) * exp(-((x - 360000) / 25000) ^ 2) + om * (x - 200000) * exp(-r2 / 30000 ^ 2)
  if (depth(ix, iy) < 0) return 0
  return var == "ucur" ? u : v
}' > "$d/current.cdl"

field_cdl level wlv '
function field(var, ix, iy, t,   x, y, w) {
  x = (ix - 1) * DX; y = (iy - 1) * DX; w = 2 * 3.141592653589793 / 12.42
  return (0.8 + 0.7 * x / 385000) * cos(w * t - 3.141592653589793 * y / (2 * 245000))
}' > "$d/level.cdl"

field_cdl ice ice '
function field(var, ix, iy, t,   x, y, c) {
  x = (ix - 1) * DX; y = (iy - 1) * DX
  c = 1.4 * t / 12 - sqrt((x - 380000) ^ 2 + (y - 245000) ^ 2) / 60000
  return c < 0 ? 0 : (c > 1 ? 1 : c)
}' > "$d/ice.cdl"

# Boundary spectra, efth(time, station, frequency, direction) in m2 s rad-1, as ww3_ounp writes
# them and ww3_bounc reads them: WW3's own frequencies (FREQ1 0.04118, XFR 1.1, NK 24) and
# directions "to", nautical, starting at 90 so that ww3_bounc's first direction is WW3's TH(1) = 0.
spec_cdl() { # <stem> <station index 0..2> <y in m>
  awk -v STEM="$1" -v S="$2" -v Y="$3" -v NT="$nt" '
BEGIN {
  NK = 24; NTH = 24; pi = 3.141592653589793; dth = 2 * pi / NTH
  for (k = 1; k <= NK; k++) { f[k] = 0.04118 * 1.1 ^ (k - 1); df[k] = f[k] * (1.1 - 1 / 1.1) / 2 }
  for (j = 1; j <= NTH; j++) d[j] = (90 - (j - 1) * 15 + 360) % 360
  printf "netcdf %s {\ndimensions:\n\ttime = %d ;\n\tstation = 1 ;\n\tstring16 = 16 ;\n\tfrequency = %d ;\n\tdirection = %d ;\nvariables:\n", STEM, NT, NK, NTH
  printf "\tdouble time(time) ;\n\t\ttime:units = \"hours since 2020-01-01 00:00:00\" ;\n\t\ttime:calendar = \"standard\" ;\n"
  printf "\tchar station_name(station, string16) ;\n\tfloat x(time, station) ;\n\tfloat y(time, station) ;\n"
  printf "\tfloat frequency(frequency) ;\n\tfloat direction(direction) ;\n"
  printf "\tfloat efth(time, station, frequency, direction) ;\n\t\tefth:_FillValue = 9.96921e+36f ;\n"
  printf "data:\n\ttime ="
  for (t = 0; t < NT; t++) printf "%s %d", (t ? "," : ""), t
  printf " ;\n\tstation_name = \"%s\" ;\n\tx =", STEM
  for (t = 0; t < NT; t++) printf "%s 0", (t ? "," : "")
  printf " ;\n\ty ="
  for (t = 0; t < NT; t++) printf "%s %d", (t ? "," : ""), Y
  printf " ;\n\tfrequency ="
  for (k = 1; k <= NK; k++) printf "%s %.7g", (k > 1 ? "," : ""), f[k]
  printf " ;\n\tdirection ="
  for (j = 1; j <= NTH; j++) printf "%s %d", (j > 1 ? "," : ""), d[j]
  printf " ;\n\tefth =\n"
  fp = 0.075; sep = ""
  for (t = 0; t < NT; t++) {
    hs = 1.5 + t / 12 + 0.2 * S; dm = 70 + 5 * S; m0 = 0
    for (k = 1; k <= NK; k++) {
      sg = f[k] <= fp ? 0.07 : 0.09
      ef[k] = f[k] ^ -5 * exp(-1.25 * (fp / f[k]) ^ 4) * 3.3 ^ exp(-(f[k] - fp) ^ 2 / (2 * sg ^ 2 * fp ^ 2))
      for (j = 1; j <= NTH; j++) {
        a = (d[j] - dm) * pi / 180
        dd[k, j] = ef[k] * cos(a / 2) ^ 20
        m0 += dd[k, j] * df[k] * dth
      }
    }
    for (k = 1; k <= NK; k++) {
      printf "%s", sep
      for (j = 1; j <= NTH; j++) printf "%s%.6g", (j > 1 ? ", " : "  "), dd[k, j] * hs ^ 2 / (16 * m0)
      sep = ",\n"
    }
  }
  printf " ;\n}\n"
}' </dev/null
}
spec_cdl bound_s_spec 0 0 > "$d/bound_s_spec.cdl"
spec_cdl bound_c_spec 1 125000 > "$d/bound_c_spec.cdl"
spec_cdl bound_n_spec 2 245000 > "$d/bound_n_spec.cdl"

for c in current level ice bound_s_spec bound_c_spec bound_n_spec; do
  ncgen -k classic -o "$d/$c.nc" "$d/$c.cdl"
  rm -f -- "$d/$c.cdl"
done
echo "coast_case: inputs written in $d"
