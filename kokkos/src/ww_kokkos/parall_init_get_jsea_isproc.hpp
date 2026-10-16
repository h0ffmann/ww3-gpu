// kokkos/src/ww_kokkos/parall_init_get_jsea_isproc.hpp
// INIT_GET_JSEA_ISPROC: the local sea index JSEA and the owning rank ISPROC of the global sea
// point ISEA, for the round-robin split and for the PDLIB domain decomposition.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3parall.F90:1152-1252 at NOAA-EMC/WW3 03adda91;
// authors Aron Roland (BGS IT&E GmbH), Mathieu Dutour-Sikiric (IRB).
// Evidence: kokkos/port_report/INIT_GET_JSEA_ISPROC_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::parall {

/// INIT_GET_JSEA_ISPROC(ISEA, JSEA, ISPROC); isea is 1-based, jsea and isproc come back
/// 1-based as in the Fortran.
/// lpdlib: CONSTANTS LPDLIB under W3_PDLIB; false in a build without W3_PDLIB, and then the
/// arrays are not read and may be null.
/// The PDLIB arrays are module state the caller passes, 0-based:
///   mapsf_col1    MAPSF(1:NSEA,1)           (W3GDATMD), the global node of each sea point
///   isea_to_jsea  ISEA_TO_JSEA(1:NSEA)      (W3PARALL, filled in w3profsmd_pdlib.F90:289-304)
///   ipgl_to_proc  IPGL_TO_PROC(1:np_global) (yowRankModule, PDLIB/yowrankModule.F90:218-223)
/// They are filled by MPI exchanges at set-up; here they are inputs, as are naproc and iaproc.
KOKKOS_INLINE_FUNCTION void init_get_jsea_isproc(int isea, int& jsea, int& isproc, int naproc,
                                                 bool lpdlib, int iaproc, const int* mapsf_col1,
                                                 const int* isea_to_jsea,
                                                 const int* ipgl_to_proc) {
  if (!lpdlib) {
    // :1236-1237; integer division truncates toward zero in both languages (idiv in both objects).
    const int j = 1 + (isea - 1) / naproc;
    jsea = j;
    isproc = isea - (j - 1) * naproc;
  } else {
    // :1240; MAPSF(ISEA,1) is read before the IAPROC test, as in the object.
    const int ip_glob = mapsf_col1[isea - 1];
    if (iaproc <= naproc) {
      jsea = isea_to_jsea[isea - 1];
    } else {
      jsea = -1;
    }
    isproc = ipgl_to_proc[ip_glob - 1];
  }
}

}  // namespace ww::parall
