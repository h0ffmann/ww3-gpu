// kokkos/src/ww_kokkos/iogo_w3flgrdupdt.hpp
// W3FLGRDUPDT: switch off the output fields whose frequency bounds the mod_def file does not
// set (E3DF, US3DF, USSPF, P2MSF), and set the group flags of groups 3 and 6 from what is left.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3iogomd.F90:179-318 at NOAA-EMC/WW3 03adda91;
// author F. Ardhuin. Evidence: kokkos/port_report/W3FLGRDUPDT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <Kokkos_Core.hpp>

namespace ww::iogo {

namespace flgrdupdt_detail {
// w3odatmd.F90:330-331, PARAMETERs.
inline constexpr int kNogrp = 10;
inline constexpr int kNgrpp = 20;

// Bits of the return value, one per WRITE(NDSEN,1008) site, in the order the Fortran writes
// them. Bit i-1 (i = 1..5): VARNAME1(i), VARNAME2(i) of :265-269 at :274.
inline constexpr int kWarnUs3d = 1 << 5;  // 'USF','US3D', :282
inline constexpr int kWarnUssp = 1 << 6;  // 'USP','USSP', :289
inline constexpr int kWarnP2sf = 1 << 7;  // 'P2L','P2SF', :296

// FLGRD(IG,IP), FLGR2(IG,IP): column-major NOGRP x NGRPP, 1-based in Fortran.
KOKKOS_INLINE_FUNCTION int at(int ig, int ip) { return (ig - 1) + (ip - 1) * kNogrp; }
}  // namespace flgrdupdt_detail

/// W3FLGRDUPDT, :179-318. Returns a mask of flgrdupdt_detail::kWarn* bits (bits 0-4 for
/// E3DF's five fields): the shim writes format 1008 (:311-314) to NDSEN once per set bit, in
/// bit order, which is the Fortran's order. NDSO is unused by the Fortran.
/// flgrd, flgr2: FLGRD(NOGRP,NGRPP), FLGR2(NOGRP,NGRPP), LOGICAL(4), column-major, 0-based.
/// flgd, flg2: FLGD(NOGRP), FLG2(NOGRP), LOGICAL(4), 0-based. A LOGICAL(4) is tested as the
/// whole 32-bit word against 0 and written as 0 or 1, as the object does.
/// Module state (w3gdatmd.F90:715): e3df E3DF(3,5) column-major, 0-based; p2msf P2MSF(3);
/// us3df US3DF(3); usspf USSPF(2).
KOKKOS_INLINE_FUNCTION int w3flgrdupdt(std::int32_t* flgrd, std::int32_t* flgr2,
                                       std::int32_t* flgd, std::int32_t* flg2, const int* e3df,
                                       const int* p2msf, const int* us3df, const int* usspf) {
  using flgrdupdt_detail::at;
  using flgrdupdt_detail::kNgrpp;
  int warn = 0;

  // :271-279; E3DF(K,I) at e3df[(K-1) + 3*(I-1)].
  for (int i = 1; i <= 5; ++i) {
    const int* e = e3df + 3 * (i - 1);
    if (e[0] <= 0 || e[2] < e[1]) {
      if (flgrd[at(3, i)] != 0 || flgr2[at(3, i)] != 0) warn |= 1 << (i - 1);
      flgrd[at(3, i)] = 0;
      flgr2[at(3, i)] = 0;
    }
  }
  // :280-286
  if (us3df[0] <= 0 || us3df[2] < us3df[1]) {
    if (flgrd[at(6, 8)] != 0 || flgr2[at(6, 8)] != 0) warn |= flgrdupdt_detail::kWarnUs3d;
    flgrd[at(6, 8)] = 0;
    flgr2[at(6, 8)] = 0;
  }
  // :287-293
  if (usspf[0] <= 0 || usspf[1] <= 0) {
    if (flgrd[at(6, 12)] != 0 || flgr2[at(6, 12)] != 0) warn |= flgrdupdt_detail::kWarnUssp;
    flgrd[at(6, 12)] = 0;
    flgr2[at(6, 12)] = 0;
  }
  // :294-300
  if (p2msf[0] <= 0 || p2msf[2] < p2msf[1]) {
    if (flgrd[at(6, 9)] != 0 || flgr2[at(6, 9)] != 0) warn |= flgrdupdt_detail::kWarnP2sf;
    flgrd[at(6, 9)] = 0;
    flgr2[at(6, 9)] = 0;
  }

  // :302-309; ANY(FLGRD(IG,:)) stops at the first non-zero element, as the object's loop does.
  for (int g = 0; g < 2; ++g) {
    const int ig = (g == 0) ? 3 : 6;
    flgd[ig - 1] = 0;
    flg2[ig - 1] = 0;
    for (int ip = 1; ip <= kNgrpp; ++ip) {
      if (flgrd[at(ig, ip)] != 0) {
        flgd[ig - 1] = 1;
        break;
      }
    }
    for (int ip = 1; ip <= kNgrpp; ++ip) {
      if (flgr2[at(ig, ip)] != 0) {
        flg2[ig - 1] = 1;
        break;
      }
    }
  }
  return warn;
}

}  // namespace ww::iogo
