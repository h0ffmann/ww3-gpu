// kokkos/src/ww_kokkos/profs_dnrm2.hpp
// DNRM2: the BLAS-1 Euclidean norm of a REAL(8) vector, with DLASSQ's loop inlined, kept
// inside W3PROFSMD.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:4568-4635 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (the routine is BLAS-1's, S. Hammarling, NAG Ltd., 1982,
// DLASSQ inlined 1993).
// Evidence: kokkos/port_report/DNRM2_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "profs_dlassq.hpp"

namespace ww::profs {

/// DNRM2(N, X); x is X(1:N) 0-based, unit stride (the Fortran has no INCX).
KOKKOS_INLINE_FUNCTION double dnrm2(int n, const double* x) {
  double norm;
  // :4605.
  if (n < 1) {
    norm = 0.0;
  } else if (n == 1) {
    norm = Kokkos::fabs(x[0]);
  } else {
    double scale = 0.0;
    double ssq = 1.0;
    // :4616-4626, DLASSQ's loop. A NaN passes X(IX) /= 0 and takes the ELSE branch at :4623.
    for (int ix = 1; ix <= 1 + (n - 1); ++ix) {
      if (x[ix - 1] != 0.0) {
        const double absxi = Kokkos::fabs(x[ix - 1]);
        if (scale < absxi) {
          // :4620. (r)**2 is r*r in the object; the product with SSQ comes after. The
          // barrier keeps the PAREN_EXPR, without it g++ drops the fabs (dlassq_paren).
          const double r = dlassq_paren(scale / absxi);
          ssq = 1.0 + ssq * (r * r);
          scale = absxi;
        } else {
          // :4623.
          const double r = dlassq_paren(absxi / scale);
          ssq = ssq + r * r;
        }
      }
    }
    // :4627.
    norm = scale * Kokkos::sqrt(ssq);
  }
  return norm;
}

}  // namespace ww::profs
