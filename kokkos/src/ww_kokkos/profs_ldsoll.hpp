// kokkos/src/ww_kokkos/profs_ldsoll.hpp
// ldsoll: solves L x = y, L lower triangular in modified sparse row storage with the diagonal
// already inverted, row by row in level-scheduling order. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:3439-3490 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (module header); SPARSKIT routine. Evidence: kokkos/port_report/LDSOLL_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// ldsoll, REAL*8 throughout. x, y: n (Fortran base 1). al, jal: Fortran base 1; al(1:n) holds
/// the inverted diagonal, jal(1:n+1) 1-based positions into al/jal, jal(n+2:) 1-based column
/// indices. lev: n, 1-based row numbers; ilev: nlev+1, 1-based positions into lev.
KOKKOS_INLINE_FUNCTION void ldsoll(int n, double* x, const double* y, const double* al,
                                   const int* jal, int nlev, const int* lev, const int* ilev) {
  // :3472-3487. The Fortran runs the level loop sequentially; the inner one is kept serial too.
  for (int ii = 1; ii <= nlev; ++ii) {
    for (int i = ilev[ii - 1]; i <= ilev[ii] - 1; ++i) {
      const int jrow = lev[i - 1];
      double t = y[jrow - 1];
      for (int k = jal[jrow - 1]; k <= jal[jrow] - 1; ++k) {
        t = t - al[k - 1] * x[jal[k - 1] - 1];
      }
      x[jrow - 1] = t * al[jrow - 1];
    }
  }
}

}  // namespace ww::profs
