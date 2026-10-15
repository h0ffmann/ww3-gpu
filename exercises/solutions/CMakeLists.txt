# exercises/solutions/CMakeLists.txt
# A tiny standalone project for the compiled solutions: ex11 (gfortran, no
# dependencies) and ex12 (Kokkos, header-only use of kokkos/src). It is not part
# of the kokkos/ tree on purpose -- a student's own port will live outside it
# too, and this is the minimum CMake that builds against the lab's headers.
#
#   nix develop ./picasso/labs/pratico#ww3            # from the repo root
#   cmake -S exercises/solutions -B exercises/solutions/build -G Ninja
#   cmake --build exercises/solutions/build
#   ctest --test-dir exercises/solutions/build --output-on-failure
#
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.25)
project(ww3_gpu_exercises LANGUAGES CXX Fortran)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

enable_testing()

# ex11: the legacy routine, its modern twin and the parity test.
add_executable(ex11_refactor_test ex11_refactor.F90 ex11_refactor_test.F90)
target_compile_options(ex11_refactor_test PRIVATE -std=f2018 -Wall -Wextra -fimplicit-none)
add_test(NAME ex11_refactor_test COMMAND ex11_refactor_test)

# ex12: Hs per sea point with a team parallel_reduce. The headers it needs
# (real.hpp, spectrum_fixtures.hpp) are header-only, so an include path into
# kokkos/src is all it takes -- no link against the lab's ww_kokkos library.
find_package(Kokkos REQUIRED)
add_executable(ex12_reduce ex12_reduce.cpp)
target_include_directories(ex12_reduce PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../kokkos/src)
target_link_libraries(ex12_reduce PRIVATE Kokkos::kokkos)
target_compile_options(ex12_reduce PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wshadow;-Wconversion>)
add_test(NAME ex12_reduce COMMAND ex12_reduce)
set_tests_properties(ex12_reduce PROPERTIES ENVIRONMENT "OMP_PROC_BIND=false;OMP_NUM_THREADS=2")
