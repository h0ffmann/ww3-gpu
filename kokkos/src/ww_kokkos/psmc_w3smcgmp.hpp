// kokkos/src/ww_kokkos/psmc_w3smcgmp.hpp
// W3SMCGMP: the cell of one SMC grid that holds each of a list of longitude-latitude points.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3psmcmd.F90:3808-3930 at NOAA-EMC/WW3 03adda91;
// author Jian-Guo Li. Evidence: kokkos/port_report/W3SMCGMP_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"

namespace ww::psmc {

namespace smcgmp_detail {

/// FLOOR(x) as gfortran -O3 inlines it at :3897-3898: i = INT(x) (cvttss2si / cvttps2dq),
/// then x >= REAL(i) ? i : i-1. A NaN or out-of-range x gives INT_MIN - 1, which wraps to
/// INT_MAX in the object (paddd); the unsigned subtraction keeps that without UB.
KOKKOS_INLINE_FUNCTION int ffloor(Real x) {
  const int i = ww::st4::fint(x);
  return x >= static_cast<Real>(i) ? i : static_cast<int>(static_cast<unsigned>(i) - 1u);
}

/// I + I3 as the object's 32-bit add, which wraps.
KOKKOS_INLINE_FUNCTION int add_wrap(int a, int b) {
  return static_cast<int>(static_cast<unsigned>(a) + static_cast<unsigned>(b));
}

}  // namespace smcgmp_detail

/// W3SMCGMP for one grid IMOD: the caller passes GRIDS(IMOD)'s fields in place of IMOD.
/// nc, xlon, ylat, idcl: the dummies NC, XLon, YLat, IDCl(NC), 1-based in Fortran, 0-based here.
/// sx, sy, x0, y0, mrfct, nsea: GRIDS(IMOD)%SX, SY, X0, Y0, MRFct, NSEA (w3gdatmd). ijkcel:
/// GRIDS(IMOD)%IJKCel(4,-9:MCel) at its column 1, contiguous, so cell ij is
/// ijkcel[4*(ij-1) .. 4*(ij-1)+3]. xlow, ix1, jy1: the automatic arrays XLow, IX1, JY1 (:3874-3875),
/// caller-provided, NC each.
/// Sequential by construction: the search stops when NFund reaches NC, and the EXIT of :3919
/// gives each cell to at most one point (the first unmatched one in list order); a second point
/// in the same cell keeps IDCl = 0, as in the Fortran.
KOKKOS_INLINE_FUNCTION void w3smcgmp(int nc, const Real* xlon, const Real* ylat, int* idcl,
                                     Real sx, Real sy, Real x0, Real y0, int mrfct, int nsea,
                                     const int* ijkcel, Real* xlow, int* ix1, int* jy1) {
  using smcgmp_detail::add_wrap;
  using smcgmp_detail::ffloor;

  const Real dxg = sx;
  const Real dyg = sy;
  const Real xi0 = x0 - static_cast<Real>(0.5) * dxg;
  const Real yj0 = y0 - static_cast<Real>(0.5) * dyg;
  const int mrf = mrfct;
  // Real(MRF) is converted once and shared by both divisions in the object (:3888-3889).
  const Real dx1 = dxg / static_cast<Real>(mrf);
  const Real dy1 = dyg / static_cast<Real>(mrf);
  const int nsem = nsea;

  // XLow = XLon; WHERE(XLow < XI0) XLow = XLow + 360.0 (:3893-3894); a NaN fails the test.
  for (int i = 0; i < nc; ++i) xlow[i] = xlon[i];
  for (int i = 0; i < nc; ++i)
    if (xlow[i] < xi0) xlow[i] = xlow[i] + static_cast<Real>(360.0);

  // (XLow - XI0) is a PAREN_EXPR, divided by DX1, not multiplied by its reciprocal (:3897-3898).
  for (int i = 0; i < nc; ++i) ix1[i] = ffloor((xlow[i] - xi0) / dx1);
  for (int i = 0; i < nc; ++i) jy1[i] = ffloor((ylat[i] - yj0) / dy1);

  for (int i = 0; i < nc; ++i) idcl[i] = 0;

  int nfund = 0;
  int ij = 0;
  while (ij < nsem && nfund < nc) {
    ij = ij + 1;
    const int* cel = ijkcel + 4L * static_cast<long>(ij - 1);
    const int i1 = cel[0];
    const int j2 = cel[1];
    const int i3 = cel[2];
    const int j4 = cel[3];
    for (int ijp = 1; ijp <= nc; ++ijp) {
      if (idcl[ijp - 1] == 0) {
        if ((ix1[ijp - 1] >= i1) && (ix1[ijp - 1] < add_wrap(i1, i3)) && (jy1[ijp - 1] >= j2) &&
            (jy1[ijp - 1] < add_wrap(j2, j4))) {
          nfund = nfund + 1;
          idcl[ijp - 1] = ij;
          break;
        }
      }
    }
  }
}

}  // namespace ww::psmc
