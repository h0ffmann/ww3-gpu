# kokkos/src/ww_kokkos/CMakeLists.txt
# Sources of the portable kernel library: the DIA port (snl1_*) and the binary
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
    fixture_io.cpp)

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
