// kokkos/src/ww_kokkos/profs_lsolc.hpp
// lsolc: solves L x = y, L unit lower triangular in compressed sparse column storage, by forward
// elimination. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:3346-3389 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (module header); SPARSKIT routine. Evidence: kokkos/port_report/LSOLC_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// lsolc, REAL*8 throughout. x, y: n (Fortran base 1). al, jal: Fortran base 1, jal holds 1-based
/// row indices. ial: n (ial(1..n), Fortran base 1), 1-based positions into al/jal.
KOKKOS_INLINE_FUNCTION void lsolc(int n, double* x, const double* y, const double* al,
                                  const int* jal, const int* ial) {
  // :3376-3378.
  for (int k = 1; k <= n; ++k) {
    x[k - 1] = y[k - 1];
  }
  // :3379-3384. t is read once per column: a jal(j) equal to k does not update it.
  for (int k = 1; k <= n - 1; ++k) {
    const double t = x[k - 1];
    for (int j = ial[k - 1]; j <= ial[k] - 1; ++j) {
      x[jal[j - 1] - 1] = x[jal[j - 1] - 1] - t * al[j - 1];
    }
  }
}

}  // namespace ww::profs
