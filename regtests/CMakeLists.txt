# The Kokkos DIA (W3SNL1) through WW3's own regtest harness (WFIP-0003), one CTest test per entry
# of $WW3/regtests/bin/matrix.base. These are WW3's regtests, not tests of the port (ADR-0010):
# each entry is run twice with run_cmake_test on one executable built with the KOKKOS switch,
# WW_KOKKOS_SNL1=0 (the Fortran DIA) into work_F_<n> and WW_KOKKOS_SNL1=1 (the Kokkos DIA) into
# work_K_<n>, and the pair is compared with WW3's test.comp (bytes) and with nccmp-tol
# (kokkos/tools/nccmp-tol/tolerances.txt) on every ww3.*.nc both runs wrote (cmake/run_entry.cmake).
#
#   just hoff-matrix [jobs] [serial|omp|mpi|all] [ctest args]  configure build/hoff-matrix-<selection>
#   just hoff-matrix-list [serial|omp|mpi|all]                 the entries, without running them
#   just hoff-matrix-fetch                              WW3's ftp data: pratico's ww3-regtest-data
#   just hoff-matrix-selftest                           the parsers and the verdict on fixtures
#
# Configure options:
#   HOFF_SELECTION    every entry whose switch has NL1 is in exactly one of serial (none of MPI,
#                     OMPG, OMPH), omp (OMPG or OMPH, no MPI) and mpi (MPI); all is the three.
#                     ww3_multi entries (-m) are in: the shim keeps one context per grid.
#   HOFF_WW3          the h0ffmann/WW3 hoff/kokkos-snl1 tree; never written (the inputs are staged
#                     in <build>/regtests/, and record:summary fails when `git status` shows a change)
#   HOFF_DATA         WW3's ftp data, <HOFF_DATA>/data_regtests: the store path of
#                     `nix build ./nix-config/labs/pratico#ww3-regtest-data` (`just hoff-matrix` passes
#                     it); its version must be ww3ver of WW3's model/bin/ww3_from_ftp.sh
#   HOFF_STRICT       a file test.comp finds different fails the entry (the DIA is byte-identical
#                     on every entry since 2026-10-15, kokkos/port_report/W3SNL1_KOKKOS.md)
#   HOFF_OMP_THREADS  OMP_NUM_THREADS of each run, matrix.base's $np on an OpenMP line, its $nth
#   HOFF_MPI_RANKS    matrix.base's $np and $nr on an MPI line (-p $mpi, mpirun)
#   The PDLIB entries build against SCOTCH_PATH and METIS_PATH, which the pratico ww3 shell
#   exports (PT-Scotch static, METIS + ParMETIS); ctest runs in that shell.
#   HOFF_OMP_NESTED   export OMP_MAX_ACTIVE_LEVELS=2 for both runs of an entry (default OFF). Kokkos
#                     5.2.0's OpenMP backend aborts when the shim is called inside W3WAVE's
#                     !$OMP PARALLEL region with the limit at 1 ("Nested parallelism requires the
#                     maximum active levels to be larger than 1"), so the omp selection needs it
#                     until the shim runs Kokkos::Serial there (docs/data/hoff_matrix_omp_2026-10-15.tsv)
#
# Tests: setup:kokkos and setup:stage (FIXTURES_SETUP hoff), NNN:<test>:<switch> per entry
# (LABELS entry and the selection), record:summary (FIXTURES_CLEANUP hoff: <build>/summary.tsv),
# selection:count (the number of entries the pin is expected to give). CPU only: there is no GPU
# path here, GPU runs are the owner's (AGENTS.md, "Running on the GPU").
cmake_minimum_required(VERSION 3.21)
project(hoff_matrix NONE)

get_filename_component(HOFF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/hoff_common.cmake")

set(HOFF_SELECTION serial CACHE STRING "Which matrix.base entries: serial, omp, mpi or all")
set_property(CACHE HOFF_SELECTION PROPERTY STRINGS serial omp mpi all)
set(HOFF_WW3 "${HOFF_ROOT}/WW3" CACHE PATH "The WW3 tree (h0ffmann/WW3, hoff/kokkos-snl1), read-only")
option(HOFF_STRICT "A test.comp difference fails the entry" ON)
option(HOFF_OMP_NESTED "Export OMP_MAX_ACTIVE_LEVELS=2 for both runs of an entry" OFF)
cmake_host_system_information(RESULT _ncpu QUERY NUMBER_OF_LOGICAL_CORES)
math(EXPR _nthr "${_ncpu} / 8")
if(_nthr LESS 1)
  set(_nthr 1)
endif()
set(HOFF_OMP_THREADS "${_nthr}" CACHE STRING "OMP_NUM_THREADS per run and matrix.base's $np")
set(HOFF_MPI_RANKS 4 CACHE STRING "matrix.base's $np and $nr on an MPI line")

if(NOT HOFF_SELECTION MATCHES "^(serial|omp|mpi|all)$")
  message(FATAL_ERROR "HOFF_SELECTION is serial, omp, mpi or all, not '${HOFF_SELECTION}'")
endif()
if(NOT HOFF_MPI_RANKS MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "HOFF_MPI_RANKS is a positive integer, not '${HOFF_MPI_RANKS}'")
endif()
if(NOT HOFF_OMP_THREADS MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "HOFF_OMP_THREADS is a positive integer, not '${HOFF_OMP_THREADS}'")
endif()
set(_base "${HOFF_WW3}/regtests/bin/matrix.base")
if(NOT EXISTS "${_base}")
  message(FATAL_ERROR "${HOFF_WW3} has no regtests/bin/matrix.base: git submodule update --init WW3")
endif()
if(NOT EXISTS "${HOFF_WW3}/model/src/w3kokkosmd.F90")
  message(FATAL_ERROR "${HOFF_WW3} is not the hoff/kokkos-snl1 branch of h0ffmann/WW3 (no model/src/w3kokkosmd.F90)")
endif()
hoff_ftp_version(_ftpver _ftpurl "${HOFF_WW3}/model/bin/ww3_from_ftp.sh")
set(HOFF_DATA "" CACHE PATH "WW3's ftp data, <HOFF_DATA>/data_regtests (pratico's ww3-regtest-data)")
# Two ifs: an if() expands ${CMAKE_MATCH_1} before its own MATCHES has run.
if(HOFF_DATA MATCHES "-ww3-regtest-data-([0-9.]+)$")
  set(_dataver "v${CMAKE_MATCH_1}")
  if(NOT _dataver STREQUAL _ftpver)
    message(FATAL_ERROR "${HOFF_DATA} is data ${_dataver}, and WW3's ww3_from_ftp.sh wants ${_ftpver}: "
                        "bump ww3RegtestData in nix-config/labs/pratico/flake.nix with the WW3 pin")
  endif()
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_base}")

# The number of entries the WW3 pin gives (WW3@6d1bcdfe): selection:count fails when a pin bump
# changes it, so the change is read before its table is.
set(_expected_serial 100)
set(_expected_omp 50)
set(_expected_mpi 161)
set(_expected_all 311)
set(HOFF_EXPECTED "${_expected_${HOFF_SELECTION}}")

hoff_select_entries(E "${_base}" "${HOFF_WW3}/regtests" "${HOFF_SELECTION}")
list(LENGTH E_TESTS _count)
message(STATUS "hoff-matrix: ${_count} ${HOFF_SELECTION} entries of ${_base}")

set(_kbuild "${HOFF_ROOT}/kokkos/build/openmp-release")
set(_config "${CMAKE_CURRENT_BINARY_DIR}/hoff_config.cmake")
file(WRITE "${_config}" "# written by ${CMAKE_CURRENT_LIST_FILE}; reconfigure to change\n")
foreach(kv
    "HOFF_ROOT;${HOFF_ROOT}" "HOFF_WW3;${HOFF_WW3}" "HOFF_DATA;${HOFF_DATA}"
    "HOFF_SELECTION;${HOFF_SELECTION}" "HOFF_STRICT;${HOFF_STRICT}" "HOFF_OMP_NESTED;${HOFF_OMP_NESTED}"
    "HOFF_OMP_THREADS;${HOFF_OMP_THREADS}" "HOFF_MPI_RANKS;${HOFF_MPI_RANKS}" "HOFF_EXPECTED;${HOFF_EXPECTED}"
    "HOFF_OUT;${CMAKE_CURRENT_BINARY_DIR}" "HOFF_STAGE;${CMAKE_CURRENT_BINARY_DIR}/regtests"
    "HOFF_KBUILD;${_kbuild}" "HOFF_KPREFIX;${_kbuild}/install"
    "HOFF_NCCMP;${_kbuild}/tools/nccmp-tol/nccmp-tol"
    "HOFF_TOLS;${HOFF_ROOT}/kokkos/tools/nccmp-tol/tolerances.txt")
  list(GET kv 0 k)
  list(GET kv 1 v)
  file(APPEND "${_config}" "set(${k} [==[${v}]==])\n")
endforeach()

set(_cm "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
enable_testing()

add_test(NAME selection:count
  COMMAND "${CMAKE_COMMAND}" "-DCONFIG=${_config}" -P "${_cm}/count.cmake")
set_tests_properties(selection:count PROPERTIES LABELS "${HOFF_SELECTION}")

add_test(NAME setup:kokkos
  COMMAND "${CMAKE_COMMAND}" "-DCONFIG=${_config}" -P "${_cm}/setup_kokkos.cmake")
add_test(NAME setup:stage
  COMMAND "${CMAKE_COMMAND}" "-DCONFIG=${_config}" -P "${_cm}/stage.cmake")
set_tests_properties(setup:kokkos setup:stage PROPERTIES FIXTURES_SETUP hoff LABELS "${HOFF_SELECTION}")
add_test(NAME record:summary
  COMMAND "${CMAKE_COMMAND}" "-DCONFIG=${_config}" -P "${_cm}/summary.cmake")
set_tests_properties(record:summary PROPERTIES FIXTURES_CLEANUP hoff LABELS "${HOFF_SELECTION}")

set(_n 0)
foreach(test IN LISTS E_TESTS)
  list(GET E_OPTS ${_n} opts)
  math(EXPR _n "${_n} + 1")
  hoff_words(o "${opts}")
  hoff_opt_of(sw -s ${o})
  if(sw STREQUAL "")
    set(sw default)
  endif()
  string(LENGTH "${_n}" _l)
  set(nnn "${_n}")
  while(_l LESS 3)
    string(PREPEND nnn 0)
    math(EXPR _l "${_l} + 1")
  endwhile()
  set(name "${nnn}:${test}:${sw}")
  add_test(NAME "${name}"
    COMMAND "${CMAKE_COMMAND}" "-DCONFIG=${_config}" "-DN=${_n}" "-DTEST=${test}" "-DOPTS=${opts}"
            -P "${_cm}/run_entry.cmake")
  set_tests_properties("${name}" PROPERTIES FIXTURES_REQUIRED hoff LABELS "entry;${HOFF_SELECTION}")
endforeach()
