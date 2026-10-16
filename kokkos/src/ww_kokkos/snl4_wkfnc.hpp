// kokkos/src/ww_kokkos/snl4_wkfnc.hpp
// wkfnc of W3SNL4MD: wavenumber k (rad/m) from frequency and depth by a Pade-type inversion of
// sigma^2 = g k tanh(kd). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3snl4md.F90:5286-5369 at NOAA-EMC/WW3 03adda91;
// authors B. Toulany, M. Casey, W. Perrie. Evidence: kokkos/port_report/WKFNC_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::snl4 {

/// wkfnc(f, dep), :5286-5369: f in Hz, dep in m, k in rad/m.
/// twopi is the module variable of w3snl4md (:142), set to TPI in W3SNL4 (:872).
/// g, y, x are REAL(KIND=8) (:5343); every literal is a default REAL widened to double, so each
/// constant below is the float literal cast to double, as the object loads it (g = 9.806f).
/// dep = 0 gives 0/0 as the Fortran does; no guard.
KOKKOS_INLINE_FUNCTION Real wkfnc(Real f, Real dep, Real twopi) {
  const double g = static_cast<double>(9.806f);  // :5350
  // :5352. (twopi*f)**2 * dep in REAL, squared as one multiply, then widened and divided in double.
  const Real tf = f * twopi;
  const double y = static_cast<double>(tf * tf * dep) / g;
  // :5355-5358, innermost first, in double.
  double p = y * static_cast<double>(0.00011f) + static_cast<double>(0.00039f);
  p = p * y + static_cast<double>(0.00171f);
  p = p * y + static_cast<double>(0.00654f);
  p = p * y + static_cast<double>(0.02174f);
  p = p * y + static_cast<double>(0.06320f);
  p = p * y + static_cast<double>(0.16084f);
  p = p * y + static_cast<double>(0.35550f);
  p = p * y + static_cast<double>(0.66667f);
  p = p * y + 1.0;
  double x = (1.0 / p + y) * y;
  // :5361. sqrtsd in the object.
  x = Kokkos::sqrt(x);
  // :5363. x / dep in double, rounded to REAL.
  return static_cast<Real>(x / static_cast<double>(dep));
}

}  // namespace ww::snl4
