// kokkos/intro/06_interop_bindc.cpp
// Lesson 11.6 -- the bind(C) boundary: C++ side.
//
// The smallest honest version of what Task 4's shim does for W3SNL1. Three rules
// are on display and all three are load-bearing:
//
//  1. extern "C" -- no name mangling, no overloads, no references, no templates in
//     the signature. Fortran can only call a C ABI.
//  2. The array is *borrowed*. An unmanaged View wraps the caller's pointer; Kokkos
//     neither allocates nor frees it, and the Fortran side stays the owner.
//  3. LayoutLeft, because the pointer came from Fortran. The View is 1D here so
//     layout cannot bite yet -- it is written out anyway, because the 2D version in
//     the real shim gets this wrong silently if you leave it to the default.
//
// Kokkos::initialize() may be called exactly once per process, so init/finalize are
// their own entry points rather than a guard inside the worker.
//
// SPDX-License-Identifier: MIT

#include <Kokkos_Core.hpp>

#include "ww_kokkos/real.hpp"

namespace {

/// Unmanaged, Fortran-ordered, host-resident: the shape of every borrowed array.
using BorrowedArray =
    Kokkos::View<ww::Real*, Kokkos::LayoutLeft, Kokkos::HostSpace,
                 Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

}  // namespace

extern "C" {

/// Start the Kokkos runtime. Call once, before any other ww_intro_* entry point.
void ww_intro_init(void) { Kokkos::initialize(); }

/// Multiply the first `n` elements of the Fortran array `x` by `s`, in parallel.
void ww_intro_scale(int n, float* x, float s) {
  BorrowedArray view(x, n);
  Kokkos::parallel_for(
      "intro.06.scale", Kokkos::RangePolicy<Kokkos::DefaultHostExecutionSpace>(0, n),
      KOKKOS_LAMBDA(const int i) { view(i) *= s; });
  Kokkos::fence();  // the caller reads x the instant we return
}

/// Shut the Kokkos runtime down. Call once, after the last ww_intro_* call.
void ww_intro_finalize(void) { Kokkos::finalize(); }

}  // extern "C"
