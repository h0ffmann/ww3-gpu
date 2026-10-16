// kokkos/src/ww_kokkos/gke_asymsmattimvec.hpp
// ASymSmatTimVec of W3GKEMD: y = (A + Symb A^T) x for an upper triangular CSR matrix A.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3gkemd.F90:983-1061 at NOAA-EMC/WW3 03adda91;
// authors O. Gramstad, Q. Liu. Evidence: kokkos/port_report/ASYMSMATTIMVEC_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::gke {

/// ASymSmatTimVec(n, a, ja, ia, x, y, Symb), :983-1061. Runs on one thread: the transposed
/// product scatters into y(ja(k)), and the order of those adds is the Fortran's.
/// a[nnz], ja[nnz] (Fortran a(:), ja(:), assumed shape, base 1; the only caller, CalcQRSNL
/// :1872, passes whole contiguous arrays), ia[n+1] (base 1), x[n], y[n] (base 1).
/// ja and ia hold Fortran (1-based) indices, as WW3 stores them.
KOKKOS_INLINE_FUNCTION void asymsmattimvec(int n, const Real* a, const int* ja, const int* ia,
                                           const Real* x, Real* y, Real symb) {
  // :1043.
  for (int i = 0; i < n; ++i) y[i] = static_cast<Real>(0.0);
  // :1045-1056; sequential in k as the object (no vectorised partial sum).
  for (int i = 1; i <= n; ++i) {
    Real t = static_cast<Real>(0.0);
    for (int k = ia[i - 1]; k <= ia[i] - 1; ++k) {
      t = t + a[k - 1] * x[ja[k - 1] - 1];
      y[ja[k - 1] - 1] = y[ja[k - 1] - 1] + symb * a[k - 1] * x[i - 1];
    }
    y[i - 1] = y[i - 1] + t;
  }
}

}  // namespace ww::gke
