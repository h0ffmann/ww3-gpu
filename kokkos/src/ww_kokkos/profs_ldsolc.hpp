// kokkos/src/ww_kokkos/profs_ldsolc.hpp
// ldsolc: solves L x = y, L non-unit lower triangular in modified sparse column storage with the
// diagonal already inverted, by forward elimination. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:3391-3437 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (module header); SPARSKIT routine. Evidence: kokkos/port_report/LDSOLC_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// ldsolc, REAL*8 throughout. x, y: n (Fortran base 1). al, jal: Fortran base 1; al(1:n) holds
/// the inverted diagonal, jal(1:n+1) 1-based positions into al/jal, jal(n+2:) 1-based row indices.
KOKKOS_INLINE_FUNCTION void ldsolc(int n, double* x, const double* y, const double* al,
                                   const int* jal) {
  // :3423-3425.
  for (int k = 1; k <= n; ++k) {
    x[k - 1] = y[k - 1];
  }
  // :3426-3432. t is read once per column: a jal(j) equal to k does not update it.
  for (int k = 1; k <= n; ++k) {
    x[k - 1] = x[k - 1] * al[k - 1];
    const double t = x[k - 1];
    for (int j = jal[k - 1]; j <= jal[k] - 1; ++j) {
      x[jal[j - 1] - 1] = x[jal[j - 1] - 1] - t * al[j - 1];
    }
  }
}

}  // namespace ww::profs
