// kokkos/src/ww_kokkos/tide_caldatt.hpp
// CALDATT: Julian day number to calendar day, month, year (Numerical Recipes 2nd ed.),
// in default REAL. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3tidemd.F90:1284-1310 at NOAA-EMC/WW3 03adda91;
// authors M. Foreman (tidal package), adapted to WW3 by F. Ardhuin.
// Evidence: kokkos/port_report/CALDATT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"

namespace ww::tide {

namespace caldatt_detail {
// The literals are default REAL in the Fortran; a float literal rounds the decimal once,
// as gfortran does (a double rounded to float could round twice).
inline constexpr Real k122_1 = 122.1f;
inline constexpr Real k30_6001 = 30.6001f;
static_assert(k122_1 == 0x1.e86666p+6f, "122.1 as REAL: 1.2209999847412109375e+2 in the GIMPLE");
static_assert(k30_6001 == 0x1.e99a02p+4f, "30.6001 as REAL: 3.06000995635986328125e+1 in the GIMPLE");
}  // namespace caldatt_detail

/// CALDATT(julian, id, mm, iyyy). INT() is cvttss2si in the object (ww::st4::fint).
KOKKOS_INLINE_FUNCTION void caldatt(int julian, int& id, int& mm, int& iyyy) {
  using caldatt_detail::k122_1;
  using caldatt_detail::k30_6001;
  constexpr int igreg = 2299161;
  int ja;
  if (julian >= igreg) {
    const int jalpha = ww::st4::fint(
        (static_cast<Real>(julian - 1867216) - 0.25f) / 36524.25f);
    ja = julian + 1 + jalpha - ww::st4::fint(0.25f * static_cast<Real>(jalpha));
  } else {
    ja = julian;
  }
  const int jb = ja + 1524;
  const int jc = ww::st4::fint(6680.f + (static_cast<Real>(jb - 2439870) - k122_1) / 365.25f);
  const int jd = 365 * jc + ww::st4::fint(0.25f * static_cast<Real>(jc));
  const int je = ww::st4::fint(static_cast<Real>(jb - jd) / k30_6001);
  id = jb - jd - ww::st4::fint(k30_6001 * static_cast<Real>(je));
  mm = je - 1;
  if (mm > 12) mm = mm - 12;
  iyyy = jc - 4715;
  if (mm > 2) iyyy = iyyy - 1;
  if (iyyy <= 0) iyyy = iyyy - 1;
}

}  // namespace ww::tide
