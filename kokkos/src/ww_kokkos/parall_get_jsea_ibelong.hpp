// kokkos/src/ww_kokkos/parall_get_jsea_ibelong.hpp
// GET_JSEA_IBELONG: the local sea index JSEA of a global sea point ISEA and whether this
// process owns it, for the round-robin split and the PDLIB domain decomposition.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3parall.F90:1265-1384 at NOAA-EMC/WW3 03adda91;
// authors Aron Roland (BGS IT&E GmbH), Mathieu Dutour-Sikiric (IRB).
// Evidence: kokkos/port_report/GET_JSEA_IBELONG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::parall {

/// GET_JSEA_IBELONG(ISEA, JSEA, IBELONG); isea and jsea are 1-based as in the Fortran.
/// pdlib: whether the build has W3_PDLIB. lpdlib: CONSTANTS LPDLIB; gtype: W3GDATMD GTYPE
/// (UNGTYPE = 3, w3gdatmd.F90:627). iaproc, naproc: W3ODATMD (1 and 1 under W3_SHRD).
/// Read only when pdlib, lpdlib, gtype == 3 and iaproc <= naproc, else they may be null:
///   mapsf_ix: W3GDATMD MAPSF(1:MSEA,1), its first column, 0-based (MAPSF(ISEA,1) is mapsf_ix[isea-1]);
///   ipgl_npa: yowRankModule IPGL_NPA(1:NP_GLOBAL), 0-based (PDLIB/yowrankModule.F90:227-233);
///   jx_to_jsea: W3PARALL JX_TO_JSEA(1:NPA), 0-based (w3profsmd_pdlib.F90:289-305).
/// All of these come from the caller's MPI and PDLIB set-up and are not computed here.
KOKKOS_INLINE_FUNCTION void get_jsea_ibelong(int isea, int& jsea, int& ibelong, bool pdlib,
                                             bool lpdlib, int gtype, int iaproc, int naproc,
                                             const int* mapsf_ix, const int* ipgl_npa,
                                             const int* jx_to_jsea) {
  constexpr int ungtype = 3;
  if (!lpdlib) {
    // :1345-1352
    jsea = 1 + (isea - 1) / naproc;
    const int isproc = isea - (jsea - 1) * naproc;
    ibelong = (isproc == iaproc) ? 1 : 0;
    return;
  }
  // ⚠ :1353-1380: without W3_PDLIB and LPDLIB true the Fortran leaves the INTENT(OUT) JSEA
  // and IBELONG unassigned; they are left as the caller passed them.
  if (!pdlib) return;
  if (gtype != ungtype) {
    // :1355-1362
    jsea = 1 + (isea - 1) / naproc;
    const int isproc = isea - (jsea - 1) * naproc;
    ibelong = (isproc == iaproc) ? 1 : 0;
  } else if (iaproc <= naproc) {
    // :1364-1373
    const int ix = mapsf_ix[isea - 1];
    const int jx = ipgl_npa[ix - 1];
    if (jx == 0) {
      ibelong = 0;
      jsea = -1;
    } else {
      ibelong = 1;
      jsea = jx_to_jsea[jx - 1];
    }
  } else {
    // :1374-1377
    ibelong = 0;
    jsea = -1;
  }
}

}  // namespace ww::parall
