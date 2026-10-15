# bench_dia: WW3's W3SNL1, the Kokkos port (v2) and ww3-dia-gpu's kernel (v1) on the same CPU
# (bench_dia.F90). It links the libww3.a of a WW3 build made with the KOKKOS and OMPG switches
# (scripts/bench_dia.sh makes one), so the Fortran timed is the one WW3 runs, compiled with WW3's
# own flags, and v2 is reached through the shim WW3 calls.
#
#   cmake -S bench/dia -B <dir> -DWW3_BUILD=<WW3 build tree> -DCMAKE_PREFIX_PATH=<ww_kokkos prefix>
cmake_minimum_required(VERSION 3.21)
project(bench_dia LANGUAGES C CXX Fortran)

set(WW3_BUILD "" CACHE PATH "A WW3 build tree with the KOKKOS and OMPG switches")
if(NOT EXISTS "${WW3_BUILD}/lib/libww3.a" OR NOT IS_DIRECTORY "${WW3_BUILD}/model/src/mod")
  message(FATAL_ERROR "WW3_BUILD='${WW3_BUILD}' has no lib/libww3.a and model/src/mod")
endif()

find_package(OpenMP REQUIRED COMPONENTS Fortran CXX)
find_package(ww_kokkos REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(NETCDFF REQUIRED IMPORTED_TARGET netcdf-fortran)

add_executable(bench_dia bench_dia.F90 v1_cpu.cpp)
target_include_directories(bench_dia PRIVATE "${WW3_BUILD}/model/src/mod")
target_link_libraries(bench_dia PRIVATE "${WW3_BUILD}/lib/libww3.a" ww_kokkos::ww_kokkos
                      OpenMP::OpenMP_Fortran OpenMP::OpenMP_CXX PkgConfig::NETCDFF)
set_target_properties(bench_dia PROPERTIES LINKER_LANGUAGE Fortran)
# v1 as its author would build it for a CPU: -O3 and GCC's default contraction (FMA on), as nvcc's
# default is on the GPU. The Fortran keeps WW3's flags (it is libww3.a); v2 keeps ww_kokkos's.
target_compile_options(bench_dia PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-O3 -march=x86-64-v3>
  $<$<COMPILE_LANGUAGE:Fortran>:-O2>)
