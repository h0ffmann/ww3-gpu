// kokkos/intro/02_parallel_for.cpp
// Lesson 11.2 -- parallel_for over a 2D iteration space, and parallel_reduce.
//
// Builds a fetch-limited JONSWAP spectrum on a WW3-shaped (theta, sigma) grid with
// an MDRangePolicy<Rank<2>>, then integrates it back to a significant wave height
// with a parallel_reduce and checks that against the closed-form m0. That check is
// the whole point: a kernel that cannot be compared with something you computed
// independently is a kernel you cannot port.
//
// The spectrum is built with gamma = 1 because only then does the frequency
// integral have an exact solution (see spectrum_fixtures.hpp). The residual is
// therefore pure discretisation error -- the geometric band grid truncates the
// sigma^-5 tail -- and stays well inside 2 %.
//
// SPDX-License-Identifier: MIT

#include <Kokkos_Core.hpp>

#include <cmath>
#include <cstdio>

#include "ww_kokkos/real.hpp"
#include "ww_kokkos/spectrum_fixtures.hpp"

namespace {

// Spell the memory space out: HostSpace under the Serial/OpenMP presets,
// CudaSpace under cuda-release. Never take the default for a View you allocate.
using DeviceSpace = Kokkos::DefaultExecutionSpace::memory_space;

}  // namespace

int main(int argc, char* argv[]) {
  Kokkos::ScopeGuard guard(argc, argv);
  double err = 0.0;
  {
    const ww::SpectralGrid g = ww::default_grid();
    const ww::Real u10 = static_cast<ww::Real>(10);      // m/s
    const ww::Real fetch = static_cast<ww::Real>(1.0e5);  // m
    const ww::Real theta_mean = static_cast<ww::Real>(0);
    const ww::Real gamma = static_cast<ww::Real>(1);

    Kokkos::View<ww::Real**, DeviceSpace> e("intro.E", g.nth, g.nk);

    // MDRangePolicy tiles the (ith, ik) rectangle itself. Writing the same thing
    // as a RangePolicy over ith*nk + ik would work, but the tiling is what lets
    // the CUDA backend pick a 2D block shape that keeps ith contiguous.
    Kokkos::parallel_for(
        "intro.02.jonswap",
        Kokkos::MDRangePolicy<Kokkos::Rank<2>>({0, 0}, {g.nth, g.nk}),
        KOKKOS_LAMBDA(const int ith, const int ik) {
          e(ith, ik) = ww::jonswap(g.sigma(ik), u10, fetch, gamma) *
                       ww::cos2_spread(g.theta(ith), theta_mean);
        });

    // The accumulator is double even though the state is float: a sum of 768
    // float terms spanning five decades loses digits that the tolerance below
    // would otherwise have to hide. Reductions are where mixed precision pays.
    double m0 = 0.0;
    Kokkos::parallel_reduce(
        "intro.02.m0",
        Kokkos::MDRangePolicy<Kokkos::Rank<2>>({0, 0}, {g.nth, g.nk}),
        KOKKOS_LAMBDA(const int ith, const int ik, double& acc) {
          acc += static_cast<double>(e(ith, ik)) * static_cast<double>(g.dtheta()) *
                 static_cast<double>(g.dsigma(ik));
        },
        m0);

    const double hs = 4.0 * std::sqrt(m0);
    const double hs_ref = 4.0 * std::sqrt(static_cast<double>(ww::jonswap_m0(u10, fetch)));
    err = std::fabs(hs - hs_ref) / hs_ref;

    std::printf("02_parallel_for: Hs=%.4f m closed-form=%.4f m rel.err=%.3f %%\n", hs,
                hs_ref, 100.0 * err);
  }

  if (!(err < 0.02)) {
    std::fprintf(stderr, "02_parallel_for: FAILED -- Hs is %.3f %% off the closed form\n",
                 100.0 * err);
    return 1;
  }
  return 0;
}
