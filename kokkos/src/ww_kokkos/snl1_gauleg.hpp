// kokkos/src/ww_kokkos/snl1_gauleg.hpp
// GAULEG: abscissas and weights of the NPOIN-point Gauss-Legendre rule on [-1, 1], by
// Newton iteration on the Legendre recurrence; a table INSNLGQM builds once (GQM, NL1).
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3snl1md.F90:1310-1346 at NOAA-EMC/WW3 03adda91, adapted
// from TOMAWAC (M. Benoit, E. Gagnaire-Renou). Evidence: kokkos/port_report/GAULEG_KOKKOS.md.
// Host only: std::cos is glibc's cos, which the object calls (no libmvec variant).
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cmath>

namespace ww::snl1 {

namespace gauleg_detail {
/// PI = ACOS(-1.) (:1324) is a default REAL ACOS widened to DOUBLE PRECISION: gfortran folds
/// it to the float nearest pi, 3.1415927410125732421875, not to the double pi.
inline constexpr double kPi = 0x1.921fb6p+1;
static_assert(kPi == static_cast<double>(static_cast<float>(3.14159265358979323846)));
/// EPS = 3.D-14 (:1322).
inline constexpr double kEps = 3.e-14;
}  // namespace gauleg_detail

/// GAULEG, arrays 0-based (Fortran W_LEG(1:NPOIN), X_LEG(1:NPOIN)). Writes the first and last
/// (NPOIN+1)/2 entries of each; for NPOIN < 1 writes nothing.
/// ⚠ The Newton loop (:1328-1340) has no iteration limit, as in the Fortran.
inline void gauleg(double* w_leg, double* x_leg, int npoin) {
  using gauleg_detail::kEps;
  using gauleg_detail::kPi;
  const int m = (npoin + 1) / 2;
  for (int i = 1; i <= m; ++i) {
    // :1327.
    double z = std::cos(kPi * (static_cast<double>(i) - 0.25) /
                        (static_cast<double>(npoin) + 0.5));
    double p1;
    double p2;
    double pp;
    for (;;) {
      p1 = 1.0;
      p2 = 0.0;
      for (int j = 1; j <= npoin; ++j) {
        const double p3 = p2;
        p2 = p1;
        const double dj = static_cast<double>(j);
        // :1334.
        p1 = ((2.0 * dj - 1.0) * z * p2 - (dj - 1.0) * p3) / dj;
      }
      // :1336.
      pp = static_cast<double>(npoin) * (z * p1 - p2) / (z * z - 1.0);
      const double z1 = z;
      z = z - p1 / pp;
      if (std::fabs(z - z1) <= kEps) break;
    }
    // :1341-1344. Z**2 and PP**2 are z*z and pp*pp in the object.
    x_leg[i - 1] = -z;
    x_leg[npoin - i] = z;
    w_leg[i - 1] = 2.0 / ((1.0 - z * z) * (pp * pp));
    w_leg[npoin - i] = w_leg[i - 1];
  }
}

}  // namespace ww::snl1
