// kokkos/src/ww_kokkos/parall_init_get_isea.hpp
// INIT_GET_ISEA: the global sea point ISEA of the local sea index JSEA, for the shared-memory
// build, the round-robin split and the PDLIB domain decomposition.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3parall.F90:1396-1504 at NOAA-EMC/WW3 03adda91;
// authors Aron Roland (BGS IT&E GmbH), Mathieu Dutour-Sikiric (IRB).
// Evidence: kokkos/port_report/INIT_GET_ISEA_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::parall {

/// INIT_GET_ISEA(ISEA, JSEA); jsea is 1-based and isea comes back 1-based as in the Fortran.
/// dist, pdlib: whether the build has W3_DIST and W3_PDLIB (W3_SHRD when dist is false).
/// lpdlib: CONSTANTS LPDLIB; gtype: W3GDATMD GTYPE (UNGTYPE = 3, w3gdatmd.F90:627).
/// iplg: yowNodepool IPLG(1:NPA), 0-based here, in its final allocation by the PDLIB set-up
/// (PDLIB/yowpdlibmain.F90:998-1012, resident nodes then ghosts); read only when pdlib, lpdlib and gtype == 3, else may be null.
/// naproc, iaproc are read only when dist. All of these come from the caller's MPI set-up.
KOKKOS_INLINE_FUNCTION void init_get_isea(int& isea, int jsea, bool dist, bool pdlib, bool lpdlib,
                                          int gtype, int iaproc, int naproc, const int* iplg) {
  constexpr int ungtype = 3;
  if (!dist) {
    // :1484
    isea = jsea;
    return;
  }
  if (!lpdlib) {
    // :1488
    isea = iaproc + (jsea - 1) * naproc;
    return;
  }
  // ⚠ :1486-1499: W3_DIST without W3_PDLIB and LPDLIB true leaves the INTENT(OUT) ISEA
  // unassigned in the Fortran; isea is left as the caller passed it.
  if (pdlib) {
    if (gtype == ungtype) {
      // :1493
      isea = iplg[jsea - 1];
    } else {
      // :1495
      isea = iaproc + (jsea - 1) * naproc;
    }
  }
}

}  // namespace ww::parall
