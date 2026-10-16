// kokkos/src/ww_kokkos/psmc_smcdcxy.hpp
// SMCDCXY: current-velocity gradients of an SMC grid, limited, rotated to map east in the
// Arctic part and spread on the full NY x NX grid. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3psmcmd.F90:3048-3159 at NOAA-EMC/WW3 03adda91;
// author Jian-Guo Li. Evidence: kokkos/port_report/SMCDCXY_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "prop_params.hpp"
#include "real.hpp"

namespace ww::psmc {

/// One iteration n (1-based) of the DO n=1,NSEA loops at :3079-3101 and :3125-3147, which are
/// the same code on (DCXDX, DCXDY) and on (DCYDX, DCYDY). grhx, grhy: GrHx, GrHy(NSEA), 1-based in
/// Fortran, 0-based here; limited and rotated in place. angarc: ANGARC(NARC), 1-based, 0-based here.
/// ijkcel: IJKCel(4,-9:MCel) at its column n=1, so cell n is ijkcel[4*(n-1) .. 4*(n-1)+3].
/// ddx, ddy: the full-grid arrays (NY,NX), column-major, leading dimension ny.
/// Under W3_OMPG the n loop is a parallel DO; it writes disjoint blocks only when no two cells
/// cover the same grid point, which an SMC cell list satisfies.
KOKKOS_INLINE_FUNCTION void smcdcxy_point(int n, Real* grhx, Real* grhy, bool arctc, int nglo,
                                          const Real* angarc, const int* ijkcel, int mrfct,
                                          int ny, Real* ddx, Real* ddy) {
  const Real lim = static_cast<Real>(0.01);
  // SIGN(0.01, x) is copysignf (GIMPLE :3082); a NaN fails the test and stays.
  if (Kokkos::fabs(grhx[n - 1]) > lim) grhx[n - 1] = Kokkos::copysign(lim, grhx[n - 1]);
  if (Kokkos::fabs(grhy[n - 1]) > lim) grhy[n - 1] = Kokkos::copysign(lim, grhy[n - 1]);

  if (arctc && n > nglo) {
    const Real cnst0 = angarc[n - nglo - 1] * ww::prop::kDera;
    // gfortran calls sincosf once for COS and SIN of CNST0.
    const Real c = Kokkos::cos(cnst0);
    const Real s = Kokkos::sin(cnst0);
    const Real cnst1 = grhx[n - 1] * c - grhy[n - 1] * s;
    const Real cnst2 = grhx[n - 1] * s + grhy[n - 1] * c;
    grhx[n - 1] = cnst1;
    grhy[n - 1] = cnst2;
  }

  const int* cel = ijkcel + 4L * static_cast<long>(n - 1);
  const int i = cel[0] / mrfct + 1;
  const int j = cel[1] / mrfct + 1;
  const int k = Kokkos::max(1, cel[2] / mrfct);
  const int m = Kokkos::max(1, cel[3] / mrfct);
  const Real gx = grhx[n - 1];
  const Real gy = grhy[n - 1];
  for (int ic = i; ic <= i + k - 1; ++ic) {
    const long col = static_cast<long>(ic - 1) * static_cast<long>(ny);
    for (int jr = j; jr <= j + m - 1; ++jr) ddx[col + (jr - 1)] = gx;
  }
  for (int ic = i; ic <= i + k - 1; ++ic) {
    const long col = static_cast<long>(ic - 1) * static_cast<long>(ny);
    for (int jr = j; jr <= j + m - 1; ++jr) ddy[col + (jr - 1)] = gy;
  }
}

/// SMCDCXY on the host. cx, cy: CX, CY(0:NSEA) (w3adatmd) passed from CX(1), so cx[n-1] = CX(n)
/// as CX(1:NSEA) at :3066 reads it. dcxdx, dcxdy, dcydx, dcydy:
/// (NY,NX) column-major, overwritten. gradn: the caller's SMCGradn (:2341), called as
/// gradn(const Real* cvq, Real* grdx, Real* grdy, int l0r1) on NSEA arrays (L0r1 is INTENT(IN));
/// SMCGradn is not ported yet.
/// cxcy, grhx, grhy: the automatic arrays CXCY, GrHx, GrHy(NSEA) (:3058), caller-provided.
template <class Gradn>
inline void smcdcxy(int nx, int ny, int nsea, int mrfct, const int* ijkcel, int nglo,
                    const Real* angarc, bool arctc, const Real* cx, const Real* cy, Real* dcxdx,
                    Real* dcxdy, Real* dcydx, Real* dcydy, Gradn&& gradn, Real* cxcy, Real* grhx,
                    Real* grhy) {
  const long nxy = static_cast<long>(nx) * static_cast<long>(ny);
  const int l = 1;
  for (int n = 0; n < nsea; ++n) cxcy[n] = cx[n];
  for (long p = 0; p < nxy; ++p) dcxdx[p] = static_cast<Real>(0.0);
  for (long p = 0; p < nxy; ++p) dcxdy[p] = static_cast<Real>(0.0);
  gradn(static_cast<const Real*>(cxcy), grhx, grhy, l);
  for (int n = 1; n <= nsea; ++n)
    smcdcxy_point(n, grhx, grhy, arctc, nglo, angarc, ijkcel, mrfct, ny, dcxdx, dcxdy);

  for (int n = 0; n < nsea; ++n) cxcy[n] = cy[n];
  for (long p = 0; p < nxy; ++p) dcydx[p] = static_cast<Real>(0.0);
  for (long p = 0; p < nxy; ++p) dcydy[p] = static_cast<Real>(0.0);
  gradn(static_cast<const Real*>(cxcy), grhx, grhy, l);
  for (int n = 1; n <= nsea; ++n)
    smcdcxy_point(n, grhx, grhy, arctc, nglo, angarc, ijkcel, mrfct, ny, dcydx, dcydy);
}

}  // namespace ww::psmc
