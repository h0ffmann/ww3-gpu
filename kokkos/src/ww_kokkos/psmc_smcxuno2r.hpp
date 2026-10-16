// kokkos/src/ww_kokkos/psmc_smcxuno2r.hpp
// SMCxUNO2r: mid-flux values on the U-faces of a single-resolution SMC grid, UNO2 scheme
// (advective mid-flux and diffusion flux). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3psmcmd.F90:1551-1644 at NOAA-EMC/WW3 03adda91;
// author Jian-Guo Li. Evidence: kokkos/port_report/SMCXUNO2R_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::psmc {

/// One U-face i (1-based, the Fortran loop counter), :1579-1636.
/// cf, uc: CF(-9:NCel), UC(-9:NCel) as cf[c+9], uc[c+9].
/// uflx, fu, fx: UFLX, FU, FX(NUFc) as v[i-1] (outputs).
/// ijkufc: IJKUFc(7,NUFc) (w3gdatmd), contiguous column-major, IJKUFc(r,i) = ijkufc[(r-1)+7*(i-1)].
/// ijkcel3: IJKCel3(-9:MCel) (w3gdatmd) as ijkcel3[c+9].
/// clats: CLATS(0:MSEA) (w3gdatmd) as clats[s].
KOKKOS_INLINE_FUNCTION void smcxuno2r_point(int i, const Real* cf, const Real* uc, Real* uflx,
                                            Real akdif, Real* fu, Real* fx, const int* ijkufc,
                                            const int* ijkcel3, const Real* clats) {
  const long c = 7L * static_cast<long>(i - 1);
  const Real zero = static_cast<Real>(0.0);
  const Real half = static_cast<Real>(0.5);
  const Real one = static_cast<Real>(1.0);
  const Real two = static_cast<Real>(2.0);

  const int l = ijkufc[c + 4];
  const int m = ijkufc[c + 5];

  const Real cnst2 = static_cast<Real>(ijkcel3[l + 9]);
  const Real cnst3 = static_cast<Real>(ijkcel3[m + 9]);
  const Real cfm = cf[m + 9];
  const Real cfl = cf[l + 9];
  const Real cnst5 = cfm - cfl;

  const Real ucl = uc[l + 9];
  const Real ucm = uc[m + 9];
  const Real cnst6 = half * (ucl + ucm);

  // :1598, CLATS is indexed by the larger cell number of the face.
  const int ij = l > m ? l : m;
  const Real cl = clats[ij];
  const Real cnst0 = two / (cl * cl);

  Real f;
  if (cnst6 >= zero) {
    const int k = ijkufc[c + 3];
    const Real u = m <= 0 ? ucl : cnst6;
    uflx[i - 1] = u;
    const Real cnst4 = cfl - cf[k + 9];
    const Real a4 = Kokkos::fabs(cnst4);
    const Real a5 = Kokkos::fabs(cnst5);
    // MIN(|CNST4|,|CNST5|): minss keeps |CNST4| only when strictly less.
    const Real cnst = Kokkos::copysign(half, cnst5) * (a4 < a5 ? a4 : a5);
    f = cfl + cnst * (one - u / cnst2);
  } else {
    const int n = ijkufc[c + 6];
    const Real u = l <= 0 ? ucm : cnst6;
    uflx[i - 1] = u;
    const Real cnst4 = cf[n + 9] - cfm;
    const Real a4 = Kokkos::fabs(cnst4);
    const Real a5 = Kokkos::fabs(cnst5);
    const Real cnst = Kokkos::copysign(half, cnst5) * (a4 < a5 ? a4 : a5);
    f = cfm - cnst * (one + u / cnst3);
  }
  fu[i - 1] = f;

  fx[i - 1] = akdif * cnst0 * cnst5 / (cnst2 + cnst3);
}

/// SMCxUNO2r over i = NUA..NUB in the Fortran order (:1579). Arguments as in smcxuno2r_point.
KOKKOS_INLINE_FUNCTION void smcxuno2r(int nua, int nub, const Real* cf, const Real* uc, Real* uflx,
                                      Real akdif, Real* fu, Real* fx, const int* ijkufc,
                                      const int* ijkcel3, const Real* clats) {
  for (int i = nua; i <= nub; ++i) {
    smcxuno2r_point(i, cf, uc, uflx, akdif, fu, fx, ijkufc, ijkcel3, clats);
  }
}

}  // namespace ww::psmc
