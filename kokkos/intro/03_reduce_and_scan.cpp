// kokkos/intro/03_reduce_and_scan.cpp
// Lesson 11.3 -- reducers other than "+", and parallel_scan.
//
// Two patterns the DIA port needs. First a reduction that carries a payload:
// Kokkos::MaxLoc returns the largest Hs *and the point it happened at*, which a
// plain max reduction cannot. Second a prefix sum: parallel_scan turns per-cell
// distances into cumulative fetch along a wind-aligned transect, in one pass, with
// no host round trip.
//
// The execution space is chosen at the top: a WW_DETERMINISTIC build pins the scan
// to Kokkos::Serial so the summation order -- and therefore the last bit of every
// float -- is fixed. That is what makes a validation build comparable with itself.
//
// SPDX-License-Identifier: MIT

#include <Kokkos_Core.hpp>

#include <cmath>
#include <cstdio>
#include <type_traits>

#include "ww_kokkos/real.hpp"
#include "ww_kokkos/spectrum_fixtures.hpp"

namespace {

// Spell the memory space out: HostSpace under the Serial/OpenMP presets,
// CudaSpace under cuda-release. Never take the default for a View you allocate.
using DeviceSpace = Kokkos::DefaultExecutionSpace::memory_space;

#if defined(WW_DETERMINISTIC) && defined(KOKKOS_ENABLE_SERIAL)
// Serial is only a legal choice if it can reach the default memory space; on a
// CUDA build it cannot, and a bit-reproducible run needs a different answer than
// "run it on the host". SpaceAccessibility is how you ask that question.
using ScanSpace =
    std::conditional_t<Kokkos::SpaceAccessibility<Kokkos::Serial, DeviceSpace>::accessible,
                       Kokkos::Serial, Kokkos::DefaultExecutionSpace>;
#else
using ScanSpace = Kokkos::DefaultExecutionSpace;
#endif

}  // namespace

int main(int argc, char* argv[]) {
  Kokkos::ScopeGuard guard(argc, argv);
  bool ok = true;
  {
    constexpr int npts = 64;
    const ww::Real u10 = static_cast<ww::Real>(10);
    const ww::Real dx0 = static_cast<ww::Real>(5.0e3);  // 5 km at the coast

    Kokkos::View<ww::Real*, DeviceSpace> dx("intro.dx", npts);
    Kokkos::View<ww::Real*, DeviceSpace> fetch("intro.fetch", npts);
    Kokkos::View<ww::Real*, DeviceSpace> hs("intro.hs", npts);

    // A stretched transect: cells widen offshore, as a nested WW3 grid does. The
    // cumulative fetch is therefore a real prefix sum, not i * dx.
    Kokkos::parallel_for(
        "intro.03.grid", Kokkos::RangePolicy<>(0, npts), KOKKOS_LAMBDA(const int i) {
          dx(i) = dx0 * (static_cast<ww::Real>(1) +
                         static_cast<ww::Real>(i) / static_cast<ww::Real>(npts));
        });

    // parallel_scan's functor is called more than once per index: Kokkos runs it
    // first to collect partial sums and again with is_final = true to write the
    // results. Never put a side effect outside the is_final branch.
    Kokkos::parallel_scan(
        "intro.03.cumulative_fetch", Kokkos::RangePolicy<ScanSpace>(0, npts),
        KOKKOS_LAMBDA(const int i, ww::Real& partial, const bool is_final) {
          partial += dx(i);
          if (is_final) fetch(i) = partial;
        });

    Kokkos::parallel_for(
        "intro.03.hs", Kokkos::RangePolicy<>(0, npts), KOKKOS_LAMBDA(const int i) {
          hs(i) = static_cast<ww::Real>(4) * Kokkos::sqrt(ww::jonswap_m0(u10, fetch(i)));
        });

    // MaxLoc's value_type is a {val, loc} pair. The reducer, not the lambda, owns
    // the identity element and the join, so this is correct on every backend.
    using MaxHs = Kokkos::MaxLoc<ww::Real, int>;
    MaxHs::value_type peak;
    Kokkos::parallel_reduce(
        "intro.03.peak", npts,
        KOKKOS_LAMBDA(const int i, MaxHs::value_type& upd) {
          if (hs(i) > upd.val) {
            upd.val = hs(i);
            upd.loc = i;
          }
        },
        MaxHs(peak));

    // Hs grows monotonically with fetch, so the peak must be at the last point,
    // where the cumulative fetch must equal the sum of all the cell widths.
    auto host_fetch = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), fetch);
    double total = 0.0;
    for (int i = 0; i < npts; ++i) {
      total += static_cast<double>(dx0) *
               (1.0 + static_cast<double>(i) / static_cast<double>(npts));
    }
    ok = ok && peak.loc == npts - 1;
    ok = ok && std::fabs(static_cast<double>(host_fetch(npts - 1)) - total) < 1.0;

    std::printf("03_reduce_and_scan: scan space=%s peak Hs=%.4f m at point %d (fetch %.1f km)\n",
                ScanSpace::name(), static_cast<double>(peak.val), peak.loc,
                static_cast<double>(host_fetch(npts - 1)) / 1.0e3);
  }

  if (!ok) {
    std::fprintf(stderr, "03_reduce_and_scan: FAILED -- peak location or cumulative fetch is wrong\n");
    return 1;
  }
  return 0;
}
