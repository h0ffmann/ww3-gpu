// kokkos/src/ww_kokkos/profs_udsol.hpp
// udsol: solves U x = y, U non-unit upper triangular in modified sparse row storage with the
// diagonal stored inverted (backward elimination). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:3536-3579 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (module header); SPARSKIT routine. Evidence: kokkos/port_report/UDSOL_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// udsol, REAL*8 throughout. x, y: n (Fortran base 1). au, jau: Fortran base 1; au(1:n) holds the
/// inverted diagonal, jau(1:n+1) the 1-based row pointers, jau(n+2:) the 1-based column indices.
/// No caller in WW3 (w3profsmd.F90:64 only makes it PUBLIC).
KOKKOS_INLINE_FUNCTION void udsol(int n, double* x, const double* y, const double* au,
                                  const int* jau) {
  // :3567-3574.
  x[n - 1] = y[n - 1] * au[n - 1];
  for (int k = n - 1; k >= 1; --k) {
    double t = y[k - 1];
    for (int j = jau[k - 1]; j <= jau[k] - 1; ++j) {
      t = t - au[j - 1] * x[jau[j - 1] - 1];
    }
    x[k - 1] = au[k - 1] * t;
  }
}

}  // namespace ww::profs
