# kokkos/CMakeLists.txt
# The C++/Kokkos half of the WAVEWATCH III lab: the portable kernel library
# (ww_kokkos), the lesson-11 intro programs, the GoogleTest suites and the
# NetCDF/bench tools. One tree, one backend per configure preset.
#
# Everything is built inside `nix develop nix-config/labs/pratico#ww3` (Serial +
# OpenMP) or `#cuda` (CUDA/ADA89); those shells are what provide Kokkos, GoogleTest
# and NetCDF to find_package/pkg-config.
#
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.25)
project(ww3_gpu_kokkos VERSION 0.1.0 LANGUAGES C CXX Fortran)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(WW_ENABLE_FORTRAN "Build the Fortran interface module, reference and drivers" ON)
option(WW_DETERMINISTIC "Force serial reductions for bit-reproducible validation builds" OFF)
set(WW_WW3_BUILD_DIR "" CACHE PATH
    "A configured WW3 build directory (libww3.a + mod/); enables the ww3_lib cross-check target")

include(cmake/CompilerWarnings.cmake)

# Kokkos' package config runs find_dependency(OpenMP), which needs an enabled
# language -- hence LANGUAGES above, and hence no cmake -P probing of this tree.
find_package(Kokkos REQUIRED)
find_package(GTest REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(NETCDF REQUIRED IMPORTED_TARGET netcdf)
if(WW_ENABLE_FORTRAN)
  pkg_check_modules(NETCDFF REQUIRED IMPORTED_TARGET netcdf-fortran)
endif()

# enable_testing() must run before any directory that calls add_test(), so it
# comes before intro/ and tests/ rather than after them.
enable_testing()

# Environment every CTest case runs with. Kokkos' OpenMP backend prints a warning
# unless OMP_PROC_BIND is set explicitly, and a fixed, small thread count keeps the
# suite from fighting CI for cores and keeps its output reproducible.
set(WW_TEST_ENVIRONMENT "OMP_PROC_BIND=false;OMP_NUM_THREADS=2")

# The portable kernel library: the ported kernels plus the bind(C) shim that
# WAVEWATCH III reaches them through. It hands its include path, Kokkos and the
# WW_DETERMINISTIC contract to everything that links it.
add_library(ww_kokkos STATIC)
# With only headers in target_sources(), CMake cannot infer a link language yet.
set_target_properties(ww_kokkos PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(ww_kokkos PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ww_kokkos PUBLIC Kokkos::kokkos)
ww_apply_warnings(ww_kokkos)
if(WW_DETERMINISTIC)
  target_compile_definitions(ww_kokkos PUBLIC WW_DETERMINISTIC=1)
endif()

add_subdirectory(src/ww_kokkos)
# Not guarded by WW_ENABLE_FORTRAN: the C ABI is C++ and belongs in ww_kokkos
# either way. The directory guards the Fortran module itself.
add_subdirectory(src/fortran_iface)
add_subdirectory(intro)
add_subdirectory(tools/nccmp-tol)
add_subdirectory(tools/bench_case)
add_subdirectory(tools/fetch_analyse)
add_subdirectory(tests)
