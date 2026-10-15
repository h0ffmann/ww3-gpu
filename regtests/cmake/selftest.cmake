# The self-test of the regtest matrix's logic (cmake -P selftest.cmake, `just hoff-matrix-selftest`),
# on fixtures: the matrix.base selection (serial, omp, mpi, all), the run_cmake_test argument list
# and the expansion of $mpi, $np, $nr, $nth and $(( )), has_ounf and opt_of, the row verdict, the
# nccmp-tol parser, the ftp script's version, URL and cp lines, and the data staging. With WW3/
# checked out it also asserts the counts at the pin, 100 serial, 50 omp, 161 mpi and 311 in all.
# Runs nothing of WW3.
cmake_minimum_required(VERSION 3.21)
include("${CMAKE_CURRENT_LIST_DIR}/hoff_common.cmake")

if(DEFINED ENV{TMPDIR})
  set(base "$ENV{TMPDIR}")
else()
  set(base /tmp)
endif()
string(RANDOM LENGTH 8 rnd)
set(t "${base}/hoff_selftest_${rnd}")
file(MAKE_DIRECTORY "${t}")

macro(fail what)
  file(REMOVE_RECURSE "${t}")
  message(FATAL_ERROR "hoff_matrix self-test: ${what}")
endmacro()
macro(expect got want what)
  if(NOT "${got}" STREQUAL "${want}")
    file(REMOVE_RECURSE "${t}")
    message(FATAL_ERROR "hoff_matrix self-test: ${what}\n  got:  ${got}\n  want: ${want}")
  endif()
endmacro()

# --- selection -----------------------------------------------------------------------------
file(MAKE_DIRECTORY "${t}/rt/bin" "${t}/rt/a/input" "${t}/rt/a/input2" "${t}/rt/b/input")
file(WRITE "${t}/rt/a/input/switch_K1" "F90 SHRD ST4 NL1 FLX0\n")
file(WRITE "${t}/rt/a/input/switch_N3" "F90 SHRD ST4 NL3 FLX0\n")
file(WRITE "${t}/rt/a/input/switch_MP" "F90 DIST MPI ST4 NL1\n")
file(WRITE "${t}/rt/a/input/switch_OH" "F90 SHRD OMPH ST4 NL1\n")
file(WRITE "${t}/rt/a/input/switch_NX" "F90 SHRD ST4 NL1X\n")
file(WRITE "${t}/rt/a/input2/switch" "F90 SHRD ST4 NL1\n")
file(WRITE "${t}/rt/b/input/switch_K1" "F90 SHRD ST4 NL1\n")
file(WRITE "${t}/rt/b/input/switch_OG" "F90 SHRD OMPG ST4 NL1\n")
file(WRITE "${t}/rt/b/input/switch_O3" "F90 SHRD OMPG ST4 NL3\n")
file(WRITE "${t}/rt/b/input/switch_MH" "F90 DIST MPI OMPH ST4 NL1\n")
file(WRITE "${t}/rt/b/input/switch_PD" "F90 DIST MPI PDLIB ST4 NL1\n")
file(WRITE "${t}/rt/bin/matrix.base" [=[
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
    echo "$rtst -s OG -w work_OG -f -O -n $np $ww3 b" >> matrix.body
    echo "$rtst -s OG -w work_OGm -m grdset_a -f -O -n $np $ww3 b" >> matrix.body
    echo "$rtst -s O3 -w work_O3 -f -O -n $np $ww3 b" >> matrix.body
    echo "$rtst -s MH -w work_MH -f -p $mpi -n $nr -t $nth $ww3 b" >> matrix.body
    echo "$rtst -s NOFILE -s PD -w work_PD -f -p $mpi -n $(( $np -1 )) $ww3 b" >> matrix.body
]=])
hoff_select_entries(S "${t}/rt/bin/matrix.base" "${t}/rt" serial)
expect("${S_TESTS}" "a;a;b;b;a;b" "serial selection, tests")
expect("${S_OPTS}" "-s K1 -w work_K1;-s K1 -w work_K1g -g G2 -N -o both;-s K1 -p $mpi -n $np -w work_P;-s K1 -m grdset -w work_M;-i input2 -w work_I;-s K1 -w work_B"
       "serial selection, options")
hoff_select_entries(O "${t}/rt/bin/matrix.base" "${t}/rt" omp)
expect("${O_TESTS}" "a;b;b" "omp selection, tests")
expect("${O_OPTS}" "-s OH -w work_OH;-s OG -w work_OG -f -O -n $np;-s OG -w work_OGm -m grdset_a -f -O -n $np"
       "omp selection, options")
hoff_select_entries(M "${t}/rt/bin/matrix.base" "${t}/rt" mpi)
expect("${M_TESTS}" "a;b;b" "mpi selection, tests (the last -s names the switch)")
expect("${M_OPTS}" "-s MP -w work_MP;-s MH -w work_MH -f -p $mpi -n $nr -t $nth;-s NOFILE -s PD -w work_PD -f -p $mpi -n $(( $np -1 ))"
       "mpi selection, options")
hoff_select_entries(A "${t}/rt/bin/matrix.base" "${t}/rt" all)
list(LENGTH A_TESTS na)
expect("${na}" "12" "all selection, the three together")

# --- the run_cmake_test argument list, $np -> threads, -o netcdf ----------------------------
hoff_entry_args(a 1 K b "-s OG -w work_OG -f -O -n $np" "${t}" 3)
expect("${a}" "-f;-O;-n;3;-s;OG_KOKKOS;-w;work_K_1;${t}/model;b" "entry args, omp entry, $np = 3")
file(WRITE "${t}/rt/a/input/ww3_ounf.nml" "")
file(MAKE_DIRECTORY "${t}/regtests/a/input")
file(WRITE "${t}/regtests/a/input/ww3_ounf.nml" "")
hoff_entry_args(a 2 F a "-s K1 -w work_K1" "${t}" 4)
expect("${a}" "-o;netcdf;-s;K1_KOKKOS;-w;work_F_2;${t}/model;a" "entry args, -o netcdf added")
hoff_entry_args(a 2 F a "-s K1 -w work_K1g -g G2 -N -o both" "${t}" 4)
expect("${a}" "-g;G2;-N;-o;both;-s;K1_KOKKOS;-w;work_F_2;${t}/model;a" "entry args, -o kept")
hoff_entry_args(a 4 K b "-s MH -w work_MH -f -p $mpi -n $nr -t $nth" "${t}" 2 3)
expect("${a}" "-f;-p;mpirun;-n;3;-t;2;-s;MH_KOKKOS;-w;work_K_4;${t}/model;b" "entry args, hybrid MPI entry")
hoff_entry_args(a 5 F b "-s NOFILE -s PD -w work_PD -f -p $mpi -n $(( $np -1 ))" "${t}" 1 4)
expect("${a}" "-f;-p;mpirun;-n;3;-s;PD_KOKKOS;-w;work_F_5;${t}/model;b" "entry args, $(( $np -1 )) and the last -s")
hoff_entry_args(a 3 K a "-i input2 -w work_I" "${t}" 4)
expect("${a}" "-i;input2;-s;KOKKOS;-w;work_K_3;${t}/model;a" "entry args, no -s, no ounf in input2")
hoff_has_ounf(y "${t}/rt/a/input")
hoff_has_ounf(n "${t}/rt/b/input")
if(NOT y OR n)
  fail("has_ounf")
endif()
hoff_opt_of(w -w -s K1 -w work_Z -g G)
hoff_opt_of(o -o -s K1)
hoff_opt_of(l -s -s MPI -s PDLIB -w x)
expect("${w}|${o}|${l}" "work_Z||PDLIB" "opt_of")

# --- the row verdict ---------------------------------------------------------------------
set(ok "1\ta\tK1\tyes\tyes\tyes\t0/9\tPASS\ths,fp\t10\t7\t-s K1")
set(diff "1\ta\tK1\tyes\tyes\tyes\t3/9\tPASS\ths,fp\t10\t7\t-s K1")
foreach(c
    "pass|OFF|0|${ok}"
    "neither-ran|OFF|1|2\ta\tK2\tno\tno\tno\tn/a\tn/a\t-\t5\t0\t-s K2"
    "fortran-missing|OFF|1|1\ta\tK1\tyes\tkokkos-only\tyes\tn/a\tn/a\t-\t10\t7\t-s K1"
    "gqm-no-spectra|ON|0|1\ta\tK1\tyes\tyes\tyes\t0/9\tPASS\ths\t10\t0\t-s K1"
    "nccmp-fail|OFF|1|1\ta\tK1\tyes\tyes\tyes\t0/9\tFAIL\ths\t10\t0\t-s K1"
    "nc-missing|OFF|1|1\ta\tK1\tyes\tyes\tyes\t0/9\tMISSING\t-\t10\t0\t-s K1"
    "kokkos-missing|OFF|1|1\ta\tK1\tyes\tfortran-only\tno\tn/a\tn/a\t-\t10\t0\t-s K1"
    "no-kokkos-log|OFF|1|1\ta\tK1\tyes\tyes\tno\t0/9\tPASS\ths\t10\t0\t-s K1"
    "test.comp-diff-lax|OFF|0|${diff}"
    "test.comp-diff-strict|ON|1|${diff}"
    "test.comp-same-strict|ON|0|${ok}")
  string(REPLACE "|" ";" c "${c}")
  list(GET c 0 name)
  list(GET c 1 strict)
  list(GET c 2 want)
  list(GET c 3 row)
  hoff_row_verdict(why "${row}" ${strict})
  if(why STREQUAL "")
    set(got 0)
  else()
    set(got 1)
  endif()
  expect("${got}" "${want}" "verdict '${name}' (${why})")
endforeach()

# --- the nccmp-tol parser ------------------------------------------------------------------
set(out "variable   n  dropped  max|d|  rms  max rel  verdict\n-----------\nhs 1 0 0 0 0  PASS\nfp 1 0 0 0 0  FAIL\nlat 1 0 0 0 0  unlisted\nnccmp-tol: note: 'dir' is listed but not in both files\nnccmp-tol: FAIL (2 judged, 1 unlisted)\n")
hoff_judged_vars(j "${out}")
expect("${j}" "hs,fp" "judged_vars")

# --- WW3's ftp script: version, URL, cp lines, staging ------------------------------------
file(MAKE_DIRECTORY "${t}/w/model/bin")
file(WRITE "${t}/w/model/bin/ww3_from_ftp.sh" [=[
ww3ver=v1.2.3
  wget --no-check-certificate https://ftp.emc.ncep.noaa.gov/static_files/public/WW3/ww3_from_ftp.${ww3ver}.tar.gz
cp -r data_regtests/a/input/*.nc  regtests/a/input/
cp -r data_regtests/a/input/w.nc regtests/a/input2/r.nc
cp -r data_regtests/z/input/* regtests/z/input/
# cp -r data_regtests/a/x regtests/a/
]=])
set(ftp "${t}/w/model/bin/ww3_from_ftp.sh")
hoff_ftp_version(v u "${ftp}")
expect("${v}|${u}" "v1.2.3|https://ftp.emc.ncep.noaa.gov/static_files/public/WW3/ww3_from_ftp.v1.2.3.tar.gz" "ftp version and URL")
hoff_ftp_copies(cp "${ftp}")
expect("${cp_SRC}|${cp_DST}" "a/input/*.nc;a/input/w.nc;z/input/*|a/input/;a/input2/r.nc;z/input/" "ftp cp lines")
file(MAKE_DIRECTORY "${t}/cache/a/input")
file(WRITE "${t}/cache/a/input/w.nc" "w")
file(WRITE "${t}/cache/a/input/v.nc" "v")
hoff_stage_data(n "${t}/cache" "${t}/st" a "${ftp}")
if(NOT n EQUAL 3 OR NOT EXISTS "${t}/st/a/input/v.nc" OR NOT EXISTS "${t}/st/a/input/w.nc"
   OR NOT EXISTS "${t}/st/a/input2/r.nc" OR IS_DIRECTORY "${t}/st/a/input2/r.nc")
  fail("stage_data copied ${n}")
endif()
hoff_stage_data(n "${t}/nocache" "${t}/st" a "${ftp}")
expect("${n}" "-1" "stage_data with the data needed and no cache")
hoff_stage_data(n "${t}/nocache" "${t}/st" b "${ftp}")
expect("${n}" "0" "stage_data for a test that needs no data")

# --- the pin -------------------------------------------------------------------------------
get_filename_component(ww3 "${CMAKE_CURRENT_LIST_DIR}/../../WW3" ABSOLUTE)
if(EXISTS "${ww3}/regtests/bin/matrix.base")
  execute_process(COMMAND git -C "${ww3}" rev-parse --short HEAD OUTPUT_VARIABLE rev OUTPUT_STRIP_TRAILING_WHITESPACE)
  foreach(kv "serial;100" "omp;50" "mpi;161" "all;311")
    list(GET kv 0 k)
    list(GET kv 1 want)
    hoff_select_entries(P "${ww3}/regtests/bin/matrix.base" "${ww3}/regtests" ${k})
    list(LENGTH P_TESTS n)
    expect("${n}" "${want}" "WW3@${rev} matrix.base, ${k} entries")
    message(STATUS "hoff_matrix: WW3@${rev} matrix.base: ${n} ${k} entries")
  endforeach()
endif()

file(REMOVE_RECURSE "${t}")
message(STATUS "hoff_matrix self-test: ok")
