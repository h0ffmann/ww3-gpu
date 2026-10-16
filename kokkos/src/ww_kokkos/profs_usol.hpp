// kokkos/src/ww_kokkos/profs_usol.hpp
// usol: solves U x = y, U unit upper triangular in compressed sparse row storage (backward
// elimination). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:3492-3534 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (module header); SPARSKIT routine. Evidence: kokkos/port_report/USOL_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// usol, REAL*8 throughout. x, y: n (Fortran base 1). au, jau: Fortran base 1, jau holds 1-based
/// column indices; iau: n+1 (Fortran base 1), 1-based row pointers.
/// No caller in WW3 (w3profsmd.F90:63 only makes it PUBLIC).
KOKKOS_INLINE_FUNCTION void usol(int n, double* x, const double* y, const double* au,
                                 const int* jau, const int* iau) {
  // :3522-3529.
  x[n - 1] = y[n - 1];
  for (int k = n - 1; k >= 1; --k) {
    double t = y[k - 1];
    for (int j = iau[k - 1]; j <= iau[k] - 1; ++j) {
      t = t - au[j - 1] * x[jau[j - 1] - 1];
    }
    x[k - 1] = t;
  }
}

}  // namespace ww::profs
