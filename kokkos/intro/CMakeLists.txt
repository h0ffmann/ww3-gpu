# kokkos/intro/CMakeLists.txt
# Lesson-11 programs: one Kokkos concept each, each a self-checking main that
# prints a single line and exits non-zero if the concept did not hold. They are
# registered as CTest cases so CI runs the lesson material, not just the library.
#
# SPDX-License-Identifier: MIT

# Every C++-only program; the file stem doubles as the test name suffix.
set(ww_intro_programs
    01_views
    02_parallel_for
    03_reduce_and_scan
    04_layouts_and_mirrors
    05_team_scratch)

foreach(prog IN LISTS ww_intro_programs)
  string(SUBSTRING "${prog}" 0 2 nn)
  add_executable(ww_intro_${nn} ${prog}.cpp)
  target_link_libraries(ww_intro_${nn} PRIVATE ww_kokkos)
  ww_apply_warnings(ww_intro_${nn})
  add_test(NAME intro_${nn} COMMAND ww_intro_${nn})
  set_tests_properties(intro_${nn} PROPERTIES ENVIRONMENT "${WW_TEST_ENVIRONMENT}")
endforeach()

# 06 is the bind(C) lesson: a Fortran main calling the C++ entry points, so it
# only exists when the Fortran half of the lab is enabled.
if(WW_ENABLE_FORTRAN)
  add_executable(ww_intro_06 06_interop_bindc.cpp 06_interop_driver.F90)
  target_link_libraries(ww_intro_06 PRIVATE ww_kokkos)
  ww_apply_warnings(ww_intro_06)
  add_test(NAME intro_06 COMMAND ww_intro_06)
  set_tests_properties(intro_06 PROPERTIES ENVIRONMENT "${WW_TEST_ENVIRONMENT}")
endif()
