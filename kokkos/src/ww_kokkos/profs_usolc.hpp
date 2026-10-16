// kokkos/src/ww_kokkos/profs_usolc.hpp
// usolc: solves U x = y, U unit upper triangular in compressed sparse column storage, by backward
// elimination. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:3581-3624 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (module header); SPARSKIT routine. Evidence: kokkos/port_report/USOLC_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// usolc, REAL*8 throughout. x, y: n (Fortran base 1). au, jau: Fortran base 1, jau holds 1-based
/// row indices. iau: n+1 (iau(1..n+1), Fortran base 1), 1-based positions into au/jau.
KOKKOS_INLINE_FUNCTION void usolc(int n, double* x, const double* y, const double* au,
                                  const int* jau, const int* iau) {
  // :3611-3613.
  for (int k = 1; k <= n; ++k) {
    x[k - 1] = y[k - 1];
  }
  // :3614-3619. Column 1 is visited too, unlike lsolc; t is read once per column.
  for (int k = n; k >= 1; --k) {
    const double t = x[k - 1];
    for (int j = iau[k - 1]; j <= iau[k] - 1; ++j) {
      x[jau[j - 1] - 1] = x[jau[j - 1] - 1] - t * au[j - 1];
    }
  }
}

}  // namespace ww::profs
