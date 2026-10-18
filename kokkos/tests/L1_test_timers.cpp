// kokkos/tests/L1_test_timers.cpp
// A test of a tool, ww::timers::Region (kokkos/src/fortran_iface/timers.hpp), not of the port.
//
// While an exception unwinds a shim, Region's destructor must not fence: on CUDA a second fence
// after a device error throws again from a noexcept destructor and terminates the run before the
// shim hands its status to WW3 (commit d56b8d6b). On a host backend a fence never throws, so the
// model's output cannot tell the guard from its absence; this test counts the fences Region asks
// Kokkos for, through Kokkos' tools interface, with and without an exception in flight.
//
// SPDX-License-Identifier: MIT

#include <Kokkos_Core.hpp>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "fortran_iface/timers.hpp"
#include "kokkos_env.hpp"

namespace {

int g_stop_fences = 0;

void count_fence(const char* name, const uint32_t /*device*/, uint64_t* handle) {
  *handle = 0;
  if (std::strcmp(name, "ww_kokkos.timers.stop") == 0) ++g_stop_fences;
}

// WW_KOKKOS_TIMERS is read once, at the first Region; nothing in this binary makes one before.
const bool g_timers_on = [] { return ::setenv("WW_KOKKOS_TIMERS", "1", 1) == 0; }();

TEST(Timers, RegionFencesAtStopOnlyWhenNoExceptionUnwinds) {
  ASSERT_TRUE(g_timers_on);
  ASSERT_TRUE(ww::timers::enabled());
  Kokkos::Tools::Experimental::set_begin_fence_callback(count_fence);

  g_stop_fences = 0;
  { ww::timers::Region r("L1.timers.normal"); }
  EXPECT_EQ(g_stop_fences, 1) << "a Region with fence = true fences when it stops";

  g_stop_fences = 0;
  bool caught = false;
  try {
    ww::timers::Region r("L1.timers.unwind");
    throw std::runtime_error("a shim's error");
  } catch (const std::runtime_error&) {
    caught = true;
  }
  EXPECT_TRUE(caught);
  EXPECT_EQ(g_stop_fences, 0) << "a Region destroyed while an exception unwinds must not fence";

  Kokkos::Tools::Experimental::set_begin_fence_callback(nullptr);
}

}  // namespace
