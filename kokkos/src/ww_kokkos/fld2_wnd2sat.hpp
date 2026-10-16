// kokkos/src/ww_kokkos/fld2_wnd2sat.hpp
// WND2SAT of W3FLD2MD: saturation level of the 1-D spectral tail from the 10 m neutral wind,
// a piecewise polynomial fitted to the HWRF 2016 Cd curve, clipped to [0.002, 0.014].
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3fld2md.F90:592-687 at NOAA-EMC/WW3 03adda91;
// authors B. G. Reichl, J. Meixner (2016 curve).
// Evidence: kokkos/port_report/W3FLD2MD_WND2SAT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::fld2 {

/// WND2SAT(WND10,SAT). The coefficients are default-REAL literals, rounded once to float.
/// A NaN WND10 fails both tests and takes the linear branch (:683), as comiss does.
KOKKOS_INLINE_FUNCTION void wnd2sat(Real wnd10, Real& sat) {
  Real s;
  if (wnd10 < static_cast<Real>(20.)) {
    // :672-674. gfortran folds -A*X + B*Y into B*Y - A*X (exact); this is its order.
    s = 0.960758623686446e-3F * wnd10 - 0.022919753482426e-3F * (wnd10 * wnd10) -
        0.084461041915030e-3F;
  } else if (wnd10 < static_cast<Real>(45.)) {
    // :676-680. The object builds W2 = W*W, W**3 = W*W2, W**4 = W2*W2.
    const Real w2 = wnd10 * wnd10;
    const Real w3 = wnd10 * w2;
    const Real w4 = w2 * w2;
    s = 0.000001058147954F * w3 - 0.000000006585745F * w4 - 0.000065829151883F * w2 +
        0.001587028483595F * wnd10 - 0.002857112191889F;
  } else {
    // :682-683.
    s = 0.012706067280674F - 0.000178498197241F * wnd10;
  }
  // :685, maxss 0.002,s then minss 0.014,m: a NaN s gives 0.002.
  const Real m = (s > 0.002F) ? s : 0.002F;
  sat = (m < 0.014F) ? m : 0.014F;
}

}  // namespace ww::fld2
