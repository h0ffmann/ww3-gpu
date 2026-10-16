// kokkos/src/ww_kokkos/fld1_wnd2sat.hpp
// WND2SAT of W3FLD1MD: saturation level of the 1-D spectral tail from the 10 m neutral wind,
// a piecewise polynomial fitted to the HWRF 2016 Cd curve, clipped to [0.002, 0.014].
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3fld1md.F90:1377-1473 at NOAA-EMC/WW3 03adda91;
// authors B. G. Reichl, J. Meixner (2016 curve).
// Evidence: kokkos/port_report/W3FLD1MD_WND2SAT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::fld1 {

/// WND2SAT(WND10,SAT). The coefficients are default-REAL literals, rounded once to float.
/// A NaN WND10 fails both tests and takes the linear branch (:1469), as comiss does.
KOKKOS_INLINE_FUNCTION void wnd2sat(Real wnd10, Real& sat) {
  Real s;
  if (wnd10 < static_cast<Real>(20.)) {
    // :1458-1460. WND10**2 is WND10*WND10; gfortran's -A*X2 + B*X is B*X - A*X2 (exact).
    s = 0.000751515452434F * wnd10 - 0.000018541921682F * (wnd10 * wnd10) + 0.002466529381004F;
  } else if (wnd10 < static_cast<Real>(45.)) {
    // :1462-1466. The object builds W2 = W*W, W**3 = W*W2, W**4 = W2*W2.
    const Real w2 = wnd10 * wnd10;
    const Real w3 = wnd10 * w2;
    const Real w4 = w2 * w2;
    s = 0.000001276678367F * w3 - 0.000000009060349F * w4 - 0.000068274393789F * w2 +
        0.001418180888868F * wnd10 + 0.000262277682984F;
  } else {
    // :1468-1469, as gfortran orders it: C - A*W.
    s = 0.012027763023184F - 0.000155976275073F * wnd10;
  }
  // :1472, maxss 0.002,s then minss 0.014,m: a NaN s gives 0.002.
  const Real m = (s > 0.002F) ? s : 0.002F;
  sat = (m < 0.014F) ? m : 0.014F;
}

}  // namespace ww::fld1
