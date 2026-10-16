// kokkos/src/ww_kokkos/psmc_smcxuno3.hpp
// SMCxUNO3: mid-flux values on the U-faces of a multi-resolution SMC grid, UNO3 scheme
// (advective mid-flux and diffusion flux). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3psmcmd.F90:1764-1900 at NOAA-EMC/WW3 03adda91;
// author Jian-Guo Li. Evidence: kokkos/port_report/SMCXUNO3_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::psmc {

/// One U-face i (1-based, the Fortran loop counter), :1799-1892.
/// cf, uc: CF(-9:NCel), UC(-9:NCel) as cf[c+9], uc[c+9].
/// uflx, fu, fx: UFLX, FU, FX(NUFc) as v[i-1] (outputs).
/// cnst0: AKDif*FTS*FTS*2.0 of :1787, from smcxuno3_cnst0.
/// ijkufc: IJKUFc(7,NUFc) (w3gdatmd), contiguous column-major, IJKUFc(r,i) = ijkufc[(r-1)+7*(i-1)].
/// ijkcel3: IJKCel3(-9:MCel) (w3gdatmd) as ijkcel3[c+9].
/// clats: CLATS(0:MSEA) (w3gdatmd) as clats[s].
// Commutative operands are written in the order the Fortran object puts them (first source
// first), so that a sum or product of two NaNs keeps the Fortran's payload.
KOKKOS_INLINE_FUNCTION void smcxuno3_point(int i, const Real* cf, const Real* uc, Real* uflx,
                                           Real cnst0, Real* fu, Real* fx, Real fts,
                                           const int* ijkufc, const int* ijkcel3,
                                           const Real* clats) {
  const long c = 7L * static_cast<long>(i - 1);
  const Real zero = static_cast<Real>(0.0);
  const Real half = static_cast<Real>(0.5);
  const Real one = static_cast<Real>(1.0);
  const Real two = static_cast<Real>(2.0);
  const Real p6 = static_cast<Real>(0.6);
  const Real onep5 = static_cast<Real>(1.5);

  const int l = ijkufc[c + 4];
  const int m = ijkufc[c + 5];

  const Real cnst2 = static_cast<Real>(ijkcel3[l + 9]);
  const Real cnst3 = static_cast<Real>(ijkcel3[m + 9]);
  const Real cfm = cf[m + 9];
  const Real cfl = cf[l + 9];
  const Real cnst23 = cnst2 + cnst3;
  const Real cnst5 = (cfm - cfl) / cnst23;

  const Real ucl = uc[l + 9];
  const Real ucm = uc[m + 9];
  const Real cnst6 = half * (ucl + ucm) * fts;

  const Real cnst8 = static_cast<Real>(ijkufc[c + 2]);

  // :1823, CLATS is indexed by the larger cell number of the face.
  const int ij = l > m ? l : m;

  Real f;
  if (cnst6 >= zero) {
    const int k = ijkufc[c + 3];
    const Real u = m <= 0 ? ucl * fts : cnst6;
    uflx[i - 1] = u;
    const Real cnst1 = static_cast<Real>(ijkcel3[k + 9]);
    const Real cfk = cf[k + 9];
    const Real cnst4 = (cfl - cfk) / (cnst2 + cnst1);
    const Real cnst7 = cnst5 - cnst4;
    // :1837, CNST3+CNST2+CNST2+CNST1; the object reuses CNST2+CNST3 of :1810 as the first sum.
    const Real cnst9 = two / (cnst23 + cnst2 + cnst1);
    Real cnst;
    if (Kokkos::fabs(cnst7) < Kokkos::fabs(cfm - cfk) * (p6 * cnst9)) {
      cnst = cnst5 - (cnst3 + u) * cnst7 * cnst9 / onep5;
    } else {
      const Real a4 = Kokkos::fabs(cnst4);
      const Real a5 = Kokkos::fabs(cnst5);
      // MIN(|CNST4|,|CNST5|) here: minss keeps |CNST5| only when strictly less (:1845, :1849).
      const Real mn = a5 < a4 ? a5 : a4;
      if (static_cast<double>(cnst4) * static_cast<double>(cnst5) > 0.0) {
        cnst = mn * Kokkos::copysign(two, cnst5);
      } else {
        cnst = Kokkos::copysign(one, cnst5) * mn;
      }
    }
    f = ((cnst2 - u) * cnst + cfl) * cnst8;
  } else {
    const int n = ijkufc[c + 6];
    const Real u = l <= 0 ? ucm * fts : cnst6;
    uflx[i - 1] = u;
    const Real cnst1 = static_cast<Real>(ijkcel3[n + 9]);
    const Real cfn = cf[n + 9];
    const Real cnst4 = (cfn - cfm) / (cnst3 + cnst1);
    const Real cnst7 = cnst4 - cnst5;
    // :1868, CNST2+CNST3+CNST3+CNST1.
    const Real cnst9 = two / (cnst23 + cnst3 + cnst1);
    Real cnst;
    if (Kokkos::fabs(cnst7) < Kokkos::fabs(cfn - cfl) * (p6 * cnst9)) {
      cnst = (cnst2 - u) * cnst7 * cnst9 / onep5 + cnst5;
    } else {
      const Real a4 = Kokkos::fabs(cnst4);
      const Real a5 = Kokkos::fabs(cnst5);
      // Here the other way round: minss keeps |CNST4| only when strictly less (:1876, :1880).
      const Real mn = a4 < a5 ? a4 : a5;
      if (static_cast<double>(cnst4) * static_cast<double>(cnst5) > 0.0) {
        cnst = mn * Kokkos::copysign(two, cnst5);
      } else {
        cnst = Kokkos::copysign(one, cnst5) * mn;
      }
    }
    f = (cfm - (cnst3 + u) * cnst) * cnst8;
  }
  fu[i - 1] = f;

  const Real cl = clats[ij];
  fx[i - 1] = cnst5 * cnst0 * cnst8 / (cl * cl);
}

/// CNST0 = AKDif*FTS*FTS*2.0, :1787, computed once per call as the Fortran does.
KOKKOS_INLINE_FUNCTION Real smcxuno3_cnst0(Real akdif, Real fts) {
  return akdif * fts * fts * static_cast<Real>(2.0);
}

/// SMCxUNO3 over i = NUA..NUB in the Fortran order (:1799). The W3_OMPG variant
/// (:1794-1797, :1894-1897) shares the faces among threads with no arithmetic change.
KOKKOS_INLINE_FUNCTION void smcxuno3(int nua, int nub, const Real* cf, const Real* uc, Real* uflx,
                                     Real akdif, Real* fu, Real* fx, Real fts, const int* ijkufc,
                                     const int* ijkcel3, const Real* clats) {
  const Real cnst0 = smcxuno3_cnst0(akdif, fts);
  for (int i = nua; i <= nub; ++i) {
    smcxuno3_point(i, cf, uc, uflx, cnst0, fu, fx, fts, ijkufc, ijkcel3, clats);
  }
}

}  // namespace ww::psmc
