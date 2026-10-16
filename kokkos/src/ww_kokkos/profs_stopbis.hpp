// kokkos/src/ww_kokkos/profs_stopbis.hpp
// STOPBIS: the stopping test of the BCGSTAB solver (residual norm against the tolerance in FPAR).
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:2503-2557 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (the routine is SPARSKIT's, Y. Saad).
// Evidence: kokkos/port_report/STOPBIS_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "profs_ddot.hpp"

namespace ww::profs {

/// STOPBIS(N, IPAR, MVPI, FPAR, R, DELX, SX), arrays 0-based: ipar[16], fpar[16]
/// (Fortran IPAR(1:16), FPAR(1:16)), r[n], delx[n]. REAL*8 stays double. Returns the LOGICAL.
/// DDOT is a call in the object, not inlined, so calling ddot() keeps its order of adds.
KOKKOS_INLINE_FUNCTION bool stopbis(int n, int* ipar, int mvpi, double* fpar, const double* r,
                                    const double* delx, double sx) {
  // :2511-2515.
  bool res = (ipar[10] == 1);
  // :2516-2519.
  if (ipar[5] > 0 && ipar[6] >= ipar[5]) {
    ipar[0] = -1;
    res = true;
  }
  // :2520.
  if (res) return res;

  // :2524-2525. The object computes 2*N in int and converts it (cvtsi2sd), then addsd.
  fpar[4] = Kokkos::sqrt(ddot(n, r, r));
  fpar[10] = fpar[10] + static_cast<double>(2 * n);
  if (ipar[2] < 0) {
    // :2530-2531.
    fpar[5] = sx * Kokkos::sqrt(ddot(n, delx, delx));
    fpar[10] = fpar[10] + static_cast<double>(2 * n);
    // :2532-2540.
    if (ipar[6] < mvpi + mvpi + 1) {
      fpar[2] = fpar[5];
      if (ipar[2] == -1) {
        fpar[3] = fpar[0] * fpar[2] + fpar[1];
      }
    }
  } else {
    // :2542.
    fpar[5] = fpar[4];
  }
  // :2548-2554. A NaN in fpar[5] makes the test false and STOPBIS true, as the Fortran
  // (comisd + jbe in the object).
  if (fpar[5] > fpar[3]) {
    res = false;
    ipar[10] = 0;
  } else {
    res = true;
    ipar[10] = 1;
  }
  return res;
}

}  // namespace ww::profs
