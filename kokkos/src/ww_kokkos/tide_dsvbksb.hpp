// kokkos/src/ww_kokkos/tide_dsvbksb.hpp
// DSVBKSB: SVD back-substitution x = V * diag(1/w) * U^T * b of the tidal
// least-squares fit (Numerical Recipes). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3tidemd.F90:995-1019 at NOAA-EMC/WW3 03adda91;
// module author M. Foreman (IOS), WW3 adaptation F. Ardhuin; routine (C) Numerical
// Recipes Software 1986-92. Evidence: kokkos/port_report/DSVBKSB_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::tide {

namespace dsvbksb_detail {
/// NMAX of :998; TMP(NMAX) is a local array in the Fortran too (no SAVE).
inline constexpr int kNmax = 500;
}  // namespace dsvbksb_detail

/// DSVBKSB, arrays 0-based, column-major as in the Fortran: u is U(MP,NP), v is V(NP,NP),
/// w(NP), b(MP), x(NP), all 1-based there. Like the Fortran, n > 500 overruns tmp.
KOKKOS_INLINE_FUNCTION void dsvbksb(const double* u, const double* w, const double* v,
                                    int m, int n, int mp, int np, const double* b,
                                    double* x) {
  double tmp[dsvbksb_detail::kNmax];
  const long ldu = mp;
  const long ldv = np;

  // :1000-1009. The -O3 object multiplies two lanes at a time but adds them to s in
  // index order, so the scalar loop gives the same bits.
  for (int j = 1; j <= n; ++j) {
    double s = 0.0;
    if (w[j - 1] != 0.0) {
      for (int i = 1; i <= m; ++i) {
        s = s + u[(i - 1) + (j - 1) * ldu] * b[i - 1];
      }
      s = s / w[j - 1];
    }
    tmp[j - 1] = s;
  }
  // :1010-1017.
  for (int j = 1; j <= n; ++j) {
    double s = 0.0;
    for (int jj = 1; jj <= n; ++jj) {
      s = s + v[(j - 1) + (jj - 1) * ldv] * tmp[jj - 1];
    }
    x[j - 1] = s;
  }
}

}  // namespace ww::tide
