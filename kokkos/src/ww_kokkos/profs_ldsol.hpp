// kokkos/src/ww_kokkos/profs_ldsol.hpp
// ldsol: solves L x = y by forward substitution, L lower triangular in modified sparse row
// storage with its diagonal stored inverted. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:3302-3344 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (module header); SPARSKIT routine. Evidence: kokkos/port_report/LDSOL_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// ldsol, REAL*8 throughout. x, y: n (Fortran base 1). al, jal: Fortran base 1; jal(1:n+1) holds
/// 1-based row pointers and the rest 1-based column indices, so jal needs jal[n] (jal(n+1)).
/// x(1) is written even when n < 1, as the Fortran does (:3333).
KOKKOS_INLINE_FUNCTION void ldsol(int n, double* x, const double* y, const double* al,
                                  const int* jal) {
  // :3333-3340.
  x[0] = y[0] * al[0];
  for (int k = 2; k <= n; ++k) {
    double t = y[k - 1];
    for (int j = jal[k - 1]; j <= jal[k] - 1; ++j) {
      t = t - al[j - 1] * x[jal[j - 1] - 1];
    }
    x[k - 1] = al[k - 1] * t;
  }
}

}  // namespace ww::profs
