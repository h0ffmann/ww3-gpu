// kokkos/tests/kokkos_env.hpp
// The one place the Kokkos runtime is started for a GoogleTest binary.
//
// Kokkos::initialize()/finalize() may each run once per process, so they belong in
// a global ::testing::Environment rather than in a fixture SetUp. Registering the
// environment at namespace scope means every test suite that includes this header
// gets a live runtime without writing its own main().
//
// The thread count is pinned to 2 on the OpenMP backend deliberately: Kokkos 5's
// OpenMP backend is the default execution space in the lab shell, tests must not
// fight CI for cores, and two threads is the smallest count that still exposes a
// missing team barrier or a racy reduction.
//
// SPDX-License-Identifier: MIT
#pragma once

#include <Kokkos_Core.hpp>
#include <gtest/gtest.h>

namespace ww {

class KokkosEnv : public ::testing::Environment {
 public:
  void SetUp() override {
    Kokkos::InitializationSettings settings;
#if defined(KOKKOS_ENABLE_OPENMP)
    settings.set_num_threads(2);
#endif
    Kokkos::initialize(settings);
  }
  void TearDown() override { Kokkos::finalize(); }
};

// GoogleTest takes ownership of the environment; the pointer is only kept so the
// registration happens at static-init time.
inline ::testing::Environment* const kokkos_env =
    ::testing::AddGlobalTestEnvironment(new KokkosEnv);

}  // namespace ww
