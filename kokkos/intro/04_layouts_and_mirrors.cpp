// kokkos/intro/04_layouts_and_mirrors.cpp
// Lesson 11.4 -- LayoutLeft vs LayoutRight, mirrors, and unmanaged views.
//
// Layout is part of a View's type, and it decides which index is contiguous:
//   LayoutLeft  (column-major, Fortran order): stride(0) == 1
//   LayoutRight (row-major, C order):          stride(1) == 1
// Kokkos picks LayoutRight for host spaces and LayoutLeft for CUDA, because each
// is the one that coalesces on that backend. You override it in exactly one place:
// the Fortran boundary. A WW3 array declared A(NTH,NK) is laid out with NTH
// contiguous, so a View wrapping that same memory must be LayoutLeft or every
// index is silently transposed -- the array still "works", it is just wrong.
//
// SPDX-License-Identifier: MIT

#include <Kokkos_Core.hpp>

#include <cstdio>
#include <vector>

#include "ww_kokkos/real.hpp"

int main(int argc, char* argv[]) {
  Kokkos::ScopeGuard guard(argc, argv);
  bool ok = true;
  {
    constexpr int nth = 24;
    constexpr int nk = 32;

    Kokkos::View<ww::Real**, Kokkos::LayoutLeft, Kokkos::HostSpace> left("intro.left", nth, nk);
    Kokkos::View<ww::Real**, Kokkos::LayoutRight, Kokkos::HostSpace> right("intro.right", nth, nk);

    ok = ok && left.stride(0) == 1u && left.stride(1) == static_cast<size_t>(nth);
    ok = ok && right.stride(1) == 1u && right.stride(0) == static_cast<size_t>(nk);

    for (int ik = 0; ik < nk; ++ik) {
      for (int ith = 0; ith < nth; ++ith) {
        left(ith, ik) = static_cast<ww::Real>(ith + ik * nth);
      }
    }

    // deep_copy between different layouts is a transpose, not a memcpy. It is
    // correct and it is expensive -- which is the argument for choosing the
    // layout once, at the boundary, instead of converting inside a time step.
    Kokkos::deep_copy(right, left);
    ok = ok && right(3, 5) == left(3, 5);

    // A mirror always has the layout of the View it mirrors, so round-tripping
    // through a mirror never transposes anything behind your back.
    auto mirror = Kokkos::create_mirror_view(left);
    Kokkos::deep_copy(mirror, left);
    ok = ok && static_cast<size_t>(mirror.stride(0)) == static_cast<size_t>(left.stride(0));

    // An unmanaged View borrows memory it does not own: no allocation, no
    // reference count, no free. This is exactly how the bind(C) shim in Task 4
    // will wrap a Fortran array -- the caller keeps ownership.
    std::vector<ww::Real> buffer(static_cast<size_t>(nth) * static_cast<size_t>(nk));
    Kokkos::View<ww::Real**, Kokkos::LayoutLeft, Kokkos::HostSpace,
                 Kokkos::MemoryTraits<Kokkos::Unmanaged>>
        borrowed(buffer.data(), nth, nk);
    borrowed(7, 2) = static_cast<ww::Real>(1.5);

    // The Fortran-order promise, spelled out: element (ith, ik) is at ith + ik*nth.
    ok = ok && buffer[static_cast<size_t>(7 + 2 * nth)] == static_cast<ww::Real>(1.5);

    std::printf("04_layouts_and_mirrors: left stride=(%zu,%zu) right stride=(%zu,%zu) "
                "unmanaged (7,2) -> buffer[%d]\n",
                static_cast<size_t>(left.stride(0)), static_cast<size_t>(left.stride(1)),
                static_cast<size_t>(right.stride(0)), static_cast<size_t>(right.stride(1)),
                7 + 2 * nth);
  }

  if (!ok) {
    std::fprintf(stderr, "04_layouts_and_mirrors: FAILED -- a stride or an index is wrong\n");
    return 1;
  }
  return 0;
}
