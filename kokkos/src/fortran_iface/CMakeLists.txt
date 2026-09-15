# kokkos/src/fortran_iface/CMakeLists.txt
# The boundary between WAVEWATCH III and the Kokkos kernels: the C ABI
# (ww_kokkos_c.hpp + snl1_shim.cpp) and the Fortran module that binds it
# (w3kokkosmd.F90).
#
# The C++ half goes into ww_kokkos itself, so it is always built and always
# testable -- a GoogleTest binary calls the same entry points WW3 does, with no
# Fortran compiler in the loop. Only the Fortran module needs WW_ENABLE_FORTRAN.
#
# SPDX-License-Identifier: MIT
target_sources(ww_kokkos PRIVATE
    ww_kokkos_c.hpp
    snl1_shim.cpp)

if(WW_ENABLE_FORTRAN)
  # A separate target, not sources of ww_kokkos: the module is meant to be copied
  # into WW3/model/src, where it is compiled by WW3's own build, and keeping it
  # out of the C++ library is what proves it has no other dependency.
  add_library(ww_kokkos_f STATIC w3kokkosmd.F90)
  set_target_properties(ww_kokkos_f PROPERTIES
      Fortran_MODULE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/mod)
  target_include_directories(ww_kokkos_f PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/mod)
  target_link_libraries(ww_kokkos_f PUBLIC ww_kokkos)
  ww_apply_warnings(ww_kokkos_f)
endif()
