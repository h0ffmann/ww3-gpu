// kokkos/src/ww_kokkos/psmc_smcyuno2r.hpp
// SMCyUNO2r: mid-flux values on the V-faces of a single-resolution SMC grid, UNO2 scheme
// (advective mid-flux and diffusion flux). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3psmcmd.F90:1661-1746 at NOAA-EMC/WW3 03adda91;
// author Jian-Guo Li. Evidence: kokkos/port_report/SMCYUNO2R_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::psmc {

/// One V-face j (1-based, the Fortran loop counter), :1687-1736.
/// cf, vc: CF(-9:NCel), VC(-9:NCel) as cf[c+9], vc[c+9].
/// vfly, fv, fy: VFLY, FV, FY(NVFc) as v[j-1] (outputs).
/// ijkvfc: IJKVFc(7,NVFc) (w3gdatmd), contiguous column-major, IJKVFc(r,j) = ijkvfc[(r-1)+7*(j-1)].
/// clatf: CLATF(NVFc) (w3gdatmd) as clatf[j-1].
KOKKOS_INLINE_FUNCTION void smcyuno2r_point(int j, const Real* cf, const Real* vc, Real* vfly,
                                            Real akdif, Real* fv, Real* fy, const int* ijkvfc,
                                            const Real* clatf) {
  const long c = 7L * static_cast<long>(j - 1);
  const Real zero = static_cast<Real>(0.0);
  const Real half = static_cast<Real>(0.5);
  const Real one = static_cast<Real>(1.0);

  const int l = ijkvfc[c + 4];
  const int m = ijkvfc[c + 5];

  const Real cfm = cf[m + 9];
  const Real cfl = cf[l + 9];
  const Real cnst5 = cfm - cfl;

  const Real vcl = vc[l + 9];
  const Real vcm = vc[m + 9];
  const Real cnst6 = half * (vcl + vcm);

  const Real cnst8 = clatf[j - 1] * static_cast<Real>(ijkvfc[c + 2]);

  Real f;
  if (cnst6 >= zero) {
    const int k = ijkvfc[c + 3];
    const Real v = m <= 0 ? vcl : cnst6;
    vfly[j - 1] = v;
    const Real cnst4 = cfl - cf[k + 9];
    const Real a4 = Kokkos::fabs(cnst4);
    const Real a5 = Kokkos::fabs(cnst5);
    // MIN(|CNST4|,|CNST5|): minss keeps |CNST4| only when strictly less.
    const Real cnst = Kokkos::copysign(half, cnst5) * (a4 < a5 ? a4 : a5);
    f = (cfl + cnst * (one - v)) * cnst8;
  } else {
    const int n = ijkvfc[c + 6];
    const Real v = l <= 0 ? vcm : cnst6;
    vfly[j - 1] = v;
    const Real cnst4 = cf[n + 9] - cfm;
    const Real a4 = Kokkos::fabs(cnst4);
    const Real a5 = Kokkos::fabs(cnst5);
    const Real cnst = Kokkos::copysign(half, cnst5) * (a4 < a5 ? a4 : a5);
    f = (cfm - cnst * (one + v)) * cnst8;
  }
  fv[j - 1] = f;

  fy[j - 1] = akdif * cnst5 * cnst8;
}

/// SMCyUNO2r over j = NVA..NVB in the Fortran order (:1684). Arguments as in smcyuno2r_point.
KOKKOS_INLINE_FUNCTION void smcyuno2r(int nva, int nvb, const Real* cf, const Real* vc, Real* vfly,
                                      Real akdif, Real* fv, Real* fy, const int* ijkvfc,
                                      const Real* clatf) {
  for (int j = nva; j <= nvb; ++j) {
    smcyuno2r_point(j, cf, vc, vfly, akdif, fv, fy, ijkvfc, clatf);
  }
}

}  // namespace ww::psmc
