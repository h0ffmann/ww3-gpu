// kokkos/src/ww_kokkos/tide_svd.hpp
// SVD: least-squares solution of the tidal harmonic fit by singular-value decomposition,
// with the residuals, their sum of squares and largest magnitude, and the covariance.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3tidemd.F90:1313-1443 at NOAA-EMC/WW3 03adda91;
// routine by J. Cherniawsky (1997-98), module author M. Foreman (IOS), WW3 adaptation
// F. Ardhuin. Evidence: kokkos/port_report/SVD_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "tide_dsvbksb.hpp"

namespace ww::tide {

namespace svd_detail {
/// NWT of :1355; WTI(NWT) is a local array in the Fortran too (no SAVE).
inline constexpr int kNwt = 302;
}  // namespace svd_detail

/// SVD, arrays 0-based, column-major as in the Fortran (all 1-based there): q is Q(MM,N2)
/// REAL, u is U(MM,N2), v and cov are (N2,N2), w and p are (N2), b and sig are (MM), all
/// DOUBLE PRECISION. sig is unused, as in the Fortran. With ic == 2, u, w and v must hold
/// the output of an earlier call, as the Fortran assumes of its INTENT(OUT) dummies.
/// dsvdcmp(double* a, int m, int n, int mp, int np, double* w, double* v) is DSVDCMP
/// (:1023-1254), not translated here. Like the Fortran, n > 302 overruns wti.
template <class Dsvdcmp>
KOKKOS_INLINE_FUNCTION void svd(float* q, double* u, double* v, double* cov, double* w,
                                double* p, double* b, const double* sig, int ic, int m,
                                int n, int mm, int n2, float toler, int& jc, float& ssq,
                                float& res, Dsvdcmp&& dsvdcmp) {
  double wti[svd_detail::kNwt];
  const long ldq = mm;
  const long ldv = n2;

  // :1376-1379.
  jc = 0;
  for (int i = 1; i <= mm; ++i) {
    b[i - 1] = static_cast<double>(q[(i - 1) + (n2 - 1) * ldq]);
  }
  // :1381-1404.
  if (ic != 2) {
    for (int j = 1; j <= n2; ++j) {
      for (int i = 1; i <= mm; ++i) {
        u[(i - 1) + (j - 1) * ldq] = static_cast<double>(q[(i - 1) + (j - 1) * ldq]);
      }
    }
    dsvdcmp(u, m, n, mm, n2, w, v);
    // wmax is REAL: each W(J) is compared with it widened and stored into it rounded.
    float wmax = 0.f;
    for (int j = 1; j <= n; ++j) {
      if (w[j - 1] > static_cast<double>(wmax)) wmax = static_cast<float>(w[j - 1]);
    }
    const float thresh = toler * wmax;
    for (int j = 1; j <= n; ++j) {
      if (w[j - 1] < static_cast<double>(thresh)) {
        w[j - 1] = 0.0;
        if (jc < 1) jc = j;
      }
    }
  }
  // :1405-1413.
  const double eps = 1.e-10;
  for (int j = 1; j <= n; ++j) {
    wti[j - 1] = 0.0;
    if (w[j - 1] > eps) {
      wti[j - 1] = 1.0 / (w[j - 1] * w[j - 1]);
    }
  }
  // :1415.
  dsvbksb(u, w, v, m, n, mm, n2, b, p);
  // :1417-1429. ssq and res are REAL: each update widens, operates in double, rounds back.
  ssq = 0.f;
  res = 0.f;
  for (int i = 1; i <= m; ++i) {
    double sum = 0.0;
    for (int j = 1; j <= n; ++j) {
      sum = sum + p[j - 1] * static_cast<double>(q[(i - 1) + (j - 1) * ldq]);
    }
    const double d = b[i - 1] - sum;
    const double resi = Kokkos::fabs(d);
    q[(i - 1) + (n2 - 1) * ldq] = static_cast<float>(d);
    // MAX(res,resi) is maxsd with the widened res as destination in the object.
    const double resd = static_cast<double>(res);
    res = static_cast<float>(resd > resi ? resd : resi);
    // gfortran squares the difference, not its ABS (same bits but for a NaN's sign).
    ssq = static_cast<float>(static_cast<double>(ssq) + d * d);
  }
  // :1432-1441. The object multiplies two k at a time but adds them to sum in k order,
  // so the scalar loop gives the same bits.
  for (int i = 1; i <= n; ++i) {
    for (int j = 1; j <= i; ++j) {
      double sum = 0.0;
      for (int k = 1; k <= n; ++k) {
        sum = sum + v[(i - 1) + (k - 1) * ldv] * v[(j - 1) + (k - 1) * ldv] * wti[k - 1];
      }
      cov[(i - 1) + (j - 1) * ldv] = sum;
      cov[(j - 1) + (i - 1) * ldv] = sum;
    }
  }
}

}  // namespace ww::tide
