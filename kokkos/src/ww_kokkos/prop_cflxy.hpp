// kokkos/src/ww_kokkos/prop_cflxy.hpp
// W3CFLXY: the largest CFL number of spatial advection over every spectral bin
// of one sea point, a diagnostic for the CFX output field. Phase 1, translated,
// not regtested.
// WW3 heritage: WW3-noaa/model/src/w3pro3md.F90:1980-2136 at NOAA-EMC/WW3
// 03adda91; author F. Ardhuin. Called by W3WAVE when FLOGRD(9,3) is set, on a
// grid that is not unstructured (w3wavemd.F90:1689-1724). W3_S/W3_T are output
// only; W3_MGP is the Mgp template flag. Evidence:
// kokkos/port_report/W3CFLXY_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "prop_params.hpp"
#include "real.hpp"
#include "st4_params.hpp"  // max_r: a > b ? a : b, maxss with a as destination

namespace ww::prop {

namespace cflxy_detail {

/// :2111-2127 for one ITH: VLCFLX, VLCFLY. The /CLATS of :2111 is applied to
/// ECOS*CG before anything is added; VGX/CLATS and CX/CLATS are their own
/// quotients (the object hoists them, same bits).
template <bool Mgp>
KOKKOS_INLINE_FUNCTION void vlcfl(Real ecos, Real esin, Real cgk, Real clat, Real vgx, Real vgy,
                                  bool flcur, Real cxs, Real cys, Real dpdx, Real dpdy,
                                  Real dqdx, Real dqdy, Real dtg, Real& vlcflx, Real& vlcfly) {
  Real cxtot = (ecos * cgk) / clat;  // :2111
  Real cytot = esin * cgk;           // :2112
  if constexpr (Mgp) {
    cxtot = cxtot - vgx / clat;  // :2114
    cytot = cytot - vgy;         // :2115
  }
  if (flcur) {
    cxtot = cxtot + cxs / clat;  // :2120
    cytot = cytot + cys;         // :2121
  }
  const Real cp = cxtot * dpdx + cytot * dpdy;  // :2124
  const Real cq = cxtot * dqdx + cytot * dqdy;  // :2125
  vlcflx = cp * dtg;                            // :2126
  vlcfly = cq * dtg;                            // :2127
}

}  // namespace cflxy_detail

/// W3CFLXY for sea point ISEA (1-based, as the Fortran's). MAPSTA and MAPFS are
/// dummies the Fortran never reads; kept for the signature. CFLXYMAX is set to 0
/// (:2105) and then raised. Module state: NK, NTH, FLCUR from p; MAPSF, CLATS,
/// ECOS, ESIN, DPDX..DQDY from g; CG, CX, CY from s (layouts in prop_params.hpp).
///
/// MAX(VLCFLX,VLCFLY,CFLXYMAX) (:2128) is a MAX_EXPR whose result for a NaN or a
/// tie of zeros of opposite sign depends on the emitted maxss/maxps operands,
/// and gfortran vectorises the ITH loop (16-byte vectors, unroll 2) when
/// 2 <= NTH < HUGE: four running maxima per IK, lanes {VLCFLX(ITH), VLCFLY(ITH),
/// VLCFLX(ITH+1), VLCFLY(ITH+1)}, each started from the CFLXYMAX of the previous
/// IK, reduced as max(max(l3,l1), max(l2,l0)), then the odd ITH = NTH. Otherwise
/// a scalar loop. The operand order of every max below is the object's.
template <bool Mgp = false>
KOKKOS_INLINE_FUNCTION void w3cflxy(int isea, Real dtg, const int* /*mapsta*/,
                                    const int* /*mapfs*/, Real& cflxymax, Real vgx, Real vgy,
                                    const Params& p, const Grid& g, const Step& s) {
  using ww::st4::max_r;
  cflxymax = static_cast<Real>(0.);  // :2105
  const int nk = p.nk;
  const int nth = p.nth;
  const int ix = g.mapsf[isea - 1];           // :2106
  const int iy = g.mapsf[p.nsea + isea - 1];  // :2107; IXY of :2108 is never used
  const int ixy = (ix - 1) * p.ny + (iy - 1);
  const Real clat = g.clats[isea];
  const Real dpdx = g.dpdx[ixy];
  const Real dpdy = g.dpdy[ixy];
  const Real dqdx = g.dqdx[ixy];
  const Real dqdy = g.dqdy[ixy];
  const bool flcur = p.flcur != 0;
  const Real cxs = s.cx[isea];
  const Real cys = s.cy[isea];
  const int cgld = nk + 2;

  for (int ik = 1; ik <= nk; ++ik) {  // :2109
    const Real cgk = s.cg[isea * cgld + ik];
    Real c = cflxymax;
    Real vx;
    Real vy;
    if (nth >= 2 && nth != 2147483647) {
      // The vectorised loop (object 0x370-0x3cd, FLCUR 0x4e0-0x548): maxps
      // %v,%acc, acc > v ? acc : v per lane.
      Real a0 = c;
      Real a1 = c;
      Real a2 = c;
      Real a3 = c;
      const int npair = nth / 2;
      for (int j = 0; j < npair; ++j) {
        const int ith = 2 * j + 1;
        Real vx1;
        Real vy1;
        cflxy_detail::vlcfl<Mgp>(g.ecos[ith - 1], g.esin[ith - 1], cgk, clat, vgx, vgy, flcur,
                                 cxs, cys, dpdx, dpdy, dqdx, dqdy, dtg, vx, vy);
        cflxy_detail::vlcfl<Mgp>(g.ecos[ith], g.esin[ith], cgk, clat, vgx, vgy, flcur, cxs, cys,
                                 dpdx, dpdy, dqdx, dqdy, dtg, vx1, vy1);
        a0 = max_r(a0, vx);
        a1 = max_r(a1, vy);
        a2 = max_r(a2, vx1);
        a3 = max_r(a3, vy1);
      }
      // movhlps; maxps %lo,%hi; shufps $0x55; maxps: hi > lo ? hi : lo.
      const Real b0 = max_r(a2, a0);
      const Real b1 = max_r(a3, a1);
      c = max_r(b1, b0);
      if (nth != 2 * npair) {
        // The scalar epilogue for ITH = NTH (0x3d8-0x436, FLCUR 0x553-0x5dd):
        // maxss VLCFLX into VLCFLY without currents, the other way with them;
        // then the reduced maximum is the destination.
        cflxy_detail::vlcfl<Mgp>(g.ecos[nth - 1], g.esin[nth - 1], cgk, clat, vgx, vgy, flcur,
                                 cxs, cys, dpdx, dpdy, dqdx, dqdy, dtg, vx, vy);
        const Real m = flcur ? max_r(vx, vy) : max_r(vy, vx);
        c = max_r(c, m);
      }
    } else {
      // NTH = 1 (or HUGE): the scalar loop (0x770-0x7dd, FLCUR 0x670-0x6e3).
      // Under MGP the loop without currents puts VLCFLX first too. The exit test
      // before the increment keeps ITH from overflowing at NTH = HUGE.
      for (int ith = 1; ith <= nth; ++ith) {
        cflxy_detail::vlcfl<Mgp>(g.ecos[ith - 1], g.esin[ith - 1], cgk, clat, vgx, vgy, flcur,
                                 cxs, cys, dpdx, dpdy, dqdx, dqdy, dtg, vx, vy);
        const Real m = (flcur || Mgp) ? max_r(vx, vy) : max_r(vy, vx);
        c = max_r(c, m);
        if (ith == nth) break;
      }
    }
    cflxymax = c;  // :2128, stored once per IK as the object does
  }
}

}  // namespace ww::prop
