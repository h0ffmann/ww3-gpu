#!/usr/bin/env bash
# Run one upstream regression test by hand, step by step, so you can watch what
# each program does. This deliberately does NOT use matrix.comp / run_test --
# the point is to see the pipeline.
#
# usage: 03_run_regtest.sh <ww3-dir> <regtest-name>   e.g. ww3_tp2.2
set -euo pipefail

WW3DIR="${1:?usage: 03_run_regtest.sh <ww3-dir> <regtest>}"
TEST="${2:-ww3_tp2.2}"
SRC="$WW3DIR/regtests/$TEST"
WORK="$SRC/work_lab"

[ -d "$SRC" ] || { echo "!! no such regtest: $SRC"; exit 1; }

BIN="$WW3DIR/build/bin"
[ -d "$BIN" ] || BIN="$WW3DIR/build"

rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cp "$SRC"/input/* . 2>/dev/null || true
cp "$BIN"/ww3_* . 2>/dev/null || true

run () {
  local prog="$1"
  if [ ! -x "./$prog" ]; then echo "-- skip $prog (not built)"; return 0; fi
  # ww3_strt often ships only the .inp, so either input counts
  if [ -e "${prog}.nml" ] || [ -e "${prog}.inp" ]; then
    echo; echo "=== $prog ==============================================="
    "./$prog" 2>&1 | tee "${prog}.out"
  else
    echo "-- skip $prog (no ${prog}.nml / .inp in this test)"
  fi
}

run ww3_grid    # bathymetry+spectrum -> mod_def.ww3   (the model definition blob)
run ww3_strt    # initial conditions  -> restart.ww3
run ww3_prnc    # netCDF forcing      -> wind.ww3 / current.ww3 / ice.ww3
run ww3_bounc   # spectral boundaries -> nest.ww3
run ww3_shel    # THE MODEL           -> out_grd.ww3, out_pnt.ww3, log.ww3
run ww3_ounf    # gridded output      -> ww3.*.nc
run ww3_ounp    # spectral output     -> ww3.*_spec.nc

echo
echo ">> Work dir: $WORK"
echo ">> Compare against reference: $WW3DIR/regtests/$TEST/  (see the test's info file)"
ls -la *.nc 2>/dev/null || echo "   (no netCDF output -- is NC4 in your switch file?)"
