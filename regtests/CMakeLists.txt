# The Kokkos port (W3SNL1, ST4) through WW3's own regtest harness (WFIP-0003), one CTest test per
# entry of $WW3/regtests/bin/matrix.base. These are WW3's regtests, not tests of the port
# (ADR-0010): each entry is run twice with run_cmake_test on one executable built with the KOKKOS
# switch, every WW_KOKKOS_<R>=0 (the Fortran) into work_F_<n> and WW_KOKKOS_<R>=1 for the routines
# of WW3GPU_ROUTINES (the Kokkos) into work_K_<n>, and the pair is compared with WW3's test.comp
# (bytes) and with nccmp-tol (kokkos/tools/nccmp-tol/tolerances.txt) on every ww3.*.nc both runs
# wrote (cmake/run_entry.cmake).
#
#   just ww3-gpu-matrix [jobs] [serial|omp|mpi|all] [ctest args]  configure build/ww3-gpu-matrix-<selection>
#   just ww3-gpu-matrix-list [serial|omp|mpi|all]                 the entries, without running them
#   just ww3-gpu-matrix-fetch                              WW3's ftp data: pratico's ww3-regtest-data
#   just ww3-gpu-matrix-selftest                           the parsers and the verdict on fixtures
#
# Configure options:
#   WW3GPU_SELECTION    every entry whose switch has a physics word of WW3GPU_ROUTINES is in exactly
#                     one of serial (none of MPI, OMPG, OMPH), omp (OMPG or OMPH, no MPI) and mpi
#                     (MPI); all is the three. ww3_multi entries (-m) are in: the shims keep one
#                     context per grid.
#   WW3GPU_ROUTINES     the routines whose WW_KOKKOS_<R> flag the Kokkos run sets to 1, a list of SNL1
#                     (selects NL1 switches), SDS4, SIN4, SPR4 (select ST4), SPR0 (ST0), SPRn, SINn,
#                     SDSn for n = 1, 2, 3, 6 (STn), SWL6 (ST6), FLX1..FLX5 (FLXn); SNL1 by default
#                     (WW3GPU_ALL_ROUTINES, cmake/ww3gpu_common.cmake). The Fortran runs set every
#                     flag to 0. ADR-0011's ST4 checkpoint runs one at a time.
#   WW3GPU_KOKKOS       empty (the default): the Kokkos run sets WW3GPU_ROUTINES as above. Otherwise
#                     the flags of scripts/ww3-gpu.sh --kokkos, so a regtest and a timed run share
#                     them: resident (SRCE,XYP3,KTP3,OUTG,IOPE,UPDV), all (WW_KOKKOS_ALL=1), none,
#                     or a comma list of the parts of kokkos/src/fortran_iface/parts.d; the
#                     selection is then WW3GPU_SWITCH_WORDS alone, and the Kokkos run must show
#                     each part as <PART>=K in W3INIT's parts line
#   WW3GPU_SWITCH_WORDS words every selected switch has, !WORD for one it must not have; for
#                     resident by default ST4;NL1;PR3;UQ;SHRD;!PDLIB;!MGP;!MGG (W3SRCE's Kokkos
#                     path needs ST4 and NL1 and refuses PDLIB; W3XYP3 needs PR3, UQ and SHRD and
#                     refuses MGP and MGG, w3kokkos_prop3md.F90:112-134 at WW3@80644ba9; it also
#                     refuses an unstructured or tripole grid, which no switch word says)
#   WW3GPU_LAB_SWITCH   OFF by default. ON: both runs of an entry build its switch with the
#                     source-term words replaced by switches/switch_lab_shrd's (LN1 ST4 NL1 BT1
#                     DB1 MLIM ...), the only set the Kokkos W3SRCE translates; no matrix.base
#                     switch has it. The entry keeps its inputs, grid and propagation; its outputs
#                     are no longer WW3's reference outputs, and only the pair is compared
#   WW3GPU_WW3          the h0ffmann/WW3 ww3-gpu/kokkos-st4 tree; never written (the inputs are staged
#                     in <build>/regtests/, and record:summary fails when `git status` shows a change)
#   WW3GPU_DATA         WW3's ftp data, <WW3GPU_DATA>/data_regtests: the store path of
#                     `nix build ./picasso/labs/pratico#ww3-regtest-data` (`just ww3-gpu-matrix` passes
#                     it); its version must be ww3ver of WW3's model/bin/ww3_from_ftp.sh
#   WW3GPU_STRICT       a file test.comp finds different fails the entry (the DIA is byte-identical
#                     on every entry since 2026-10-15, kokkos/port_report/W3SNL1_KOKKOS.md)
#   WW3GPU_OMP_THREADS  OMP_NUM_THREADS of each run, matrix.base's $np on an OpenMP line, its $nth
#   WW3GPU_MPI_RANKS    matrix.base's $np and $nr on an MPI line (-p $mpi, mpirun)
#   The PDLIB entries build against SCOTCH_PATH and METIS_PATH, which the pratico ww3 shell
#   exports (PT-Scotch static, METIS + ParMETIS); ctest runs in that shell.
#   WW3GPU_BACKEND      the Kokkos backend of every entry's build: openmp (the default, the agents'
#                     regtest) or cuda (the kokkos/ cuda-release preset, WW3 built with nvcc_wrapper),
#                     which runs on the GPU and so only under the owner's supervision
#                     (`just ww3-gpu-matrix-cuda`, AGENTS.md "Running on the GPU")
#   WW3GPU_OMP_NESTED   export OMP_MAX_ACTIVE_LEVELS=2 for both runs of an entry (default OFF). Kokkos
#                     5.2.0's OpenMP backend aborts when the shim is called inside W3WAVE's
#                     !$OMP PARALLEL region with the limit at 1 ("Nested parallelism requires the
#                     maximum active levels to be larger than 1"), so the omp selection needs it
#                     until the shim runs Kokkos::Serial there (docs/data/hoff_matrix_omp_2026-10-15.tsv)
#
# Tests: setup:kokkos and setup:stage (FIXTURES_SETUP ww3-gpu), NNN:<test>:<switch> per entry
# (LABELS entry and the selection), record:summary (FIXTURES_CLEANUP ww3-gpu: <build>/summary.tsv),
# selection:count (the number of entries the pin is expected to give). CPU by default; with
# WW3GPU_BACKEND=cuda the Kokkos runs are on the GPU, which are the owner's (AGENTS.md, "Running on
# the GPU").
cmake_minimum_required(VERSION 3.21)
project(ww3gpu_matrix NONE)

get_filename_component(WW3GPU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ww3gpu_common.cmake")

set(WW3GPU_SELECTION serial CACHE STRING "Which matrix.base entries: serial, omp, mpi or all")
set_property(CACHE WW3GPU_SELECTION PROPERTY STRINGS serial omp mpi all)
set(WW3GPU_ROUTINES SNL1 CACHE STRING "The routines the Kokkos run turns on: a list of WW3GPU_ALL_ROUTINES")
set(WW3GPU_WW3 "${WW3GPU_ROOT}/WW3" CACHE PATH "The WW3 tree (h0ffmann/WW3, ww3-gpu/kokkos-st4), read-only")
set(WW3GPU_BACKEND openmp CACHE STRING "The Kokkos backend: openmp or cuda")
set_property(CACHE WW3GPU_BACKEND PROPERTY STRINGS openmp cuda)
if(NOT WW3GPU_BACKEND MATCHES "^(openmp|cuda)$")
  message(FATAL_ERROR "WW3GPU_BACKEND is openmp or cuda, not '${WW3GPU_BACKEND}'")
endif()
option(WW3GPU_STRICT "A test.comp difference fails the entry" ON)
option(WW3GPU_OMP_NESTED "Export OMP_MAX_ACTIVE_LEVELS=2 for both runs of an entry" OFF)
cmake_host_system_information(RESULT _ncpu QUERY NUMBER_OF_LOGICAL_CORES)
math(EXPR _nthr "${_ncpu} / 8")
if(_nthr LESS 1)
  set(_nthr 1)
endif()
set(WW3GPU_OMP_THREADS "${_nthr}" CACHE STRING "OMP_NUM_THREADS per run and matrix.base's $np")
set(WW3GPU_MPI_RANKS 4 CACHE STRING "matrix.base's $np and $nr on an MPI line")

if(NOT WW3GPU_SELECTION MATCHES "^(serial|omp|mpi|all)$")
  message(FATAL_ERROR "WW3GPU_SELECTION is serial, omp, mpi or all, not '${WW3GPU_SELECTION}'")
endif()
string(REPLACE "," ";" WW3GPU_ROUTINES "${WW3GPU_ROUTINES}")
if(WW3GPU_ROUTINES STREQUAL "")
  message(FATAL_ERROR "WW3GPU_ROUTINES is empty")
endif()
foreach(_r IN LISTS WW3GPU_ROUTINES)
  ww3gpu_routine_word(_w "${_r}")
  if(_w STREQUAL "" OR NOT _r IN_LIST WW3GPU_ALL_ROUTINES)
    list(JOIN WW3GPU_ALL_ROUTINES ", " _all)
    message(FATAL_ERROR "WW3GPU_ROUTINES: '${_r}' is not one of ${_all}")
  endif()
endforeach()
ww3gpu_physics(WW3GPU_PHYSICS ${WW3GPU_ROUTINES})
set(WW3GPU_KOKKOS "" CACHE STRING "The flags of ww3-gpu.sh --kokkos (resident, all, none, PART,...); empty: WW3GPU_ROUTINES")
option(WW3GPU_LAB_SWITCH "Build every entry with switch_lab_shrd's source-term words" OFF)
set(WW3GPU_SWITCH_WORDS "" CACHE STRING "Words every selected switch has, !WORD for one it must not")
ww3gpu_parts_d(WW3GPU_PARTS WW3GPU_PART_COUNTS "${WW3GPU_ROOT}/kokkos/src/fortran_iface/parts.d")
# Every flag a run unsets or sets: the parts.d parts and the routines (the same names, but the
# routines list stays the reference of WW3GPU_ROUTINES).
set(WW3GPU_ENV_PARTS ${WW3GPU_PARTS} ${WW3GPU_ALL_ROUTINES})
list(REMOVE_DUPLICATES WW3GPU_ENV_PARTS)
set(WW3GPU_ENV_COUNTS ${WW3GPU_PART_COUNTS})
foreach(u IN LISTS WW3GPU_COUNT_FLAGS)
  list(APPEND WW3GPU_ENV_COUNTS "WW_KOKKOS_${u}_COUNT")
endforeach()
list(REMOVE_DUPLICATES WW3GPU_ENV_COUNTS)
set(WW3GPU_FLAGS "")
set(_words "${WW3GPU_SWITCH_WORDS}")
if(NOT WW3GPU_KOKKOS STREQUAL "")
  ww3gpu_kokkos_preset(WW3GPU_FLAGS "${WW3GPU_KOKKOS}" "${WW3GPU_PARTS}")
  if(WW3GPU_FLAGS MATCHES "^ERROR:(.*)")
    message(FATAL_ERROR "WW3GPU_KOKKOS: ${CMAKE_MATCH_1}")
  endif()
  if(WW3GPU_FLAGS STREQUAL "")
    message(FATAL_ERROR "WW3GPU_KOKKOS=none turns nothing on: the pair would compare the Fortran with itself")
  endif()
  set(WW3GPU_PHYSICS ANY)
  if(_words STREQUAL "" AND WW3GPU_KOKKOS STREQUAL "resident")
    set(_words "ST4;NL1;PR3;UQ;SHRD;!PDLIB;!MGP;!MGG")
  endif()
endif()
set(WW3GPU_WORDS "${_words}")
if(NOT WW3GPU_MPI_RANKS MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "WW3GPU_MPI_RANKS is a positive integer, not '${WW3GPU_MPI_RANKS}'")
endif()
if(NOT WW3GPU_OMP_THREADS MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "WW3GPU_OMP_THREADS is a positive integer, not '${WW3GPU_OMP_THREADS}'")
endif()
set(_base "${WW3GPU_WW3}/regtests/bin/matrix.base")
if(NOT EXISTS "${_base}")
  message(FATAL_ERROR "${WW3GPU_WW3} has no regtests/bin/matrix.base: git submodule update --init WW3")
endif()
if(NOT EXISTS "${WW3GPU_WW3}/model/src/w3kokkosmd.F90")
  message(FATAL_ERROR "${WW3GPU_WW3} is not the ww3-gpu/kokkos-st4 branch of h0ffmann/WW3 (no model/src/w3kokkosmd.F90)")
endif()
ww3gpu_ftp_version(_ftpver _ftpurl "${WW3GPU_WW3}/model/bin/ww3_from_ftp.sh")
set(WW3GPU_DATA "" CACHE PATH "WW3's ftp data, <WW3GPU_DATA>/data_regtests (pratico's ww3-regtest-data)")
# Two ifs: an if() expands ${CMAKE_MATCH_1} before its own MATCHES has run.
if(WW3GPU_DATA MATCHES "-ww3-regtest-data-([0-9.]+)$")
  set(_dataver "v${CMAKE_MATCH_1}")
  if(NOT _dataver STREQUAL _ftpver)
    message(FATAL_ERROR "${WW3GPU_DATA} is data ${_dataver}, and WW3's ww3_from_ftp.sh wants ${_ftpver}: "
                        "bump ww3RegtestData in picasso/labs/pratico/flake.nix with the WW3 pin")
  endif()
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_base}")

# The number of entries the WW3 pin gives at WW3@c9a6a0c6, where KOKKOS stopped requiring NL1
# and refuses ST4 with FLX5 (ww3_tp2.15 -s ST4 -i input_rho, dropped from every selection): selection:count fails when a pin bump changes it, so the
# change is read before its table is.
set(_expected_NL1_serial 99)
set(_expected_NL1_omp 50)
set(_expected_NL1_mpi 161)
set(_expected_NL1_all 310)
set(_expected_ST4_serial 46)
set(_expected_ST4_omp 10)
set(_expected_ST4_mpi 80)
set(_expected_ST4_all 136)
set(_expected_NL1_ST4_serial 105)
set(_expected_NL1_ST4_omp 50)
set(_expected_NL1_ST4_mpi 164)
set(_expected_NL1_ST4_all 318)
# The resident preset's default words at WW3@80644ba9 (scripts/ww3-gpu.sh --kokkos resident), after the
# ST4 with FLX5 exclusion (ww3_tp2.15 -i input_rho). No OpenMP or MPI entry has them: the PR3 UQ
# OpenMP entries are moving-grid (MGP MGG) and W3XYP3's Kokkos path needs SHRD.
set(_expected_ANY_ST4_NL1_PR3_UQ_SHRD_NOTPDLIB_NOTMGP_NOTMGG_serial 7)
set(_expected_ANY_ST4_NL1_PR3_UQ_SHRD_NOTPDLIB_NOTMGP_NOTMGG_omp 0)
set(_expected_ANY_ST4_NL1_PR3_UQ_SHRD_NOTPDLIB_NOTMGP_NOTMGG_mpi 0)
set(_expected_ANY_ST4_NL1_PR3_UQ_SHRD_NOTPDLIB_NOTMGP_NOTMGG_all 7)
list(JOIN WW3GPU_PHYSICS "_" _phys)
if(NOT WW3GPU_WORDS STREQUAL "")
  list(JOIN WW3GPU_WORDS "_" _w)
  string(REPLACE "!" "NOT" _w "${_w}")
  string(APPEND _phys "_${_w}")
endif()
set(WW3GPU_EXPECTED "${_expected_${_phys}_${WW3GPU_SELECTION}}")

ww3gpu_select_entries(E "${_base}" "${WW3GPU_WW3}/regtests" "${WW3GPU_SELECTION}" "${WW3GPU_PHYSICS}" "${WW3GPU_WORDS}")
list(LENGTH E_TESTS _count)
if(WW3GPU_KOKKOS STREQUAL "")
  set(_what "Kokkos ${WW3GPU_ROUTINES}")
else()
  set(_what "Kokkos --kokkos ${WW3GPU_KOKKOS} (${WW3GPU_FLAGS})")
endif()
message(STATUS "ww3-gpu-matrix: ${_count} ${WW3GPU_SELECTION} entries of ${_base} with ${WW3GPU_PHYSICS} ${WW3GPU_WORDS}; ${_what} on ${WW3GPU_BACKEND}")

set(_kbuild "${WW3GPU_ROOT}/kokkos/build/${WW3GPU_BACKEND}-release")
set(_config "${CMAKE_CURRENT_BINARY_DIR}/ww3gpu_config.cmake")
file(WRITE "${_config}" "# written by ${CMAKE_CURRENT_LIST_FILE}; reconfigure to change\n")
foreach(kv
    "WW3GPU_ROOT;${WW3GPU_ROOT}" "WW3GPU_WW3;${WW3GPU_WW3}" "WW3GPU_DATA;${WW3GPU_DATA}"
    "WW3GPU_SELECTION;${WW3GPU_SELECTION}" "WW3GPU_BACKEND;${WW3GPU_BACKEND}" "WW3GPU_ROUTINES;${WW3GPU_ROUTINES}" "WW3GPU_PHYSICS;${WW3GPU_PHYSICS}" "WW3GPU_KOKKOS;${WW3GPU_KOKKOS}" "WW3GPU_FLAGS;${WW3GPU_FLAGS}" "WW3GPU_WORDS;${WW3GPU_WORDS}" "WW3GPU_LAB_SWITCH;${WW3GPU_LAB_SWITCH}"
    "WW3GPU_ENV_PARTS;${WW3GPU_ENV_PARTS}" "WW3GPU_ENV_COUNTS;${WW3GPU_ENV_COUNTS}" "WW3GPU_STRICT;${WW3GPU_STRICT}" "WW3GPU_OMP_NESTED;${WW3GPU_OMP_NESTED}"
    "WW3GPU_OMP_THREADS;${WW3GPU_OMP_THREADS}" "WW3GPU_MPI_RANKS;${WW3GPU_MPI_RANKS}" "WW3GPU_EXPECTED;${WW3GPU_EXPECTED}"
    "WW3GPU_OUT;${CMAKE_CURRENT_BINARY_DIR}" "WW3GPU_STAGE;${CMAKE_CURRENT_BINARY_DIR}/regtests"
    "WW3GPU_KBUILD;${_kbuild}" "WW3GPU_KPREFIX;${_kbuild}/install"
    "WW3GPU_NCCMP;${_kbuild}/tools/nccmp-tol/nccmp-tol"
    "WW3GPU_TOLS;${WW3GPU_ROOT}/kokkos/tools/nccmp-tol/tolerances.txt")
  list(POP_FRONT kv k)
  set(v "${kv}")
  file(APPEND "${_config}" "set(${k} [==[${v}]==])\n")
endforeach()

set(_cm "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
enable_testing()

add_test(NAME selection:count
  COMMAND "${CMAKE_COMMAND}" "-DCONFIG=${_config}" -P "${_cm}/count.cmake")
set_tests_properties(selection:count PROPERTIES LABELS "${WW3GPU_SELECTION}")

add_test(NAME setup:kokkos
  COMMAND "${CMAKE_COMMAND}" "-DCONFIG=${_config}" -P "${_cm}/setup_kokkos.cmake")
add_test(NAME setup:stage
  COMMAND "${CMAKE_COMMAND}" "-DCONFIG=${_config}" -P "${_cm}/stage.cmake")
set_tests_properties(setup:kokkos setup:stage PROPERTIES FIXTURES_SETUP ww3-gpu LABELS "${WW3GPU_SELECTION}")
add_test(NAME record:summary
  COMMAND "${CMAKE_COMMAND}" "-DCONFIG=${_config}" -P "${_cm}/summary.cmake")
set_tests_properties(record:summary PROPERTIES FIXTURES_CLEANUP ww3-gpu LABELS "${WW3GPU_SELECTION}")

set(_n 0)
foreach(test IN LISTS E_TESTS)
  list(GET E_OPTS ${_n} opts)
  math(EXPR _n "${_n} + 1")
  ww3gpu_words(o "${opts}")
  ww3gpu_opt_of(sw -s ${o})
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
  set_tests_properties("${name}" PROPERTIES FIXTURES_REQUIRED ww3-gpu LABELS "entry;${WW3GPU_SELECTION}")
endforeach()
