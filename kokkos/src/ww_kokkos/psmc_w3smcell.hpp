// kokkos/src/ww_kokkos/psmc_w3smcell.hpp
// W3SMCELL: centre longitude and latitude, in degrees, of a list of SMC cells of one grid.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3psmcmd.F90:3665-3784 at NOAA-EMC/WW3 03adda91;
// author Jian-Guo Li. Evidence: kokkos/port_report/W3SMCELL_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::psmc {

/// One iteration ij (1-based) of the DO ij=1,NC loop at :3757-3775. idcl, xlon, ylat: IDCl,
/// XLon, YLat(NC), 1-based in Fortran, 0-based here. ijkcel: GRIDS(IMOD)%IJKCel(4,-9:MCel) at its
/// column 1, so cell ijp is ijkcel[4*(ijp-1) .. 4*(ijp-1)+3]. xi0, yj0, dx1, dy1, nsem: the
/// locals of :3743-3750, which w3smcell computes once.
/// Under W3_OMPG the loop is a parallel DO; every iteration writes its own ij only.
KOKKOS_INLINE_FUNCTION void w3smcell_point(int ij, const int* idcl, Real* xlon, Real* ylat,
                                           const int* ijkcel, Real xi0, Real yj0, Real dx1,
                                           Real dy1, int nsem) {
  const int ijp = idcl[ij - 1];
  if (ijp < 1 || ijp > nsem) {
    xlon[ij - 1] = static_cast<Real>(0.0);
    ylat[ij - 1] = static_cast<Real>(-90.0);
  } else {
    const int* cel = ijkcel + 4L * static_cast<long>(ijp - 1);
    const int i1 = cel[0];
    const int j2 = cel[1];
    const int i3 = cel[2];
    const int j4 = cel[3];
    // (FLOAT(I1) + 0.5*FLOAT(I3)) is a PAREN_EXPR (:3772-3773), multiplied by DX1 before XI0.
    xlon[ij - 1] =
        xi0 + (static_cast<Real>(i1) + static_cast<Real>(0.5) * static_cast<Real>(i3)) * dx1;
    ylat[ij - 1] =
        yj0 + (static_cast<Real>(j2) + static_cast<Real>(0.5) * static_cast<Real>(j4)) * dy1;
  }
}

/// W3SMCELL on the host for one grid IMOD: the caller passes GRIDS(IMOD)'s fields in place of
/// IMOD. nc, idcl, xlon, ylat: the dummies NC, IDCl, XLon, YLat (0-based). sx, sy, x0, y0, mrfct,
/// nsea, ijkcel: GRIDS(IMOD)%SX, SY, X0, Y0, MRFct, NSEA, IJKCel (w3gdatmd), ijkcel as in
/// w3smcell_point.
inline void w3smcell(int nc, const int* idcl, Real* xlon, Real* ylat, Real sx, Real sy, Real x0,
                     Real y0, int mrfct, int nsea, const int* ijkcel) {
  const Real dxg = sx;
  const Real dyg = sy;
  const Real xi0 = x0 - static_cast<Real>(0.5) * dxg;
  const Real yj0 = y0 - static_cast<Real>(0.5) * dyg;
  const int mrf = mrfct;
  // Real(MRF) is converted once and shared by both divisions in the object (:3748-3749).
  const Real dx1 = dxg / static_cast<Real>(mrf);
  const Real dy1 = dyg / static_cast<Real>(mrf);
  const int nsem = nsea;

  for (int ij = 1; ij <= nc; ++ij)
    w3smcell_point(ij, idcl, xlon, ylat, ijkcel, xi0, yj0, dx1, dy1, nsem);

  // WHERE(XLon < 0.0) XLon = XLon + 360.0 (:3781); a NaN fails the test and stays.
  for (int ij = 0; ij < nc; ++ij)
    if (xlon[ij] < static_cast<Real>(0.0)) xlon[ij] = xlon[ij] + static_cast<Real>(360.0);
}

}  // namespace ww::psmc
