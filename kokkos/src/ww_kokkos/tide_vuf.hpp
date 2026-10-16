// kokkos/src/ww_kokkos/tide_vuf.hpp
// VUF: the nodal corrections V, u and f of one tidal constituent at one time, looked up by
// name in the tables SETVUF filled. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3tidemd.F90:1706-1746 at NOAA-EMC/WW3 03adda91;
// authors M. Foreman (tidal package), adapted to WW3 by F. Ardhuin.
// Evidence: kokkos/port_report/VUF_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::tide {

namespace vuf_detail {
/// Returned where the Fortran writes 'ERROR IN VUF: STOP.' with KONX on NDSET and stops
/// (:1728-1731): the shim does the WRITE and the STOP.
inline constexpr int kStop = 1;
}  // namespace vuf_detail

/// VUF. konx: one CHARACTER*5 name, 5 bytes, no terminator. vx, ux, fx: REAL(8) outputs.
/// itime: the 1-based time index. Then the W3TIDEMD state it reads: ntotal_con;
/// tidecon_allnames (NTOTAL_CON) as consecutive 5-byte records; v_arg, u_arg, f_arg
/// (REAL, column major, element (k,itime) at (k-1) + ld_arg*(itime-1)); ld_arg is their
/// first extent, 170 in both allocations (ww3_prnc.F90:1522, ww3_shel.F90:1966).
/// The first matching name wins (RETURN at :1726). Returns 0, or kStop with vx, ux, fx not
/// written.
KOKKOS_INLINE_FUNCTION int vuf(const char* konx, double& vx, double& ux, double& fx, int itime,
                               int ntotal_con, const char* tidecon_allnames, const float* v_arg,
                               const float* u_arg, const float* f_arg, long ld_arg) {
  // :1721-1727. Equal-length CHARACTER .eq. is a 5-byte memcmp_eq in the object.
  for (int k = 1; k <= ntotal_con; ++k) {
    const char* name = tidecon_allnames + 5L * (k - 1);
    bool eq = true;
    for (int c = 0; c < 5; ++c) eq = eq && (name[c] == konx[c]);
    if (eq) {
      const long at = static_cast<long>(k - 1) + ld_arg * static_cast<long>(itime - 1);
      vx = static_cast<double>(v_arg[at]);
      ux = static_cast<double>(u_arg[at]);
      fx = static_cast<double>(f_arg[at]);
      return 0;
    }
  }
  return vuf_detail::kStop;
}

}  // namespace ww::tide
