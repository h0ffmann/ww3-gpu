// kokkos/src/ww_kokkos/prop_freq_shift.hpp
// PROP_FREQ_SHIFT of W3PARALL: the per-point frequency-shift velocity CAS(NSPEC) and the
// wavenumber steps DMM(0:NK2) of the PDLIB implicit scheme, FreqShiftMethod 1. Phase 1,
// translated, not regtested. WW3 heritage: WW3-noaa/model/src/w3parall.F90:604-739 at
// NOAA-EMC/WW3 03adda91; A. Roland, M. Dutour-Sikiric. Evidence:
// kokkos/port_report/PROP_FREQ_SHIFT_KOKKOS.md. CFLK(NK2,NTH) is not stored: its rows
// IK <= NK go straight into CAS (:730-735 read nothing else). ⚠ In the vectorised :703 and
// :725 the operands of + and * commute between body and tail: only two NaN payloads differ.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "prop_parall.hpp"  // PropParallArrays, parall_detail::dsdd_fill
#include "real.hpp"

namespace ww::pdlib {

/// Work reals per call: DSDD(0:NK+1), DB(NK2), FKC(NTH); one buffer per thread. Never more
/// than prop_parall_work_reals, so one buffer serves the three w3parall routines.
KOKKOS_INLINE_FUNCTION constexpr int prop_freq_shift_work_reals(int nk, int nth) {
  return (nk + 2) + (nk + 2) + nth;
}

/// PROP_FREQ_SHIFT, :604-739. ip, isea 1-based; cas = CAS(1:NSPEC), dmm = DMM(0:NK2), both
/// written in full; work has prop_freq_shift_work_reals entries.
KOKKOS_INLINE_FUNCTION void prop_freq_shift(const PropParallArrays& a, int ip, int isea,
                                            Real* cas, Real* dmm, Real dtg, Real* work) {
  const int nk = a.nk;
  const int nth = a.nth;
  Real* dsdd = work;          // DSDD(0:NK+1): DSDD(IK) = dsdd[IK]
  Real* db = dsdd + nk + 2;   // DB(NK2):      DB(J)    = db[J-1]
  Real* fkc = db + nk + 2;    // FKC(NTH):     FKC(ITH) = fkc[ITH-1]
  const Real* cg = a.cg + static_cast<long>(isea) * (nk + 2);
  const Real* wn = a.wn + static_cast<long>(isea) * (nk + 2);

  // :679-695.
  long j;
  if (a.lpdlib != 0) {
    j = ip - 1;  // X(1,IP)
  } else {
    const int ix = a.mapsf[(isea - 1) + 0 * a.msea];
    const int iy = a.mapsf[(isea - 1) + 1 * a.msea];
    j = (iy - 1) + static_cast<long>(ix - 1) * a.ny;  // X(IY,IX)
  }
  const Real edcxdx = a.dcxdx[j];
  const Real edcxdy = a.dcxdy[j];
  const Real edcydx = a.dcydx[j];
  const Real edcydy = a.dcydy[j];
  const Real edddx = a.dddx[j];
  const Real edddy = a.dddy[j];
  // :696-702.
  const Real ecx = a.cx[isea];
  const Real ecy = a.cy[isea];
  const Real dcxx = -edcxdx;
  const Real dcxyyx = -(edcxdy + edcydx);
  const Real dcyy = -edcydy;
  const Real fkd = ecx * edddx + ecy * edddy;
  const Real fack = dtg;
  // :703-705.
  for (int ith = 1; ith <= nth; ++ith) {
    fkc[ith - 1] = (a.ec2[ith - 1] * dcxx + a.esc[ith - 1] * dcxyyx) + a.es2[ith - 1] * dcyy;
  }
  // :706-712.
  for (int ik = 0; ik <= nk; ++ik) {
    db[ik] = a.dsip[ik] / cg[ik];       // DB(IK+1)
    dmm[ik + 1] = wn[ik + 1] - wn[ik];  // DMM(IK+1)
  }
  db[nk + 1] = a.dsip[nk + 1] / cg[nk + 1];  // DB(NK+2)
  dmm[nk + 2] = kZero;  // DMM(NK+2) = ZERO (pdlib_params.hpp)
  dmm[0] = dmm[1];
  // :714-721.
  parall_detail::dsdd_fill(a, isea, dsdd);
  // :722-729 with :730-735 folded in: CFLK(IK+1,ITH) is CAS(ITH + IK*NTH) for IK+1 <= NK.
  for (int ik = 0; ik <= nk + 1; ++ik) {
    const Real fkd0 = (fkd / cg[ik]) * dsdd[ik];
    const Real velfac = fack / db[ik];
    for (int ith = 1; ith <= nth; ++ith) {
      const Real velnofilt = (fkd0 + wn[ik] * fkc[ith - 1]) * velfac;
      const Real cflk = velnofilt / velfac;
      if (ik + 1 <= nk) cas[(ith - 1) + ik * nth] = cflk;
    }
  }
}

}  // namespace ww::pdlib
