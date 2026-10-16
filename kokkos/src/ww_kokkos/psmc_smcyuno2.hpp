// kokkos/src/ww_kokkos/psmc_smcyuno2.hpp
// SMCyUNO2: mid-flux values on the V-faces of a multi-resolution SMC grid, UNO2 scheme
// (advective mid-flux and diffusion flux). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3psmcmd.F90:1424-1534 at NOAA-EMC/WW3 03adda91;
// author Jian-Guo Li. Evidence: kokkos/port_report/SMCYUNO2_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::psmc {

/// One V-face j (1-based, the Fortran loop counter), :1445 and :1455-1526.
/// cf, vc: CF(-9:NCel), VC(-9:NCel) as cf[c+9], vc[c+9].
/// vfly, fv, fy: VFLY, FV, FY(NVFc) as v[j-1] (outputs).
/// ijkvfc: IJKVFc(7,NVFc) (w3gdatmd), contiguous column-major, IJKVFc(r,j) = ijkvfc[(r-1)+7*(j-1)].
/// clatf: CLATF(NVFc) (w3gdatmd) as clatf[j-1].
/// ijkcel4: IJKCel4(-9:NCel) (w3gdatmd) as ijkcel4[c+9].
KOKKOS_INLINE_FUNCTION void smcyuno2_point(int j, const Real* cf, const Real* vc, Real* vfly,
                                           Real akdif, Real* fv, Real* fy, Real fts,
                                           const int* ijkvfc, const Real* clatf,
                                           const int* ijkcel4) {
  const long c = 7L * static_cast<long>(j - 1);
  const Real zero = static_cast<Real>(0.0);
  const Real half = static_cast<Real>(0.5);
  const Real one = static_cast<Real>(1.0);
  const Real two = static_cast<Real>(2.0);

  // Loop invariant in the Fortran (:1445); the same bits on every face.
  const Real cnst0 = akdif * fts * fts * two;

  const int k = ijkvfc[c + 3];
  const int l = ijkvfc[c + 4];
  const int m = ijkvfc[c + 5];
  const int n = ijkvfc[c + 6];

  const Real cnst2 = static_cast<Real>(ijkcel4[l + 9]);
  const Real cnst3 = static_cast<Real>(ijkcel4[m + 9]);
  const Real cfm = cf[m + 9];
  const Real cfl = cf[l + 9];
  const Real cnst5 = (cfm - cfl) / (cnst2 + cnst3);

  const Real vcl = vc[l + 9];
  const Real vcm = vc[m + 9];
  const Real cnst6 = half * (vcl + vcm) * fts;

  const Real cnst8 = clatf[j - 1] * static_cast<Real>(ijkvfc[c + 2]);

  // The Fortran's CNST3 = CNST2 (:1486) and CNST2 = CNST3 (:1506) are dead stores: CNST5 is
  // already computed and neither is read again in its branch.
  Real f;
  if (cnst6 >= zero) {
    const Real v = m <= 0 ? vcl * fts : cnst6;
    vfly[j - 1] = v;
    const Real cnst1 = static_cast<Real>(ijkcel4[k + 9]);
    const Real cnst4 = (cfl - cf[k + 9]) / (cnst2 + cnst1);
    const Real a4 = Kokkos::fabs(cnst4);
    const Real a5 = Kokkos::fabs(cnst5);
    // MIN(|CNST4|,|CNST5|): minss keeps |CNST4| only when strictly less.
    const Real cnst = Kokkos::copysign(one, cnst5) * (a4 < a5 ? a4 : a5);
    f = (cfl + cnst * (cnst2 - v)) * cnst8;
  } else {
    const Real v = l <= 0 ? vcm * fts : cnst6;
    vfly[j - 1] = v;
    const Real cnst1 = static_cast<Real>(ijkcel4[n + 9]);
    const Real cnst4 = (cf[n + 9] - cfm) / (cnst1 + cnst3);
    const Real a4 = Kokkos::fabs(cnst4);
    const Real a5 = Kokkos::fabs(cnst5);
    const Real cnst = Kokkos::copysign(one, cnst5) * (a4 < a5 ? a4 : a5);
    f = (cfm - cnst * (cnst3 + v)) * cnst8;
  }
  fv[j - 1] = f;

  fy[j - 1] = cnst0 * cnst5 * cnst8;
}

/// SMCyUNO2 over j = NVA..NVB in the Fortran order (:1455). Arguments as in smcyuno2_point.
/// Under W3_OMPG the Fortran splits this loop over threads (:1449-1453); the faces are
/// independent, so a parallel_for over j gives the same bits.
KOKKOS_INLINE_FUNCTION void smcyuno2(int nva, int nvb, const Real* cf, const Real* vc, Real* vfly,
                                     Real akdif, Real* fv, Real* fy, Real fts, const int* ijkvfc,
                                     const Real* clatf, const int* ijkcel4) {
  for (int j = nva; j <= nvb; ++j) {
    smcyuno2_point(j, cf, vc, vfly, akdif, fv, fy, fts, ijkvfc, clatf, ijkcel4);
  }
}

}  // namespace ww::psmc
