# selection:count: the WW3 pin gives the number of entries CMakeLists.txt expects for the
# selection and the physics of WW3GPU_ROUTINES. A pin bump that changes the selection fails here first.
cmake_minimum_required(VERSION 3.21)
include("${CONFIG}")
include("${CMAKE_CURRENT_LIST_DIR}/ww3gpu_common.cmake")
ww3gpu_select_entries(E "${WW3GPU_WW3}/regtests/bin/matrix.base" "${WW3GPU_WW3}/regtests" "${WW3GPU_SELECTION}" "${WW3GPU_PHYSICS}")
list(LENGTH E_TESTS n)
execute_process(COMMAND git -C "${WW3GPU_WW3}" rev-parse --short HEAD OUTPUT_VARIABLE rev OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT n EQUAL WW3GPU_EXPECTED)
  message(FATAL_ERROR "ww3gpu_matrix: WW3@${rev} matrix.base gives ${n} ${WW3GPU_SELECTION} ${WW3GPU_PHYSICS} entries, not ${WW3GPU_EXPECTED}; "
                      "read the change, then update the expected count in regtests/CMakeLists.txt")
endif()
message(STATUS "ww3gpu_matrix: WW3@${rev} matrix.base: ${n} ${WW3GPU_SELECTION} entries")
