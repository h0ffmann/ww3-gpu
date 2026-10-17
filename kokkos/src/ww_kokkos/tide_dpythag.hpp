// kokkos/src/ww_kokkos/tide_dpythag.hpp
// DPYTHAG: sqrt(a**2+b**2) without overflow, in double precision (Numerical Recipes),
// used by the SVD of the tidal analysis. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3tidemd.F90:976-991 at NOAA-EMC/WW3 03adda91;
// authors M. Foreman (tidal package), adapted to WW3 by F. Ardhuin.
// Evidence: kokkos/port_report/DPYTHAG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::tide {

/// The Fortran parentheses of (absb/absa)**2 as gfortran keeps them (PAREN_EXPR in the GIMPLE).
/// Without the barrier g++ drops both fabs because the quotient is squared, divides b/a, and
/// a negative NaN input leaves a negative NaN where the Fortran returns a positive one.
KOKKOS_INLINE_FUNCTION double dpythag_paren(double x) {
// nvcc's front end reports GCC's builtin and cannot parse it, in its host pass too; under
// nvcc the barrier is absent (the CUDA build is not regtested).
#if defined(__has_builtin) && !defined(__CUDACC__)
#if __has_builtin(__builtin_assoc_barrier)
  return __builtin_assoc_barrier(x);
#else
  return x;
#endif
#else
  return x;
#endif
}

/// DPYTHAG(A,B), DOUBLE PRECISION in and out.
KOKKOS_INLINE_FUNCTION double dpythag(double a, double b) {
  const double absa = Kokkos::fabs(a);
  const double absb = Kokkos::fabs(b);
  if (absa > absb) {
    // (x)**2 is x*x in the object; contraction is off, so no FMA with the 1.0d0.
    const double r = dpythag_paren(absb / absa);
    return absa * Kokkos::sqrt(1.0 + r * r);
  }
  if (absb == 0.0) return 0.0;
  const double r = dpythag_paren(absa / absb);
  return absb * Kokkos::sqrt(1.0 + r * r);
}

}  // namespace ww::tide
