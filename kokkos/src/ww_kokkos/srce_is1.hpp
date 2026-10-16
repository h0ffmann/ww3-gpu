// kokkos/src/ww_kokkos/srce_is1.hpp
// W3SIS1: scattering by sea ice, a linear transfer of action from each direction to
// the other NTH-1 of the same frequency, one spectral point. Phase 1, translated, not
// regtested. WW3 heritage: WW3-noaa/model/src/w3sis1md.F90:74-227 at NOAA-EMC/WW3
// 03adda91; author S. Zieger. Evidence: kokkos/port_report/W3SIS1_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"

namespace ww::srce {

/// SICPS%IS1C1, IS1C2 of the grid (w3gdatmd.F90:2845-2846).
struct Is1Params {
  Real is1c1;  ///< IS1C1, scale factor for the ice concentration
  Real is1c2;  ///< IS1C2, offset
};

/// W3SIS1(A, ICE, S) for one point, arrays 0-based; reads p.nk, nth, nspec, dtmin.
KOKKOS_INLINE_FUNCTION void w3sis1_point(const Params& p, const Is1Params& is1,
                                         const Real* a, Real ice, Real* s) {
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  const Real zero = static_cast<Real>(0.);

  // :179.
  for (int is = 0; is < nspec; ++is) s[is] = zero;

  // :185. maxss operand order from w3sis1md.o: (v > 0) ? v : 0.
  const Real v = is1.is1c1 * ice + is1.is1c2;
  const Real alpha = (v > zero) ? v : zero;

  // :190-220.
  if (alpha > zero && ice > zero) {
    for (int ik = 1; ik <= nk; ++ik) {
      for (int ith = 1; ith <= nth; ++ith) {
        const int is = ith + (ik - 1) * nth;
        if (a[is - 1] >= zero) {
          const Real aa = a[is - 1] * alpha;
          s[is - 1] = s[is - 1] - aa;
          for (int ith2 = 1; ith2 <= nth; ++ith2) {
            const int is2 = ith2 + (ik - 1) * nth;
            if (is2 != is) {
              s[is2 - 1] = s[is2 - 1] + aa / static_cast<Real>(nth - 1);
            }
          }
        }
      }
    }
    // :207.
    for (int is = 0; is < nspec; ++is) s[is] = s[is] / p.dtmin;
  }
}

}  // namespace ww::srce
