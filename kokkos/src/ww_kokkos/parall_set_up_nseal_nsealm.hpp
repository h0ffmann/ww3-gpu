// kokkos/src/ww_kokkos/parall_set_up_nseal_nsealm.hpp
// SET_UP_NSEAL_NSEALM: the number of local sea points NSEAL of this rank and its maximum over
// ranks NSEALM, for the shared-memory build, the round-robin split and the PDLIB decomposition.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3parall.F90:1030-1139 at NOAA-EMC/WW3 03adda91;
// authors Aron Roland (BGS IT&E GmbH), Mathieu Dutour-Sikiric (IRB).
// Evidence: kokkos/port_report/SET_UP_NSEAL_NSEALM_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::parall {

/// SET_UP_NSEAL_NSEALM(NSEALout, NSEALMout).
/// dist, pdlib: whether the build has W3_DIST and W3_PDLIB (W3_SHRD when dist is false; PDLIB
/// requires MPI, which requires DIST, model/src/cmake/switches.json).
/// nsea: W3GDATMD NSEA; lpdlib: CONSTANTS LPDLIB; gtype: W3GDATMD GTYPE (UNGTYPE = 3,
/// w3gdatmd.F90:627); iaproc, naproc: W3ODATMD; pdlib_nseal, pdlib_nsealm: W3PARALL PDLIB_NSEAL,
/// PDLIB_NSEALM, set by the PDLIB set-up (w3profsmd_pdlib.F90:248-279). These are read only
/// where the Fortran reads them and come from the caller's MPI set-up.
KOKKOS_INLINE_FUNCTION void set_up_nseal_nsealm(int& nseal_out, int& nsealm_out, bool dist,
                                                bool pdlib, int nsea, bool lpdlib, int gtype,
                                                int iaproc, int naproc, int pdlib_nseal,
                                                int pdlib_nsealm) {
  constexpr int ungtype = 3;
  if (!dist) {
    // :1106-1107
    nseal_out = nsea;
    nsealm_out = nsea;
    return;
  }
  if (!lpdlib) {
    // :1111-1117; integer division truncates toward zero in both languages (idiv in both).
    if (iaproc <= naproc) {
      nseal_out = 1 + (nsea - iaproc) / naproc;
    } else {
      nseal_out = 0;
    }
    nsealm_out = 1 + (nsea - 1) / naproc;
    return;
  }
  // ⚠ :1110-1134: W3_DIST without W3_PDLIB and LPDLIB true leaves both INTENT(OUT) values
  // unassigned in the Fortran; they are left as the caller passed them.
  if (pdlib) {
    if (gtype == ungtype) {
      // :1122-1123
      nseal_out = pdlib_nseal;
      nsealm_out = pdlib_nsealm;
    } else {
      // :1125-1130
      if (iaproc <= naproc) {
        nseal_out = 1 + (nsea - iaproc) / naproc;
      } else {
        nseal_out = 0;
      }
      nsealm_out = 1 + (nsea - 1) / naproc;
    }
  }
}

}  // namespace ww::parall
