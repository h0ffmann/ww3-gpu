# record:summary (FIXTURES_CLEANUP): assemble <build>/summary.tsv from the rows the entries of
# this run wrote, under a header naming the date, both commits, the machine, the command and the
# selection; then fail if WW3/ was written (`git -C WW3 status --porcelain` not empty).
# Columns: n, test, switch, built, ran, kokkos_log, test_comp (files differing/compared, WW3's
# test.comp), nccmp_tol (PASS/FAIL/MISSING/n/a, kokkos/tools/nccmp-tol/tolerances.txt), judged
# variables, wall_s, kokkos_spectra (the spectra the Kokkos DIA computed, summed over the
# processes; 0 when the grid calls GQM), ww3_self (files two Fortran runs of the same executable
# write differently, - when not measured: test_comp counts only the others), and the entry's matrix.base options, which tell apart entries of one test
# and switch. nccmp_tol is n/a when the test has no ww3_ounf input, MISSING when it has one and
# either run wrote no ww3.*.nc.
cmake_minimum_required(VERSION 3.21)
include("${CONFIG}")
include("${CMAKE_CURRENT_LIST_DIR}/hoff_common.cmake")

function(sh out)
  execute_process(COMMAND ${ARGN} OUTPUT_VARIABLE o ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(${out} "${o}" PARENT_SCOPE)
endfunction()

sh(date date -u +%Y-%m-%dT%H:%M:%SZ) # not string(TIMESTAMP): the Nix shell's SOURCE_DATE_EPOCH
sh(rev git -C "${HOFF_ROOT}" rev-parse --short HEAD)
execute_process(COMMAND git -C "${HOFF_ROOT}" diff --quiet HEAD -- . ":!WW3" RESULT_VARIABLE dirty)
if(NOT dirty EQUAL 0)
  string(APPEND rev " (dirty)")
endif()
sh(wrev git -C "${HOFF_WW3}" rev-parse --short HEAD)
sh(wbr git -C "${HOFF_WW3}" branch -r --contains HEAD)
string(REGEX REPLACE "[ \t]*\n[ \t]*" "," wbr "${wbr}")
string(STRIP "${wbr}" wbr)
sh(model lscpu)
if(model MATCHES "Model name:[ \t]*([^\n]*)")
  set(model "${CMAKE_MATCH_1}")
else()
  cmake_host_system_information(RESULT model QUERY PROCESSOR_DESCRIPTION)
endif()
sh(nall nproc --all)
set(omp "OMP_NUM_THREADS=${HOFF_OMP_THREADS} per run")
if(HOFF_SELECTION MATCHES "^(mpi|all)$")
  string(APPEND omp ", ${HOFF_MPI_RANKS} MPI ranks per MPI run")
endif()
if(HOFF_OMP_NESTED)
  string(APPEND omp ", HOFF_OMP_NESTED=ON: OMP_MAX_ACTIVE_LEVELS=2 for both runs")
elseif(DEFINED ENV{OMP_MAX_ACTIVE_LEVELS})
  string(APPEND omp ", HOFF_OMP_NESTED=OFF, OMP_MAX_ACTIVE_LEVELS=$ENV{OMP_MAX_ACTIVE_LEVELS} inherited")
else()
  string(APPEND omp ", HOFF_OMP_NESTED=OFF")
endif()
if(DEFINED ENV{HOFF_COMMAND})
  set(cmd "$ENV{HOFF_COMMAND}")
else()
  set(cmd "ctest --test-dir ${HOFF_OUT}")
endif()
hoff_sel_desc(desc "${HOFF_SELECTION}")

set(t "# date: ${date}\n")
string(APPEND t "# ww3-gpu commit: ${rev}\n")
string(APPEND t "# WW3 commit: ${wrev} (${wbr})\n")
string(APPEND t "# machine: ${model}, ${nall} hardware threads (nproc --all), ${omp}\n")
string(APPEND t "# command: ${cmd}\n")
string(APPEND t "# data: ${HOFF_DATA}; SCOTCH_PATH: $ENV{SCOTCH_PATH}; METIS_PATH: $ENV{METIS_PATH}\n")
string(APPEND t "# selection: ${HOFF_SELECTION}, ${desc}; HOFF_STRICT=${HOFF_STRICT}\n")
string(APPEND t "n\ttest\tswitch\tbuilt\tran\tkokkos_log\ttest_comp\tnccmp_tol\tjudged\twall_s\tkokkos_spectra\tww3_self\toptions\n")
file(GLOB rows "${HOFF_OUT}/rows/*.tsv")
list(SORT rows COMPARE NATURAL)
set(nrows 0)
foreach(r IN LISTS rows)
  file(READ "${r}" s)
  string(APPEND t "${s}")
  math(EXPR nrows "${nrows} + 1")
endforeach()
file(WRITE "${HOFF_OUT}/summary.tsv" "${t}")
message("${t}")
message(STATUS "${nrows} row(s) in ${HOFF_OUT}/summary.tsv; logs in ${HOFF_OUT}/logs/")

sh(st git -C "${HOFF_WW3}" status --porcelain)
if(NOT st STREQUAL "")
  message(FATAL_ERROR "hoff_matrix: WW3/ was written (git -C WW3 status --porcelain):\n${st}")
endif()
