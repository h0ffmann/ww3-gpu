# kokkos/src/fortran_iface/CMakeLists.txt
# The boundary between WAVEWATCH III and the Kokkos kernels: the C ABI
# (ww_kokkos_c.hpp + snl1_shim.cpp) here, and the Fortran module that binds it
# (w3kokkosmd.F90), which lives in WW3 itself: the h0ffmann/WW3 branch
# ww3-gpu/kokkos-st4, at the commit the WW3/ submodule pins. This repository keeps
# no WW3 Fortran; this build compiles the module from WW3/model/src.
#
# The C++ half goes into ww_kokkos itself, so it is always built; the benchmark
# (tests/bench_snl1.cpp) calls the same entry points WW3 does, with no Fortran
# compiler in the loop. Only the Fortran module needs WW_ENABLE_FORTRAN. The shim
# has no tests of its own (the owner, 2026-10-14): WW3's regtests, through
# scripts/ww3-gpu.sh --ww3-gpu --cpu, exercise it.
#
# SPDX-License-Identifier: MIT
target_sources(ww_kokkos PRIVATE
    ww_kokkos_c.hpp
    shim_state.hpp
    snl1_shim.cpp
    st4_shim.cpp
    srce_shim.cpp)

set(WW_W3KOKKOSMD "${CMAKE_CURRENT_SOURCE_DIR}/../../../WW3/model/src/w3kokkosmd.F90"
    CACHE FILEPATH "WW3's w3kokkosmd.F90 (the WW3/ submodule, branch ww3-gpu/kokkos-st4)")

if(WW_ENABLE_FORTRAN)
  # Loud, not skipped: a missing module means the WW3/ pin and this C ABI can drift
  # apart unseen.
  if(NOT EXISTS "${WW_W3KOKKOSMD}")
    message(FATAL_ERROR "No ${WW_W3KOKKOSMD}: run `git submodule update --init WW3` "
        "(the ww3-gpu/kokkos-st4 branch of h0ffmann/WW3), or set WW_W3KOKKOSMD, "
        "or configure with -DWW_ENABLE_FORTRAN=OFF")
  endif()
  # A separate target, not sources of ww_kokkos: WW3 compiles the module in its own
  # build, and keeping it out of the C++ library is what proves it has no other
  # dependency.
  add_library(ww_kokkos_f STATIC "${WW_W3KOKKOSMD}")
  set_target_properties(ww_kokkos_f PROPERTIES
      Fortran_MODULE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/mod)
  target_include_directories(ww_kokkos_f PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/mod)
  target_link_libraries(ww_kokkos_f PUBLIC ww_kokkos)
  ww_apply_warnings(ww_kokkos_f)
endif()
