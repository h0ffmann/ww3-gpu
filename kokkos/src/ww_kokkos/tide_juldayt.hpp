// kokkos/src/ww_kokkos/tide_juldayt.hpp
// JULDAYT: calendar day, month, year to Julian day number (Numerical Recipes 2nd ed.),
// in default REAL. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3tidemd.F90:1257-1281 at NOAA-EMC/WW3 03adda91;
// authors M. Foreman (tidal package), adapted to WW3 by F. Ardhuin.
// Evidence: kokkos/port_report/JULDAYT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"

namespace ww::tide {

namespace juldayt_detail {
// Default REAL literals; a float literal rounds the decimal once, as gfortran does.
inline constexpr Real k30_6001 = 30.6001f;
inline constexpr Real k0_01 = 0.01f;
static_assert(k30_6001 == 0x1.e99a02p+4f, "30.6001 as REAL: 3.06000995635986328125e+1 in the GIMPLE");
static_assert(k0_01 == 0x1.47ae14p-7f, "0.01 as REAL: 9.99999977648258209228515625e-3 in the GIMPLE");
}  // namespace juldayt_detail

/// JULDAYT(id, mm, iyyy). INT() is cvttss2si in the object (ww::st4::fint).
/// The Fortran's WRITE(6,*) 'There is no zero year !!' (w3tidemd.F90:1267) fires when
/// iyyy == 0 on entry and does not stop; a kernel does not print, so the caller writes it.
KOKKOS_INLINE_FUNCTION int juldayt(int id, int mm, int iyyy) {
  using juldayt_detail::k0_01;
  using juldayt_detail::k30_6001;
  constexpr int igreg = 15 + 31 * (10 + 12 * 1582);
  static_assert(igreg == 588829, "IGREG at w3tidemd.F90:1265");
  int jy = iyyy;
  if (jy < 0) jy = jy + 1;
  int jm;
  if (mm > 2) {
    jm = mm + 1;
  } else {
    jy = jy - 1;
    jm = mm + 13;
  }
  const Real rjy = static_cast<Real>(jy);
  int julday = ww::st4::fint(rjy * 365.25f) + ww::st4::fint(static_cast<Real>(jm) * k30_6001) +
               id + 1720995;
  if (id + 31 * (mm + 12 * iyyy) >= igreg) {
    const int ja = ww::st4::fint(rjy * k0_01);
    julday = julday + 2 - ja + ww::st4::fint(0.25f * static_cast<Real>(ja));
  }
  return julday;
}

}  // namespace ww::tide
