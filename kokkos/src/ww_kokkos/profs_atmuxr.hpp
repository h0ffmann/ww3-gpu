// kokkos/src/ww_kokkos/profs_atmuxr.hpp
// ATMUXR: y = transpose(A) x for a rectangular REAL(8) matrix in compressed sparse row (CSR)
// storage, a SPARSKIT routine kept inside W3PROFSMD. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:2996-3044 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (the routine is Y. Saad's, SPARSKIT).
// Evidence: kokkos/port_report/ATMUXR_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// ATMUXR(M, N, X, Y, A, JA, IA); x is X(1:N), y is Y(1:M), a and ja are A(1:nnz), JA(1:nnz),
/// ia is IA(1:N+1), all 0-based here. ja and ia hold the Fortran's 1-based indices.
/// Y(1:M) is zeroed (:3029-3031); ja(k) must lie in 1..M, which the Fortran does not check.
KOKKOS_INLINE_FUNCTION void atmuxr(int m, int n, const double* x, double* y, const double* a,
                                   const int* ja, const int* ia) {
  for (int i = 1; i <= m; ++i) y[i - 1] = 0.0;
  // :3035-3039. The object loads x(i) once per row and loads and stores y(ja(k)) on every k
  // (a scatter: two k of one row may hit the same y).
  for (int i = 1; i <= n; ++i) {
    const int kend = ia[i] - 1;
    const double xi = x[i - 1];
    for (int k = ia[i - 1]; k <= kend; ++k) {
      const int j = ja[k - 1] - 1;
      y[j] = y[j] + xi * a[k - 1];
    }
  }
}

}  // namespace ww::profs
