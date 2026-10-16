// kokkos/src/ww_kokkos/profs_lsol.hpp
// lsol: solves L x = y, L unit lower triangular in compressed sparse row storage, by
// forward elimination. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:3258-3300 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (module header); SPARSKIT routine. Evidence: kokkos/port_report/LSOL_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// lsol, REAL*8 throughout. x, y: n (Fortran base 1). al, jal: Fortran base 1, jal holds
/// 1-based column indices; ial: n+1, 1-based row starts. The diagonal is implied (unit) and
/// absent from al, so only ial(2:n+1) is read.
KOKKOS_INLINE_FUNCTION void lsol(int n, double* x, const double* y, const double* al,
                                 const int* jal, const int* ial) {
  // :3290-3297. x(1) = y(1) is executed even for n < 1, as in the Fortran.
  x[0] = y[0];
  for (int k = 2; k <= n; ++k) {
    double t = y[k - 1];
    for (int j = ial[k - 1]; j <= ial[k] - 1; ++j) {
      t = t - al[j - 1] * x[jal[j - 1] - 1];
    }
    x[k - 1] = t;
  }
}

}  // namespace ww::profs
