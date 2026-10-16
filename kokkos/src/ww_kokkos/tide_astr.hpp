// kokkos/src/ww_kokkos/tide_astr.hpp
// ASTR: mean longitudes of sun, solar perigee, moon, lunar perigee and the negative lunar
// node (cycles) and their rates (cycles per 365 days) at a time in days. Phase 1, translated,
// not regtested.
// WW3 heritage: WW3-noaa/model/src/w3tidemd.F90:910-971 at NOAA-EMC/WW3 03adda91;
// authors M. Foreman (tidal package), adapted to WW3 by F. Ardhuin.
// Evidence: kokkos/port_report/ASTR_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::tide {

/// ASTR(D1, H,PP,S,P,NP, DH,DPP,DS,DP,DNP), all REAL(KIND=8).
/// The products of two literals (2.d-8*.00002267d0 ...) are folded by both compilers to the same
/// double, left to right as written; d2**3 is d2*(d2*d2) and d1**2 is d1*d1 in the object.
KOKKOS_INLINE_FUNCTION void astr(double d1, double& h, double& pp, double& s, double& p,
                                 double& np, double& dh, double& dpp, double& ds, double& dp,
                                 double& dnp) {
  const double d2 = d1 * 1.e-4;
  const double f = 360.0;
  const double f2 = f / 365.0;
  const double d2_3 = d2 * (d2 * d2);
  h = 279.696678 + .9856473354 * d1 + .00002267 * d2 * d2;
  pp = 281.220833 + .0000470684 * d1 + .0000339 * d2 * d2 + .00000007 * d2_3;
  s = 270.434164 + 13.1763965268 * d1 - .000085 * d2 * d2 + .000000039 * d2_3;
  p = 334.329556 + .1114040803 * d1 - .0007739 * d2 * d2 - .00000026 * d2_3;
  np = -259.183275 + .0529539222 * d1 - .0001557 * d2 * d2 - .00000005 * d2_3;
  h = h / f;
  pp = pp / f;
  s = s / f;
  p = p / f;
  np = np / f;
  h = h - Kokkos::trunc(h);
  pp = pp - Kokkos::trunc(pp);
  s = s - Kokkos::trunc(s);
  p = p - Kokkos::trunc(p);
  np = np - Kokkos::trunc(np);
  const double d1_2 = d1 * d1;
  dh = .9856473354 + 2.e-8 * .00002267 * d1;
  dpp = .0000470684 + 2.e-8 * .0000339 * d1 + 3.e-12 * .00000007 * d1_2;
  ds = 13.1763965268 - 2.e-8 * .000085 * d1 + 3.e-12 * .000000039 * d1_2;
  dp = .1114040803 - 2.e-8 * .0007739 * d1 - 3.e-12 * .00000026 * d1_2;
  dnp = +.0529539222 - 2.e-8 * .0001557 * d1 - 3.e-12 * .00000005 * d1_2;
  dh = dh / f2;
  dpp = dpp / f2;
  ds = ds / f2;
  dp = dp / f2;
  dnp = dnp / f2;
}

}  // namespace ww::tide
