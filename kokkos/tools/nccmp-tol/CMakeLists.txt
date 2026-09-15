# kokkos/tools/nccmp-tol/CMakeLists.txt
# The per-field NetCDF comparator: a static library (so the L1 test can drive it
# in-process) and the CLI on top of it. Both link netcdf-c through the pkg-config
# target the top level found.
#
# SPDX-License-Identifier: MIT
add_library(nccmp_tol_lib STATIC
    tolerances.hpp
    tolerances.cpp
    compare.hpp
    compare.cpp)
# Headers are included as "nccmp-tol/<name>.hpp", relative to tools/.
target_include_directories(nccmp_tol_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(nccmp_tol_lib PUBLIC PkgConfig::NETCDF)
ww_apply_warnings(nccmp_tol_lib)

add_executable(nccmp-tol nccmp_tol.cpp)
target_link_libraries(nccmp-tol PRIVATE nccmp_tol_lib)
# Without a third argument the CLI reads the committed default tolerance file
# straight from the source tree, the way the tests read their fixtures.
target_compile_definitions(nccmp-tol PRIVATE
    NCCMP_DEFAULT_TOLERANCES="${CMAKE_CURRENT_SOURCE_DIR}/tolerances.txt")
ww_apply_warnings(nccmp-tol)
