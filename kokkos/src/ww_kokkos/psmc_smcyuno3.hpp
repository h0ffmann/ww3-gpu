// kokkos/src/ww_kokkos/psmc_smcyuno3.hpp
// SMCyUNO3: mid-flux values on the V-faces of a multi-resolution SMC grid, UNO3 scheme
// (advective mid-flux and diffusion flux). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3psmcmd.F90:1919-2063 at NOAA-EMC/WW3 03adda91;
// author Jian-Guo Li. Evidence: kokkos/port_report/SMCYUNO3_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::psmc {

/// One V-face j (1-based, the Fortran loop counter), :1952-2055.
/// cf, vc: CF(-9:NCel), VC(-9:NCel) as cf[c+9], vc[c+9].
/// vfly, fv, fy: VFLY, FV, FY(NVFc) as v[j-1] (outputs).
/// cnst0: AKDif*FTS*FTS*2.0 of :1942, from smcyuno3_cnst0.
/// ijkvfc: IJKVFc(7,NVFc) (w3gdatmd), contiguous column-major, IJKVFc(r,j) = ijkvfc[(r-1)+7*(j-1)].
/// ijkcel4: IJKCel4(-9:MCel) (w3gdatmd) as ijkcel4[c+9].
/// clatf: CLATF(NVFc) (w3gdatmd) as clatf[j-1].
// Commutative operands are written in the order the Fortran object puts them (first source
// first), so that a sum or product of two NaNs keeps the Fortran's payload.
KOKKOS_INLINE_FUNCTION void smcyuno3_point(int j, const Real* cf, const Real* vc, Real* vfly,
                                           Real cnst0, Real* fv, Real* fy, Real fts,
                                           const int* ijkvfc, const int* ijkcel4,
                                           const Real* clatf) {
  const long c = 7L * static_cast<long>(j - 1);
  const Real zero = static_cast<Real>(0.0);
  const Real half = static_cast<Real>(0.5);
  const Real one = static_cast<Real>(1.0);
  const Real two = static_cast<Real>(2.0);
  const Real p6 = static_cast<Real>(0.6);
  const Real onep5 = static_cast<Real>(1.5);

  const int l = ijkvfc[c + 4];
  const int m = ijkvfc[c + 5];

  const Real cnst2 = static_cast<Real>(ijkcel4[l + 9]);
  const Real cnst3 = static_cast<Real>(ijkcel4[m + 9]);
  const Real cfm = cf[m + 9];
  const Real cfl = cf[l + 9];
  const Real cnst23 = cnst2 + cnst3;
  const Real cnst5 = (cfm - cfl) / cnst23;

  const Real vcl = vc[l + 9];
  const Real vcm = vc[m + 9];
  const Real cnst6 = half * (vcl + vcm) * fts;

  // :1972, CLATF(j)*FLOAT(IJKVFc(3,j)); the integer-valued factor cannot be NaN.
  const Real cnst8 = static_cast<Real>(ijkvfc[c + 2]) * clatf[j - 1];

  // :2053, CNST0*CNST5*CNST8, written before the branches: the object multiplies CNST5 by
  // CNST0 first, and g++ keeps CNST5 as first source only with the product here.
  fy[j - 1] = cnst5 * cnst0 * cnst8;

  Real f;
  if (cnst6 >= zero) {
    const int k = ijkvfc[c + 3];
    const Real v = m <= 0 ? vcl * fts : cnst6;
    vfly[j - 1] = v;
    // :1983, CNST3 = CNST2 at the boundary.
    const Real c3 = m <= 0 ? cnst2 : cnst3;
    const Real c23 = m <= 0 ? cnst2 + cnst2 : cnst23;
    const Real cnst1 = static_cast<Real>(ijkcel4[k + 9]);
    const Real cfk = cf[k + 9];
    const Real cnst4 = (cfl - cfk) / (cnst2 + cnst1);
    const Real cnst7 = cnst5 - cnst4;
    // :1992, CNST3+CNST2+CNST2+CNST1; the object starts from CNST2+CNST3 (exact integer sums).
    const Real cnst9 = two / (c23 + cnst2 + cnst1);
    Real cnst;
    if (Kokkos::fabs(cnst7) < Kokkos::fabs(cfm - cfk) * (p6 * cnst9)) {
      cnst = cnst5 - (c3 + v) * cnst7 * cnst9 / onep5;
    } else {
      const Real a4 = Kokkos::fabs(cnst4);
      const Real a5 = Kokkos::fabs(cnst5);
      // MIN(|CNST4|,|CNST5|) here: minss keeps |CNST5| only when strictly less (:2000, :2005).
      const Real mn = a5 < a4 ? a5 : a4;
      if (static_cast<double>(cnst4) * static_cast<double>(cnst5) > 0.0) {
        cnst = mn * Kokkos::copysign(two, cnst5);
      } else {
        cnst = Kokkos::copysign(one, cnst5) * mn;
      }
    }
    f = ((cnst2 - v) * cnst + cfl) * cnst8;
  } else {
    const int n = ijkvfc[c + 6];
    const Real v = l <= 0 ? vcm * fts : cnst6;
    vfly[j - 1] = v;
    // :2019, CNST2 = CNST3 at the boundary.
    const Real c2 = l <= 0 ? cnst3 : cnst2;
    const Real c23 = l <= 0 ? cnst3 + cnst3 : cnst23;
    const Real cnst1 = static_cast<Real>(ijkcel4[n + 9]);
    const Real cfn = cf[n + 9];
    const Real cnst4 = (cfn - cfm) / (cnst3 + cnst1);
    const Real cnst7 = cnst4 - cnst5;
    // :2029, CNST2+CNST3+CNST3+CNST1.
    const Real cnst9 = two / (c23 + cnst3 + cnst1);
    Real cnst;
    if (Kokkos::fabs(cnst7) < Kokkos::fabs(cfn - cfl) * (p6 * cnst9)) {
      cnst = (c2 - v) * cnst7 * cnst9 / onep5 + cnst5;
    } else {
      const Real a4 = Kokkos::fabs(cnst4);
      const Real a5 = Kokkos::fabs(cnst5);
      // Here the other way round: minss keeps |CNST4| only when strictly less (:2037, :2042).
      const Real mn = a4 < a5 ? a4 : a5;
      if (static_cast<double>(cnst4) * static_cast<double>(cnst5) > 0.0) {
        cnst = mn * Kokkos::copysign(two, cnst5);
      } else {
        cnst = Kokkos::copysign(one, cnst5) * mn;
      }
    }
    f = (cfm - (cnst3 + v) * cnst) * cnst8;
  }
  fv[j - 1] = f;

}

/// CNST0 = AKDif*FTS*FTS*2.0, :1942, computed once per call as the Fortran does.
KOKKOS_INLINE_FUNCTION Real smcyuno3_cnst0(Real akdif, Real fts) {
  return akdif * fts * fts * static_cast<Real>(2.0);
}

/// SMCyUNO3 over j = NVA..NVB in the Fortran order (:1952). The W3_OMPG variant
/// (:1946-1950, :2057-2060) shares the faces among threads with no arithmetic change.
KOKKOS_INLINE_FUNCTION void smcyuno3(int nva, int nvb, const Real* cf, const Real* vc, Real* vfly,
                                     Real akdif, Real* fv, Real* fy, Real fts, const int* ijkvfc,
                                     const int* ijkcel4, const Real* clatf) {
  const Real cnst0 = smcyuno3_cnst0(akdif, fts);
  for (int j = nva; j <= nvb; ++j) {
    smcyuno3_point(j, cf, vc, vfly, cnst0, fv, fy, fts, ijkvfc, ijkcel4, clatf);
  }
}

}  // namespace ww::psmc
