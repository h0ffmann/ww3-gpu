// kokkos/intro/05_team_scratch.cpp
// Lesson 11.5 -- TeamPolicy and team scratch memory.
//
// This is the shape the DIA kernel will use. W3SNL1 works on an *extended*
// spectrum: the resolved bands plus a parametric sigma^-5 tail, so that a
// quadruplet whose interaction frequency falls above the grid still has something
// to read. That scratch array is per sea point, it is written and re-read many
// times, and it must never be a global allocation inside a time loop.
//
// A team is a group of threads that share scratch level 0 -- shared memory on a
// GPU, a slice of a thread-local arena on a CPU. One team per sea point, one
// extended spectrum in its scratch, a team_barrier between filling it and reading
// it. That last part is not optional: without the barrier a thread may read a slot
// its team mate has not written yet.
//
// SPDX-License-Identifier: MIT

#include <Kokkos_Core.hpp>

#include <cmath>
#include <cstdio>

#include "ww_kokkos/real.hpp"
#include "ww_kokkos/spectrum_fixtures.hpp"

namespace {

using ExecSpace = Kokkos::DefaultExecutionSpace;
// Spell the memory space out: HostSpace under the Serial/OpenMP presets,
// CudaSpace under cuda-release. Never take the default for a View you allocate.
using DeviceSpace = ExecSpace::memory_space;
using TeamMember = Kokkos::TeamPolicy<ExecSpace>::member_type;
/// Unmanaged view onto scratch: scratch memory is a bump allocator, so the view
/// never owns and never frees.
using ScratchSpectrum =
    Kokkos::View<ww::Real**, Kokkos::LayoutRight, ExecSpace::scratch_memory_space,
                 Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

}  // namespace

int main(int argc, char* argv[]) {
  Kokkos::ScopeGuard guard(argc, argv);
  double err = 0.0;
  {
    const ww::SpectralGrid g = ww::default_grid();
    constexpr int npts = 32;     // sea points -> teams
    constexpr int ntail = 2;     // extra bands carrying the parametric tail
    const int nkx = g.nk + ntail;
    const ww::Real u10 = static_cast<ww::Real>(10);
    const ww::Real fetch = static_cast<ww::Real>(1.0e5);

    Kokkos::View<ww::Real*, DeviceSpace> m0("intro.m0", npts);

    const size_t scratch_bytes = ScratchSpectrum::shmem_size(g.nth, nkx);
    Kokkos::TeamPolicy<ExecSpace> policy(npts, Kokkos::AUTO);
    policy.set_scratch_size(0, Kokkos::PerTeam(scratch_bytes));

    Kokkos::parallel_for(
        "intro.05.team_scratch", policy, KOKKOS_LAMBDA(const TeamMember& team) {
          const int ip = team.league_rank();
          ScratchSpectrum ue(team.team_scratch(0), g.nth, nkx);

          // Resolved bands hold the spectrum; bands at and above nk hold the
          // parametric tail E(sigma) = E(sigma_last) * (sigma_last/sigma)^5.
          Kokkos::parallel_for(Kokkos::TeamThreadRange(team, nkx), [&](const int ik) {
            const int last = g.nk - 1;
            const ww::Real one = static_cast<ww::Real>(1);
            const ww::Real tail =
                (ik <= last) ? one
                             : Kokkos::pow(g.sigma(last) / g.sigma(ik), static_cast<ww::Real>(5));
            const ww::Real band =
                ww::jonswap(g.sigma(ik <= last ? ik : last), u10, fetch, one) * tail;
            for (int ith = 0; ith < g.nth; ++ith) {
              ue(ith, ik) = band * ww::cos2_spread(g.theta(ith), static_cast<ww::Real>(0));
            }
          });
          team.team_barrier();  // scratch is written above, read below

          double acc = 0.0;
          Kokkos::parallel_reduce(
              Kokkos::TeamThreadRange(team, g.nk),
              [&](const int ik, double& sum) {
                for (int ith = 0; ith < g.nth; ++ith) {
                  sum += static_cast<double>(ue(ith, ik)) *
                         static_cast<double>(g.dtheta()) * static_cast<double>(g.dsigma(ik));
                }
              },
              acc);
          Kokkos::single(Kokkos::PerTeam(team),
                         [&]() { m0(ip) = static_cast<ww::Real>(acc); });
        });

    auto host_m0 = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), m0);
    const double ref = static_cast<double>(ww::jonswap_m0(u10, fetch));
    err = std::fabs(static_cast<double>(host_m0(0)) - ref) / ref;
    for (int ip = 1; ip < npts; ++ip) {
      if (host_m0(ip) != host_m0(0)) err = 1.0;  // every team must agree
    }

    std::printf("05_team_scratch: %d teams, %zu B scratch each, m0=%.6e (closed form %.6e)\n",
                npts, scratch_bytes, static_cast<double>(host_m0(0)), ref);
  }

  if (!(err < 0.02)) {
    std::fprintf(stderr, "05_team_scratch: FAILED -- teams disagree or m0 is %.2f %% off\n",
                 100.0 * err);
    return 1;
  }
  return 0;
}
