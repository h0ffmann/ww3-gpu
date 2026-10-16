// kokkos/src/ww_kokkos/ic3_cginic3_cheng.hpp
// CGINIC3_CHENG: group velocity of the IC3 ice-covered dispersion relation by finite differences
// of sigma over k_r, a harmonic mean of the two one-sided slopes inside. Phase 1, translated, not
// regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic3md.F90:2420-2493 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, S. Zieger, X. Zhao, S. Cheng. Evidence: kokkos/port_report/CGINIC3_CHENG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::ic3 {

/// CGINIC3_CHENG. cg, sigma, wn_r: Fortran (1:N), here 0-based. The unused automatic array
/// CG0 (:2479) is dropped by the object and here. The Fortran reads SIGMA(2), WN_R(2) for
/// n == 1 (:2483); the caller must pass n >= 2.
KOKKOS_INLINE_FUNCTION void cginic3_cheng(Real* cg, const Real* sigma, const Real* wn_r, int n) {
  const Real one = static_cast<Real>(1.);
  const Real two = static_cast<Real>(2.);
  // ik is the Fortran's 1-based index; X(ik) is x[ik - 1].
  int ik = 1;
  // :2483.
  cg[ik - 1] = (sigma[ik] - sigma[ik - 1]) / (wn_r[ik] - wn_r[ik - 1]);
  ik = n;
  // :2485.
  cg[ik - 1] = (sigma[ik - 1] - sigma[ik - 2]) / (wn_r[ik - 1] - wn_r[ik - 2]);
  for (ik = 2; ik <= n - 1; ++ik) {
    // :2487-2489. The object adds 1./CG2 + 1./CG1 (addss destination 1./CG2). g++ keeps 1./cg2
    // first except at the first scalar epilogue step after the vector loop, where 1./cg1 is
    // first: only the NaN out of two NaNs of different payload or sign can differ there.
    const Real cg1 = (sigma[ik - 1] - sigma[ik - 2]) / (wn_r[ik - 1] - wn_r[ik - 2]);
    const Real cg2 = (sigma[ik] - sigma[ik - 1]) / (wn_r[ik] - wn_r[ik - 1]);
    cg[ik - 1] = two / (one / cg2 + one / cg1);
  }
}

}  // namespace ww::ic3
