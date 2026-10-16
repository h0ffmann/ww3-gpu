// kokkos/src/ww_kokkos/ic3_smooth_k.hpp
// SMOOTH_K: replaces the real and imaginary ice wavenumbers at the mode-switch points of the IC3
// dispersion relation by linear interpolation in sigma. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic3md.F90:2497-2535 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, S. Zieger, S. Cheng. Evidence: kokkos/port_report/SMOOTH_K_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::ic3 {

/// SMOOTH_K. wn_r, wn_i, sigma: Fortran (1:N), here 0-based. diff and removeid (N each) are
/// the Fortran's automatic arrays DIFF, REMOVEID (:2499), provided by the caller. switchid is
/// a 1-based frequency index (0 = none). The Fortran reads out of bounds for n < 3 (:2511,
/// :2527-2532); the caller must pass n >= 3.
KOKKOS_INLINE_FUNCTION void smooth_k(Real* wn_r, Real* wn_i, const Real* sigma, int n,
                                     int switchid, Real* diff, Real* removeid) {
  const Real zero = static_cast<Real>(0);
  const Real one = static_cast<Real>(1);
  const Real three = static_cast<Real>(3);
  // i is the Fortran's 1-based index throughout; X(i) is x[i - 1].
  // :2502. DIFF(1) is set only here.
  for (int i = 1; i <= n; ++i) diff[i - 1] = zero;
  for (int j = 1; j <= 3; ++j) {
    for (int i = 1; i <= n; ++i) removeid[i - 1] = zero;
    for (int i = 2; i <= n; ++i) diff[i - 1] = wn_r[i - 1] - wn_r[i - 2];
    // :2510-2515. A NaN DIFF fails both comparisons, as in the object.
    for (int i = 3; i <= n; ++i) {
      if (diff[i - 1] <= zero || diff[i - 1] > diff[i - 2] * three || switchid == i) {
        removeid[i - 1] = one;
        removeid[i - 2] = one;
      }
    }
    // :2517-2524. Sequential: WN_R(I-1) may already be the interpolated value.
    for (int i = 2; i <= n - 1; ++i) {
      if (removeid[i - 1] == one) {
        wn_r[i - 1] = wn_r[i - 2] + (wn_r[i] - wn_r[i - 2]) /
            (sigma[i] - sigma[i - 2]) * (sigma[i - 1] - sigma[i - 2]);
        wn_i[i - 1] = wn_i[i - 2] + (wn_i[i] - wn_i[i - 2]) /
            (sigma[i] - sigma[i - 2]) * (sigma[i - 1] - sigma[i - 2]);
      }
    }
  }
  // :2527-2533. DIFF is the one of the third pass, computed before its interpolation.
  if (diff[n - 1] > diff[n - 2] * three) {
    const int i = n;
    wn_r[i - 1] = wn_r[i - 2] + (wn_r[i - 2] - wn_r[i - 3]) /
        (sigma[i - 2] - sigma[i - 3]) * (sigma[i - 1] - sigma[i - 2]);
    wn_i[i - 1] = wn_i[i - 2] + (wn_i[i - 2] - wn_i[i - 3]) /
        (sigma[i - 2] - sigma[i - 3]) * (sigma[i - 1] - sigma[i - 2]);
  }
}

}  // namespace ww::ic3
