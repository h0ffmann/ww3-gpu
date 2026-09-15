// kokkos/intro/01_views.cpp
// Lesson 11.1 -- Kokkos::View: allocation, label, extents, layout, host mirror.
//
// A View is a reference-counted, multi-dimensional array whose memory space and
// index order are part of its type. The one rule that catches every newcomer:
// you may not dereference a device View from host code. You copy it to a mirror
// first. On a host-only backend the mirror is the same allocation and deep_copy
// is a no-op, which is why the pattern costs nothing when it is not needed.
//
// SPDX-License-Identifier: MIT

#include <Kokkos_Core.hpp>

#include <cstdio>

#include "ww_kokkos/real.hpp"

namespace {

// The memory space is part of a View's type, and this lab spells it out rather
// than taking the default. `DefaultExecutionSpace::memory_space` is HostSpace for
// the Serial/OpenMP presets and CudaSpace for cuda-release, so the alias is the
// same code everywhere -- but it is now visible at the declaration whether a View
// is reachable from host code or not. Leaving it implicit is how host code ends up
// dereferencing device memory.
using DeviceSpace = Kokkos::DefaultExecutionSpace::memory_space;

}  // namespace

int main(int argc, char* argv[]) {
  Kokkos::ScopeGuard guard(argc, argv);
  bool ok = true;
  {
    // Views must die before Kokkos::finalize(), hence this inner scope: the
    // ScopeGuard finalises at the closing brace of main, not of this block.
    constexpr int nth = 24;
    constexpr int nk = 32;

    // The string is a label. It is not decoration: it is what shows up in the
    // profiler, in a bounds-check abort and in a "View destroyed after finalize"
    // message, so it is always worth spelling properly.
    Kokkos::View<ww::Real**, DeviceSpace> spectrum("intro.spectrum", nth, nk);

    // extent(i) is size_t; rank and extents are compile-time/runtime metadata
    // carried by the View itself, so no separate nth/nk arguments travel around.
    ok = ok && spectrum.rank() == 2u;
    ok = ok && spectrum.extent(0) == static_cast<size_t>(nth);
    ok = ok && spectrum.extent(1) == static_cast<size_t>(nk);

    // A freshly allocated View is value-initialised (zero) unless you ask for
    // Kokkos::view_alloc(Kokkos::WithoutInitializing, ...).
    Kokkos::parallel_for(
        "intro.01.fill", Kokkos::RangePolicy<>(0, nth),
        KOKKOS_LAMBDA(const int ith) {
          for (int ik = 0; ik < nk; ++ik) {
            spectrum(ith, ik) = static_cast<ww::Real>(ith * nk + ik);
          }
        });

    // create_mirror_view gives a host View with the *same layout*; on a host
    // backend it aliases the original, on CUDA it is a fresh host allocation.
    auto host = Kokkos::create_mirror_view(spectrum);
    Kokkos::deep_copy(host, spectrum);

    ok = ok && host(0, 0) == static_cast<ww::Real>(0);
    ok = ok && host(nth - 1, nk - 1) == static_cast<ww::Real>(nth * nk - 1);

    // A default-layout 2D View is one dense block, so a bulk memcpy or a raw
    // pointer hand-off to C or Fortran is legal. Strided sub-views are not.
    ok = ok && spectrum.span_is_contiguous();

    std::printf("01_views: label=%s rank=%zu extents=%zux%zu contiguous=%d span=%zu\n",
                spectrum.label().c_str(), spectrum.rank(), spectrum.extent(0),
                spectrum.extent(1), static_cast<int>(spectrum.span_is_contiguous()),
                spectrum.span());
  }

  if (!ok) {
    std::fprintf(stderr, "01_views: FAILED -- View metadata or mirror copy is wrong\n");
    return 1;
  }
  return 0;
}
