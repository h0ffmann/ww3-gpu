# selection:count: the WW3 pin gives the number of entries CMakeLists.txt expects (100 serial,
# 25 omp at WW3@6d1bcdfe). A pin bump that changes the selection fails here first.
cmake_minimum_required(VERSION 3.21)
include("${CONFIG}")
include("${CMAKE_CURRENT_LIST_DIR}/hoff_common.cmake")
hoff_select_entries(E "${HOFF_WW3}/regtests/bin/matrix.base" "${HOFF_WW3}/regtests" "${HOFF_SELECTION}")
list(LENGTH E_TESTS n)
execute_process(COMMAND git -C "${HOFF_WW3}" rev-parse --short HEAD OUTPUT_VARIABLE rev OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT n EQUAL HOFF_EXPECTED)
  message(FATAL_ERROR "hoff_matrix: WW3@${rev} matrix.base gives ${n} ${HOFF_SELECTION} entries, not ${HOFF_EXPECTED}; "
                      "read the change, then update the expected count in regtests/CMakeLists.txt")
endif()
message(STATUS "hoff_matrix: WW3@${rev} matrix.base: ${n} ${HOFF_SELECTION} entries")
