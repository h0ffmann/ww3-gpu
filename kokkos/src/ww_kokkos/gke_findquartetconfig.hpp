// kokkos/src/ww_kokkos/gke_findquartetconfig.hpp
// FindQuartetConfig of W3GKEMD: the list of quasi-resonant quartets (k1, k2, k3, k4) of the
// generalised kinetic equation (GKE) on the (k, theta) grid, with k4 and omega4 per quartet.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3gkemd.F90:732-869 at NOAA-EMC/WW3 03adda91;
// authors O. Gramstad, Q. Liu. Evidence: kokkos/port_report/FINDQUARTETCONFIG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <Kokkos_Core.hpp>

#include "gke_qfunc.hpp"
#include "real.hpp"

namespace ww::gke {

namespace findquartetconfig_detail {
/// qr_grav, :198: 9.806, 0x411ce560 in the object (module constant, not constants.F90's GRAV).
inline constexpr Real kQrGrav = static_cast<Real>(9.806);
}  // namespace findquartetconfig_detail

/// FindQuartetConfig(ns, kx, ky, om, oml, nnz, NN, PP, QQ, RR, k4x, k4y, om4), :732-869.
/// Called once, by PrepKernelIO (:1285-1287), out of line in the object; runs on one thread.
/// kx, ky, om [ns] (base 1). Outputs nn, pp, qq, rr, k4x, k4y, om4 [nnz] (base 1); the indices
/// are written as Fortran (1-based) positions in the ns vector.
/// qr_depth (:165) and qi_interp (:180) are the module variables the Fortran reads; qi_bound is
/// a PARAMETER fixed to 1 (:184), so only qi_interp is tested at :794.
/// k [ns] is the caller-provided work array for the Fortran's automatic array k(ns) (:784,
/// malloc'd by the object).
/// Returns 0, or 1 where the Fortran writes format 1001 and calls exit(1) (:860-863: the count
/// s differs from nnz); the shim does the WRITE and the exit.
/// ⚠ As in the Fortran, s is not bounded by nnz while storing (:840-847): a caller whose nnz
/// is smaller than the count writes past the end of the outputs before the error is seen.
KOKKOS_INLINE_FUNCTION int findquartetconfig(int ns, const Real* kx, const Real* ky,
                                             const Real* om, Real oml, std::int64_t nnz, int* nn,
                                             int* pp, int* qq, int* rr, Real* k4x, Real* k4y,
                                             Real* om4, Real qr_depth, int qi_interp, Real* k) {
  using findquartetconfig_detail::kQrGrav;
  constexpr Real kInf = Kokkos::Experimental::infinity_v<Real>;
  int s = 0;  // :806
  // With ns <= 0 the object skips :789-796 and every loop and goes to :860 with s = 0.
  if (ns > 0) {
    // :789.
    for (int i = 0; i < ns; ++i) k[i] = Kokkos::sqrt(kx[i] * kx[i] + ky[i] * ky[i]);
    // MINVAL (:790) and MAXVAL (:791) as gfortran expands them: the first element that is not
    // NaN starts the scan, then a strict compare keeps the earlier value on ties and NaN; all NaN
    // gives NaN.
    Real kmin = Kokkos::Experimental::quiet_NaN_v<Real>;
    for (int i = 0; i < ns; ++i) {
      if (k[i] <= kInf) {
        kmin = k[i];
        for (int j = i + 1; j < ns; ++j) kmin = (k[j] < kmin) ? k[j] : kmin;
        break;
      }
    }
    Real kmax = Kokkos::Experimental::quiet_NaN_v<Real>;
    for (int i = 0; i < ns; ++i) {
      if (k[i] >= -kInf) {
        kmax = k[i];
        for (int j = i + 1; j < ns; ++j) kmax = (k[j] > kmax) ? k[j] : kmax;
        break;
      }
    }
    // :794-797. Division by 9, not multiplication by its reciprocal (divss in the object).
    if (qi_interp == 1) {
      kmin = kmin / static_cast<Real>(9.);
      kmax = kmax * static_cast<Real>(9.);
    }

    for (int i1 = 1; i1 <= ns; ++i1) {        // :808
      for (int i2 = i1; i2 <= ns; ++i2) {     // :810
        for (int i3 = 1; i3 <= ns; ++i3) {    // :811
          if (i3 != i1 && i3 != i2) {         // :813
            const Real k4xT = kx[i1 - 1] + kx[i2 - 1] - kx[i3 - 1];  // :815
            const Real k4yT = ky[i1 - 1] + ky[i2 - 1] - ky[i3 - 1];  // :816
            const Real k4T = Kokkos::sqrt(k4xT * k4xT + k4yT * k4yT);  // :817
            if (k4T >= kmin && k4T <= kmax) {  // :820, both ordered compares
              const Real om4T = Kokkos::sqrt(kQrGrav * qfunc(k4xT, k4yT, qr_depth));  // :822
              // :824-825. MIN(om1, om2, om3, om4T) as the object pairs it (minss, the first
              // operand kept unless the second is strictly smaller):
              // min(min(om3, om4T), min(om1, om2)).
              const Real o1 = om[i1 - 1];
              const Real o2 = om[i2 - 1];
              const Real o3 = om[i3 - 1];
              const Real m12 = (o1 < o2) ? o1 : o2;
              const Real m34 = (o3 < om4T) ? o3 : om4T;
              const Real mn = (m34 < m12) ? m34 : m12;
              const Real dom = Kokkos::fabs(o1 + o2 - o3 - om4T) / mn;
              if (oml <= kQrEps || dom <= oml) {  // :826
                // MINLOC(..., 1), :828-829, as gfortran expands it: the first element that is
                // not NaN starts the scan, a strict compare keeps the first minimum; all NaN
                // gives 1.
                int i4 = 1;
                for (int j = 1; j <= ns; ++j) {
                  const Real dj = (kx[j - 1] - k4xT) * (kx[j - 1] - k4xT) +
                                  (ky[j - 1] - k4yT) * (ky[j - 1] - k4yT);
                  if (dj <= kInf) {
                    i4 = j;
                    Real lim = dj;
                    for (int jj = j + 1; jj <= ns; ++jj) {
                      const Real d = (kx[jj - 1] - k4xT) * (kx[jj - 1] - k4xT) +
                                     (ky[jj - 1] - k4yT) * (ky[jj - 1] - k4yT);
                      if (d < lim) {
                        i4 = jj;
                        lim = d;
                      }
                    }
                    break;
                  }
                }
                if (i4 != i1 && i4 != i2) {  // :830
                  if (i4 >= i3) {            // :832
                    const int row = i1 + ns * (i2 - 1);  // :833-834
                    const int col = i3 + ns * (i4 - 1);
                    if (col > row) {  // :836
                      s = s + 1;      // :838
                      nn[s - 1] = i1;  // :840-847
                      pp[s - 1] = i2;
                      qq[s - 1] = i3;
                      rr[s - 1] = i4;
                      k4x[s - 1] = k4xT;
                      k4y[s - 1] = k4yT;
                      om4[s - 1] = om4T;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  // :860-863, s widened to INTEGER(8) for the compare.
  if (static_cast<std::int64_t>(s) != nnz) return 1;
  return 0;
}

}  // namespace ww::gke
