// kokkos/src/ww_kokkos/profs_amux.hpp
// AMUX: y = A x for a REAL(8) matrix in compressed sparse row (CSR) storage, a SPARSKIT
// routine kept inside W3PROFSMD. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:2856-2901 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (the routine is Y. Saad's, SPARSKIT).
// Evidence: kokkos/port_report/AMUX_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// AMUX(N, X, Y, A, JA, IA); x is X(1:ncol), y is Y(1:N), a is A(1:nnz), ja is JA(1:nnz),
/// ia is IA(1:N+1), all 0-based here. ja and ia hold the Fortran's 1-based indices.
KOKKOS_INLINE_FUNCTION void amux(int n, const double* x, double* y, const double* a,
                                 const int* ja, const int* ia) {
  for (int i = 1; i <= n; ++i) {
    double t = 0.0;
    // :2890. The DO bound ia(i+1)-1 is evaluated once, before the loop.
    const int kend = ia[i] - 1;
    for (int k = ia[i - 1]; k <= kend; ++k) t = t + a[k - 1] * x[ja[k - 1] - 1];
    y[i - 1] = t;
  }
}

}  // namespace ww::profs
