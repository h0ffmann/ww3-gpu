// kokkos/src/ww_kokkos/profs_udsolc.hpp
// udsolc: solves U x = y, U non-unit upper triangular in modified sparse column storage with the
// diagonal stored inverted, by backward elimination. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:3626-3671 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (module header); SPARSKIT routine. Evidence: kokkos/port_report/UDSOLC_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// udsolc, REAL*8 throughout. x, y: n (Fortran base 1). au, jau: Fortran base 1; au(1:n) holds the
/// inverted diagonal, jau(1:n+1) the 1-based column pointers, jau(n+2:) the 1-based row indices.
/// No caller in WW3 (PRIVATE at w3profsmd.F90:61-65).
KOKKOS_INLINE_FUNCTION void udsolc(int n, double* x, const double* y, const double* au,
                                   const int* jau) {
  // :3657-3659.
  for (int k = 1; k <= n; ++k) {
    x[k - 1] = y[k - 1];
  }
  // :3660-3666. t is read after the scaling and held for the column.
  for (int k = n; k >= 1; --k) {
    x[k - 1] = x[k - 1] * au[k - 1];
    const double t = x[k - 1];
    for (int j = jau[k - 1]; j <= jau[k] - 1; ++j) {
      x[jau[j - 1] - 1] = x[jau[j - 1] - 1] - t * au[j - 1];
    }
  }
}

}  // namespace ww::profs
