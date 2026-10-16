// kokkos/src/ww_kokkos/profs_amuxms.hpp
// AMUXMS: y = A x for a REAL(8) matrix in modified sparse row (MSR) storage, a SPARSKIT
// routine kept inside W3PROFSMD. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:2903-2944 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (the routine is Y. Saad's, SPARSKIT).
// Evidence: kokkos/port_report/AMUXMS_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// AMUXMS(N, X, Y, A, JA); x is X(1:ncol), y is Y(1:N), a is A(1:nnz), ja is JA(1:nnz), all
/// 0-based here. ja holds the Fortran's 1-based indices: ja[0..n] are the row pointers of the
/// off-diagonal part, the rest column numbers; a[0..n-1] is the diagonal.
KOKKOS_INLINE_FUNCTION void amuxms(int n, const double* x, double* y, const double* a,
                                   const int* ja) {
  // :2932-2934. The object vectorises this loop (mulpd); each product is element-wise, so
  // the scalar loop gives the same bits.
  for (int i = 1; i <= n; ++i) y[i - 1] = a[i - 1] * x[i - 1];
  // :2935-2941. The object keeps y(i) in a register over the k loop and stores it once
  // after it (Fortran dummies do not alias); the local reproduces that.
  for (int i = 1; i <= n; ++i) {
    const int kend = ja[i] - 1;
    double yi = y[i - 1];
    for (int k = ja[i - 1]; k <= kend; ++k) yi = yi + a[k - 1] * x[ja[k - 1] - 1];
    y[i - 1] = yi;
  }
}

}  // namespace ww::profs
