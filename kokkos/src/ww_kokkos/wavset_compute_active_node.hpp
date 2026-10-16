// kokkos/src/ww_kokkos/wavset_compute_active_node.hpp
// COMPUTE_ACTIVE_NODE: flags the nodes of an unstructured (PDLIB) grid deep enough for the wave
// setup computation, ACTIVE = 1 where DWNX >= CRIT_DEP_STP. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3wavset.F90:1493-1588 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, M. Dutour-Sikiric.
// Evidence: kokkos/port_report/COMPUTE_ACTIVE_NODE_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::wavset {

/// dwnx: Fortran DWNX(npa), REAL(rkind) with rkind = 4 (yowdatapool.F90:48), 1-based there,
/// 0-based here; active: ACTIVE(npa), same bases. npa is yowNodepool NPA; crit_dep_stp is
/// W3GDATMD CRIT_DEP_STP, REAL(8) (w3gdatmd.F90:1413).
/// The comparison is in double: the object widens DWNX(IP) (cvtps2pd) and tests it against
/// CRIT_DEP_STP with an ordered >=, so a NaN depth gives 0 (:1571-1581).
/// The W3_DEBUGSTP blocks (:1562-1570, :1577-1579, :1582-1587) only count and write; not ported.
KOKKOS_INLINE_FUNCTION void compute_active_node(const float* dwnx, int* active, int npa,
                                                double crit_dep_stp) {
  for (int ip = 0; ip < npa; ++ip) {
    int eact;
    if (static_cast<double>(dwnx[ip]) >= crit_dep_stp) {
      eact = 1;
    } else {
      eact = 0;
    }
    active[ip] = eact;
  }
}

}  // namespace ww::wavset
