// kokkos/src/ww_kokkos/profs_uppdir.hpp
// UPPDIR: updates the stored conjugate directions p of the SPARSKIT banded solver from the
// upper part of the banded triangular matrix u, in REAL(8), inside W3PROFSMD.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:2418-2459 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin. Evidence: kokkos/port_report/UPPDIR_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// UPPDIR. p is p(n, lbp), column major, Fortran base (1,1); y, u, usav are assumed-size,
/// Fortran base 1. indp and flops are read and written. The indices j, ju, k, indp keep the
/// Fortran's 1-based values; every array access subtracts 1.
KOKKOS_INLINE_FUNCTION void uppdir(int n, double* p, int np, int lbp, int& indp, double* y,
                                   const double* u, double* usav, double& flops) {
  const double zero = 0.0;
  const long ldp = n > 0 ? n : 0;  // p's leading dimension, MAX(n, 0) in the object

  const int npm1 = np - 1;
  if (np > 1) {
    int j = indp;
    int ju = npm1;
    for (;;) {
      if (j <= 0) j = lbp;
      const double x = u[ju - 1] / usav[j - 1];
      if (x != zero) {
        // :2440-2442. gfortran vectorises this in pairs; each y(k) is its own mulpd/subpd lane,
        // so the bits are those of the scalar loop.
        for (int k = 1; k <= n; ++k) {
          y[k - 1] = y[k - 1] - x * p[(k - 1) + (j - 1) * ldp];
        }
        // :2443. 2*n in INTEGER(4), converted to REAL(8), added even when n <= 0.
        flops = flops + static_cast<double>(2 * n);
      }
      j = j - 1;
      ju = ju - 1;
      if (ju < 1) break;
    }
  }
  indp = indp + 1;
  if (indp > lbp) indp = 1;
  usav[indp - 1] = u[np - 1];
  for (int k = 1; k <= n; ++k) {
    p[(k - 1) + (indp - 1) * ldp] = y[k - 1];
  }
}

}  // namespace ww::profs
