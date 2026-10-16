// kokkos/src/ww_kokkos/fld1_sig2wn.hpp
// SIG2WN: wavenumber from radian frequency and depth by Newton iteration on the full
// dispersion relation, deep-water value when tanh(kh) >= 0.99. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3fld1md.F90:1174-1247 at NOAA-EMC/WW3 03adda91;
// authors B. G. Reichl, Newton iteration by X. Chen (2018).
// Evidence: kokkos/port_report/SIG2WN_KOKKOS.md.
// ⚠ On a device tanhf and coshf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::fld1 {

namespace sig2wn_detail {
inline constexpr Real kGrav = static_cast<Real>(9.806);  // GRAV, constants.F90:62
}  // namespace sig2wn_detail

/// SIG2WN(SIG,DEPTH,WN). The loop has no iteration cap, as in the Fortran (:1204):
/// SIG = 0 gives a 0/0 step, DEPTH = 0 an infinite step then tanh(inf*0); the NaN never
/// passes the test and the loop never ends.
KOKKOS_INLINE_FUNCTION void sig2wn(Real sig, Real depth, Real& wn) {
  using sig2wn_detail::kGrav;
  Real wn1 = (sig * sig) / kGrav;
  Real wn2;
  bool sw = true;
  if (Kokkos::tanh(wn1 * depth) < static_cast<Real>(0.99)) {
    while (sw) {
      const Real fk = kGrav * wn1 * Kokkos::tanh(wn1 * depth) - sig * sig;
      const Real c = Kokkos::cosh(wn1 * depth);
      const Real fk_slp = kGrav * Kokkos::tanh(wn1 * depth) + kGrav * wn1 * depth / (c * c);
      wn2 = wn1 - fk / fk_slp;
      if (Kokkos::fabs(wn2 - wn1) / wn1 < static_cast<Real>(0.0001)) {
        sw = false;
      } else {
        wn1 = wn2;
      }
    }
  } else {
    wn2 = wn1;
  }
  wn = wn2;
}

}  // namespace ww::fld1
