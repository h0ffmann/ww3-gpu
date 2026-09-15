# kokkos/tests/fixtures/CMakeLists.txt
# The Fortran side of the L1 parity fixtures: the standalone W3SNL1/INSNL1
# reference, the shared sea state, the generator that streams the reference's
# answers into the committed snl1_nk25_nth24.bin, and the shim round-trip driver.
# Nothing here is built into ww_kokkos. The generator only ever runs when a human
# regenerates the fixture (`just snl1-fixtures`); shim_driver is a CTest case.
#
# SPDX-License-Identifier: MIT

set(WW_SNL1_FIXTURE ${CMAKE_CURRENT_SOURCE_DIR}/snl1_nk25_nth24.bin)

# The reference and the sea state are shared by two programs, so they are a
# library: two executables compiling the same .F90 would race to write the same
# .mod file into the same directory, and Ninja refuses that outright.
add_library(snl1_ref STATIC snl1_ref.F90 snl1_sea_state.F90)
set_target_properties(snl1_ref PROPERTIES
    Fortran_MODULE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/mod)
target_include_directories(snl1_ref PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/mod)
ww_apply_warnings(snl1_ref)

add_executable(gen_snl1_fixture gen_snl1_fixture.F90)
target_link_libraries(gen_snl1_fixture PRIVATE snl1_ref)
ww_apply_warnings(gen_snl1_fixture)

# The round trip of Task 4: a Fortran program that reaches the Kokkos kernel only
# through W3KOKKOSMD and the bind(C) shim, and checks the answer against the same
# W3SNL1_REF the fixture is made of. It is a test, not a generator, so it is part
# of ALL and of CTest.
add_executable(shim_driver shim_driver.F90)
target_link_libraries(shim_driver PRIVATE snl1_ref ww_kokkos_f)
ww_apply_warnings(shim_driver)
add_test(NAME shim_roundtrip COMMAND shim_driver)
set_tests_properties(shim_roundtrip PROPERTIES ENVIRONMENT "${WW_TEST_ENVIRONMENT}")

# Not part of ALL and not a test: regenerating rewrites a committed file, which
# must be a deliberate act with a reviewable diff.
add_custom_target(snl1-fixtures
    COMMAND $<TARGET_FILE:gen_snl1_fixture> ${WW_SNL1_FIXTURE}
    DEPENDS gen_snl1_fixture
    COMMENT "Regenerating ${WW_SNL1_FIXTURE} from the Fortran reference")

# Optional cross-check: the same record written by the *real* W3SNL1/INSNL1 out
# of a configured WW3 build, so the standalone reference can be proven to be a
# faithful copy rather than assumed to be one. Needs a WW3 built with a switch
# that includes NL1 -- with NL0 the model never compiles w3snl1md at all. See the
# note in kokkos/README.md.
if(WW_WW3_BUILD_DIR)
  # WW3's CMake and its older build scripts disagree about where the archive and
  # the .mod files land, so look in both places rather than hard-coding one.
  find_library(WW_WW3_LIBRARY NAMES ww3
      HINTS ${WW_WW3_BUILD_DIR} PATH_SUFFIXES lib src model/src NO_DEFAULT_PATH)
  find_path(WW_WW3_MODULE_DIR NAMES w3snl1md.mod
      HINTS ${WW_WW3_BUILD_DIR} PATH_SUFFIXES model/src/mod src/mod mod NO_DEFAULT_PATH)

  if(NOT WW_WW3_LIBRARY OR NOT WW_WW3_MODULE_DIR)
    message(WARNING
        "WW_WW3_BUILD_DIR=${WW_WW3_BUILD_DIR} has no libww3.a with a w3snl1md.mod "
        "next to it; skipping gen_snl1_ww3lib. A WW3 built with NL1 is required.")
  else()
    add_executable(gen_snl1_ww3lib gen_snl1_ww3lib.F90)
    ww_apply_warnings(gen_snl1_ww3lib)
    target_include_directories(gen_snl1_ww3lib PRIVATE ${WW_WW3_MODULE_DIR})
    target_link_libraries(gen_snl1_ww3lib PRIVATE ${WW_WW3_LIBRARY})
    if(NETCDFF_FOUND)
      target_link_libraries(gen_snl1_ww3lib PRIVATE PkgConfig::NETCDFF)
    endif()
    find_package(MPI COMPONENTS Fortran)
    if(MPI_Fortran_FOUND)
      target_link_libraries(gen_snl1_ww3lib PRIVATE MPI::MPI_Fortran)
    endif()
  endif()
endif()
