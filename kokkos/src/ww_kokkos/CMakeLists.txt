# kokkos/src/ww_kokkos/CMakeLists.txt
# Sources of the portable kernel library: the DIA port (snl1_*), the ST4 port
# (st4_*: W3SDS4, W3SIN4, W3SPR4, CALC_USTAR, INSIN4, TABU_*) and the binary
# fixture reader its L1 tests read the Fortran reference answers with.
#
# SPDX-License-Identifier: MIT
target_sources(ww_kokkos PRIVATE
    real.hpp
    spectrum_fixtures.hpp
    snl1_config.hpp
    snl1_tables.hpp
    snl1_tables.cpp
    snl1_dia.hpp
    snl1_dia.cpp
    fixture_io.hpp
    fixture_io.cpp
    st4_params.hpp
    st4_arrays.hpp
    st4_libmvec.hpp
    st4_calc_ustar.hpp
    st4_spr4.hpp
    st4_sin4.hpp
    st4_sds4.hpp
    st4_tables_build.hpp
    st4_insin4.cpp
    st4_tabu_stress.cpp
    st4_tabu_tauhf.cpp
    st4_tabu_tauhf2.cpp)

# No floating-point contraction in the ported kernels.
#
# GCC defaults to -ffp-contract=fast, so at -O3 -march=x86-64-v3 it fuses
# `AWG1*UE(..) + AWG2*UE(..)` into an FMA: one rounding where WW3's Fortran does
# two. That is a different computation, and it showed up exactly where it must --
# the openmp-release preset drifted from the committed Fortran fixture by 1.1e-5
# relative while serial-debug was bit-identical. The port's contract is the
# Fortran's arithmetic, so contraction is off and the answer is the same on every
# preset. Removing this line is a physics change, not an optimisation.
target_compile_options(ww_kokkos PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-ffp-contract=off>)
# nvcc_wrapper reports itself to CMake as GNU, so the flag above lands on the
# *host* half of the compile only; the device half needs nvcc's own switch.
if(Kokkos_ENABLE_CUDA)
  target_compile_options(ww_kokkos PRIVATE $<$<COMPILE_LANGUAGE:CXX>:--fmad=false>)
endif()

# ST4 calls glibc's libmvec where gfortran -O3 does (st4_libmvec.hpp), so the
# host port computes the Fortran's bits at those five loops.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
  target_link_libraries(ww_kokkos PUBLIC mvec)
endif()
