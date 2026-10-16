// kokkos/src/ww_kokkos/srce_calc_fpi.hpp
// CALC_FPI: equivalent peak frequency from the wind input source term (Tolman and
// Chalikov 1996), one spectral point. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3srcemd.F90:2428-2545 at NOAA-EMC/WW3 03adda91;
// author J. Meixner. Evidence: kokkos/port_report/CALC_FPI_KOKKOS.md.
// Called only under FLD1 or FLD2 (w3srcemd.F90:2278-2280).
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::srce {

/// CALC_FPI for one point, arrays 0-based. a (A, 1:NSPEC) is not read, as in the
/// Fortran; cg = CG(1:NK); s = S(1:NSPEC). Grid arrays of W3GDATMD from their first
/// element used: sig = SIG(1:NK) (SIG is (0:NK+1), so pass &SIG(1)), dden = DDEN(1:NK).
/// sin1a (NK) is work, the Fortran's automatic SIN1A.
KOKKOS_INLINE_FUNCTION void calc_fpi(const Real* a, const Real* cg, Real& fpi, const Real* s,
                                     int nk, int nth, Real xfr, const Real* sig,
                                     const Real* dden, Real fte, Real fttr, Real* sin1a) {
  const Real zero = static_cast<Real>(0.);

  // :2521-2526. MAX(0., S(IS)) is maxss with S(IS) as destination (S > 0 ? S : 0).
  for (int ik = 1; ik <= nk; ++ik) {
    Real acc = zero;
    for (int is = (ik - 1) * nth + 1; is <= ik * nth; ++is) {
      const Real v = s[is - 1];
      acc = acc + (v > zero ? v : zero);
    }
    sin1a[ik - 1] = acc;
  }

  // :2528-2534. SIG(IK)**3 is (SIG*SIG)*SIG in the object.
  Real m0 = zero;
  Real m1 = zero;
  for (int ik = 1; ik <= nk; ++ik) {
    const Real sg = sig[ik - 1];
    sin1a[ik - 1] = sin1a[ik - 1] * dden[ik - 1] / (cg[ik - 1] * (sg * sg * sg));
    m0 = m0 + sin1a[ik - 1];
    m1 = m1 + sin1a[ik - 1] / sg;
  }

  // :2536-2543.
  sin1a[nk - 1] = sin1a[nk - 1] / dden[nk - 1];
  m0 = m0 + sin1a[nk - 1] * fte;
  m1 = m1 + sin1a[nk - 1] * fttr;
  if (m1 < 1E-20f) {
    fpi = xfr * sig[nk - 1];
  } else {
    fpi = m0 / m1;
  }
}

}  // namespace ww::srce
