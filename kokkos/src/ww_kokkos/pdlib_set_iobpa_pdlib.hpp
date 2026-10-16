// kokkos/src/ww_kokkos/pdlib_set_iobpa_pdlib.hpp
// SET_IOBPA_PDLIB: the active-boundary flag of the PDLIB nodes. For each local sea point
// JSEA = 1..NSEAL, IOBPA_LOC(JSEA) = 1 when the status MAPSTA(1,IPLG(JSEA)) of its global node
// is 2, 0 otherwise. Integer compare and store only, no floating point. Phase 1, translated, not
// regtested.
// WW3 heritage: WW3/model/src/w3profsmd_pdlib.F90:6978-7061 at h0ffmann/WW3 8e433f99, text
// identical at NOAA-EMC/WW3 03adda91; A. Roland (BGS IT&E GmbH), M. Dutour-Sikiric (IRB).
// Evidence: kokkos/port_report/SET_IOBPA_PDLIB_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement WW3/regtests/ww3_tp2.17/input/switch_PDLIB compiles (no S). Not
// ported: W3_S (USE STRACE, :7023-7025; IENT, :7038-7040; CALL STRACE, :7047-7049). DTHR
// (:7046) and INIT_GET_ISEA (:7028) are declared and never used. No callee, no MPI, no stop: no
// hook table and no exit code.
//
// PDLIB_IOBP_INIT's set_iobpa_pdlib hook (pdlib_pdlib_iobp_init.hpp) takes only ctx, as the
// Fortran takes no argument; the shim behind that hook calls this function with W3GDATMD's
// NSEAL, MAPSTA and IOBPA_LOC and yowNodepool's iplg.
//
// The loop runs to NSEAL and IOBPA_LOC is allocated NPA (:602): the Fortran relies on
// NSEAL <= NPA and checks nothing, and so does this.
// Structural change, no arithmetic: the JSEA loop (:7050-7057) is one parallel_for; each element
// of IOBPA_LOC is written once from its own JSEA and the sources are not written, so the order
// cannot change a bit.
#pragma once

#include <Kokkos_Core.hpp>

#include <cstdint>

namespace ww::pdlib {

/// The scalar the routine reads: W3GDATMD NSEAL (w3gdatmd.F90:1100, pointed at
/// GRIDS(IMOD)%NSEAL by W3SETG, :2292).
struct SetIobpaConfig {
  int nseal = 0;
};

/// @brief SET_IOBPA_PDLIB, w3profsmd_pdlib.F90:6978-7061, under switch_PDLIB.
/// @param c         NSEAL.
/// @param iplg      yowNodepool iplg (PDLIB/yownodepool.F90:116), index 0 is JSEA = 1, the
///                  stored global node ids 1-based.
/// @param mapsta    W3GDATMD MAPSTA(MY,MX), default INTEGER (w3gdatmd.F90:1166, allocated at
///                  :1733), LayoutLeft, mapsta(0, k - 1) is MAPSTA(1,k).
/// @param iobpa_loc W3GDATMD IOBPA_LOC, INTEGER*1 (w3gdatmd.F90:1122), index 0 is JSEA = 1.
/// @pre iplg and iobpa_loc have at least NSEAL elements and every IPLG(JSEA) is in
///      1..extent(1) of mapsta, as in the Fortran, which checks none of them.
template <class ExecSpace>
void set_iobpa_pdlib(
    const SetIobpaConfig& c,
    const Kokkos::View<const int*, Kokkos::LayoutLeft, typename ExecSpace::memory_space>& iplg,
    const Kokkos::View<const int**, Kokkos::LayoutLeft, typename ExecSpace::memory_space>& mapsta,
    const Kokkos::View<std::int8_t*, Kokkos::LayoutLeft, typename ExecSpace::memory_space>&
        iobpa_loc) {
  if (c.nseal <= 0) return;  // DO JSEA=1,NSEAL with no trip
  Kokkos::parallel_for(
      "ww::pdlib::set_iobpa_pdlib", Kokkos::RangePolicy<ExecSpace>(1, c.nseal + 1),
      KOKKOS_LAMBDA(const int jsea) {
        const int ip_glob = iplg(jsea - 1);       // :7051
        if (mapsta(0, ip_glob - 1) == 2) {        // :7052
          iobpa_loc(jsea - 1) = 1;  // :7053
        } else {
          iobpa_loc(jsea - 1) = 0;  // :7055
        }
      });
  ExecSpace().fence();
}

}  // namespace ww::pdlib
