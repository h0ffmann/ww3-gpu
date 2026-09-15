// kokkos/tests/L1_test_intro.cpp
// L1 unit tests for the fixtures and the Kokkos idioms the intro programs teach.
//
// Every assertion here is against something computed independently of the code
// under test -- a closed-form integral, an exact normalisation, or a hand-written
// index expression. That is the standard the kernel tests in Task 3 have to meet
// too, so the habit starts here.
//
// SPDX-License-Identifier: MIT

#include <Kokkos_Core.hpp>
#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "kokkos_env.hpp"
#include "ww_kokkos/real.hpp"
#include "ww_kokkos/spectrum_fixtures.hpp"

namespace {

constexpr ww::Real kU10 = static_cast<ww::Real>(10);      // m/s
constexpr ww::Real kFetch = static_cast<ww::Real>(1.0e5);  // m

// Kernels live in free functions, not in the TEST bodies. nvcc refuses an extended
// __host__ __device__ lambda inside a private member function, and a TEST body is
// exactly that -- GoogleTest expands it to a private TestBody() override. Hoisting
// the kernel out is the portable fix and keeps the assertions readable.

/// Integrate the gamma = 1 JONSWAP spectrum over the discrete (theta, sigma) grid.
double jonswap_m0_discrete(const ww::SpectralGrid& g) {
  const ww::Real gamma = static_cast<ww::Real>(1);
  double m0 = 0.0;
  Kokkos::parallel_reduce(
      "L1.intro.m0", Kokkos::MDRangePolicy<Kokkos::Rank<2>>({0, 0}, {g.nth, g.nk}),
      KOKKOS_LAMBDA(const int ith, const int ik, double& acc) {
        const ww::Real e = ww::jonswap(g.sigma(ik), kU10, kFetch, gamma) *
                           ww::cos2_spread(g.theta(ith), static_cast<ww::Real>(0));
        acc += static_cast<double>(e) * static_cast<double>(g.dtheta()) *
               static_cast<double>(g.dsigma(ik));
      },
      m0);
  return m0;
}

#if defined(KOKKOS_ENABLE_SERIAL)
/// Fill 0..n-1 and sum it back, both on an explicitly named Kokkos::Serial policy.
double serial_range_policy_sum(int n) {
  Kokkos::View<double*, Kokkos::Serial::memory_space> v("L1.intro.serial", n);
  Kokkos::parallel_for(
      "L1.intro.serial.fill", Kokkos::RangePolicy<Kokkos::Serial>(0, n),
      KOKKOS_LAMBDA(const int i) { v(i) = static_cast<double>(i); });
  double sum = 0.0;
  Kokkos::parallel_reduce(
      "L1.intro.serial.sum", Kokkos::RangePolicy<Kokkos::Serial>(0, n),
      KOKKOS_LAMBDA(const int i, double& acc) { acc += v(i); }, sum);
  return sum;
}
#endif

}  // namespace

// The discrete (theta, sigma) sum must reproduce the closed-form m0 of the
// gamma = 1 JONSWAP form. What is left over is the geometric grid's truncation of
// the sigma^-5 tail, which is a fraction of a percent on WW3's default grid.
TEST(IntroFixtures, JonswapIntegralMatchesClosedFormHs) {
  const double hs = 4.0 * std::sqrt(jonswap_m0_discrete(ww::default_grid()));
  const double m0 = static_cast<double>(ww::jonswap_m0(kU10, kFetch));
  EXPECT_NEAR(hs, 4.0 * std::sqrt(m0), 0.02 * hs);
}

// cos2_spread is a probability density over direction: whatever the mean, summing
// it over a full circle must give 1, or every integrated wave height is biased.
TEST(IntroFixtures, Cos2SpreadIntegratesToOne) {
  const ww::SpectralGrid g = ww::default_grid();
  for (const ww::Real theta_mean :
       {static_cast<ww::Real>(0), static_cast<ww::Real>(0.7), static_cast<ww::Real>(-2.3)}) {
    double total = 0.0;
    for (int ith = 0; ith < g.nth; ++ith) {
      total += static_cast<double>(ww::cos2_spread(g.theta(ith), theta_mean)) *
               static_cast<double>(g.dtheta());
    }
    EXPECT_NEAR(total, 1.0, 1.0e-3) << "theta_mean = " << theta_mean;
  }
}

// The Fortran boundary contract: a LayoutLeft unmanaged View over a buffer that
// Fortran declared A(NTH,NK) must place element (ith, ik) at ith + ik*nth.
TEST(IntroFixtures, UnmanagedLayoutLeftMatchesFortranOrder) {
  constexpr int nth = 24;
  constexpr int nk = 32;
  std::vector<ww::Real> buffer(static_cast<size_t>(nth) * static_cast<size_t>(nk),
                               static_cast<ww::Real>(0));

  Kokkos::View<ww::Real**, Kokkos::LayoutLeft, Kokkos::HostSpace,
               Kokkos::MemoryTraits<Kokkos::Unmanaged>>
      view(buffer.data(), nth, nk);
  ASSERT_EQ(view.stride(0), 1u);
  ASSERT_EQ(view.stride(1), static_cast<size_t>(nth));

  for (int ik = 0; ik < nk; ++ik) {
    for (int ith = 0; ith < nth; ++ith) {
      view(ith, ik) = static_cast<ww::Real>(ith + ik * nth);
    }
  }
  for (size_t i = 0; i < buffer.size(); ++i) {
    ASSERT_EQ(buffer[i], static_cast<ww::Real>(i)) << "flat index " << i;
  }
}

// The lab shell's default execution space is OpenMP, so nothing above ever touches
// the Serial backend. Name it explicitly once: a RangePolicy<Kokkos::Serial> over
// Serial-accessible memory is how the deterministic build pins summation order.
#if defined(KOKKOS_ENABLE_SERIAL)
TEST(IntroFixtures, SerialBackendRunsARangePolicy) {
  constexpr int n = 1024;
  EXPECT_EQ(serial_range_policy_sum(n),
            0.5 * static_cast<double>(n) * static_cast<double>(n - 1));
}
#endif
