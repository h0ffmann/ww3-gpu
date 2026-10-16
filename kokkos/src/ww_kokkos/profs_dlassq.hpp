// kokkos/src/ww_kokkos/profs_dlassq.hpp
// DLASSQ: the LAPACK scaled sum of squares of a REAL(8) vector, kept inside W3PROFSMD.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:4638-4681 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (the routine is LAPACK 3.1's, Univ. of Tennessee, Univ. of
// California Berkeley and NAG Ltd., 2006).
// Evidence: kokkos/port_report/DLASSQ_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// The Fortran parentheses of (SCALE/ABSXI)**2 and (ABSXI/SCALE)**2 as gfortran keeps them
/// (PAREN_EXPR in the GIMPLE). Without the barrier g++ drops the fabs because the quotient is
/// squared and divides by the signed X(IX): a negative NaN in X then leaves a negative NaN in
/// SUMSQ where the Fortran leaves a positive one.
KOKKOS_INLINE_FUNCTION double dlassq_paren(double v) {
#if defined(__has_builtin)
#if __has_builtin(__builtin_assoc_barrier)
  return __builtin_assoc_barrier(v);
#else
  return v;
#endif
#else
  return v;
#endif
}

/// DLASSQ(N, X, SCALE, SUMSQ); x is X(1:N) 0-based, unit stride (the Fortran has no INCX).
/// scale and sumsq are INTENT(INOUT) and written at every update, as the object stores them.
KOKKOS_INLINE_FUNCTION void dlassq(int n, const double* x, double& scale, double& sumsq) {
  // :4667.
  if (n > 0) {
    // :4668.
    for (int ix = 1; ix <= 1 + (n - 1); ++ix) {
      // :4669. A NaN passes this test and then takes the ELSE branch at :4671, as in the object.
      if (x[ix - 1] != 0.0) {
        const double absxi = Kokkos::fabs(x[ix - 1]);
        if (scale < absxi) {
          // :4672. (r)**2 is r*r in the object; the product with SUMSQ comes after.
          const double r = dlassq_paren(scale / absxi);
          sumsq = 1.0 + sumsq * (r * r);
          scale = absxi;
        } else {
          // :4675.
          const double r = dlassq_paren(absxi / scale);
          sumsq = sumsq + r * r;
        }
      }
    }
  }
}

}  // namespace ww::profs
