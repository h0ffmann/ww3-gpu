// kokkos/src/ww_kokkos/profs_setdepth.hpp
// SETDEPTH: flags the unstructured-grid nodes shallower than DMIN + 1e-5 as dry (IOBDP = 0)
// before the explicit fluctuation-splitting advection W3XYPUG. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:1601-1678 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, M. Dutour-Sikiric. Evidence: kokkos/port_report/SETDEPTH_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <Kokkos_Core.hpp>

namespace ww::profs {

namespace setdepth_detail {
// :1667, REAL*8 DTHR = 10E-6: the literal is default REAL, widened to double after rounding.
inline constexpr double kDthr = static_cast<double>(10E-6f);
static_assert(kDthr == 0x1.4f8b58p-17, "DTHR is (double)(float)1e-5");
}  // namespace setdepth_detail

/// SETDEPTH (no dummies; module state only). iobdp is W3GDATMD's IOBDP(1:MX), INTEGER*1, 0-based,
/// iobdp_size = MX (its extent; :1672 sets the whole array). dw is W3ADATMD's DW(0:NSEA) with
/// dw[k] = DW(k), so dw[ip] for ip = 1..nx is DW(IP) as the Fortran indexes it (by node, not by
/// sea point: see the port report).
KOKKOS_INLINE_FUNCTION void setdepth(float dmin, std::int8_t* iobdp, int iobdp_size, int nx,
                                     const float* dw) {
  // :1672.
  for (int i = 0; i < iobdp_size; ++i) iobdp[i] = 1;
  // :1674. The sum is double; the object hoists it out of the loop (same bits).
  const double thr = static_cast<double>(dmin) + setdepth_detail::kDthr;
  // :1673-1676. A NaN depth compares false and keeps IOBDP = 1, as comisd/jbe does.
  for (int ip = 1; ip <= nx; ++ip) {
    if (static_cast<double>(dw[ip]) < thr) iobdp[ip - 1] = 0;
  }
}

}  // namespace ww::profs
