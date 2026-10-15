# kokkos/tests/CMakeLists.txt
# GoogleTest suites of the lessons and the tools, not of the port: the port has
# no tests of its own (the owner, 2026-10-14). The DIA is validated by WW3's own
# regtests, replayed by scripts/hoff.sh --hoff --cpu pair to pair with --ww3.
#
# SPDX-License-Identifier: MIT

# Build <name>.cpp into test executable <name> and register it with CTest.
function(ww_add_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE ww_kokkos GTest::gtest_main)
  # WW_FIXTURE_DIR is where the committed binary fixtures live in the *source*
  # tree: they are inputs under version control, never build artefacts.
  target_compile_definitions(${name} PRIVATE
      WW_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
  ww_apply_warnings(${name})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES ENVIRONMENT "${WW_TEST_ENVIRONMENT}")
endfunction()

ww_add_test(L1_test_intro)

# Not a test: the timing harness behind the ms/call numbers in kokkos/PORT_STATUS.md.
# It runs the same shim entry point WW3 would, so what it measures is the phase-1
# cost a caller actually pays -- copy-in, kernel, copy-out -- not just the kernel.
add_executable(ww_bench_snl1 bench_snl1.cpp)
target_link_libraries(ww_bench_snl1 PRIVATE ww_kokkos)
target_compile_definitions(ww_bench_snl1 PRIVATE
    WW_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
ww_apply_warnings(ww_bench_snl1)

ww_add_test(L1_test_nccmp_tol)
target_link_libraries(L1_test_nccmp_tol PRIVATE nccmp_tol_lib)
ww_add_test(L1_test_bench_case)
target_link_libraries(L1_test_bench_case PRIVATE bench_case_lib)
ww_add_test(L1_test_fetch_analyse)
target_link_libraries(L1_test_fetch_analyse PRIVATE fetch_analyse_lib)
