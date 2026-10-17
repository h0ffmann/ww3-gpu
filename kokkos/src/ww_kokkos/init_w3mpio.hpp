// kokkos/src/ww_kokkos/init_w3mpio.hpp
// W3MPIO: set up the MPI persistent communication of WW3's output routines (W3IOGO, W3IORS,
// W3IOBC, W3IOTR) for model IMOD. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3initmd.F90:2045-5384 at NOAA-EMC/WW3 03adda91, identical
// in the h0ffmann/WW3 fork at the WW3/ pin 8e433f99 (its three w3initmd.F90 hunks, at :436,
// :473 and :733 of the base, are inside W3INIT, behind W3_KOKKOS); author H. L. Tolman.
// Evidence: kokkos/port_report/W3MPIO_KOKKOS.md.
//
// Ported as switches/switch_lab_shrd compiles it. Every statement of the body (:2216-5326) and
// every local but IMOD sit under #ifdef W3_MPI, W3_MPIT or W3_S, none of which that switch
// defines, so the preprocessed routine is USE EXTCDE, USE UNGTYPE, the dummy IMOD and RETURN
// (:2136, :2182, :2191, :5328); neither USE'd name is referenced. The -O3 object is one `ret`
// (__w3initmd_MOD_w3mpio, 1 byte), and its GIMPLE is `return;`. Nothing reaches it under that
// switch either: its only caller, W3INIT (:1559-1563), is under #ifdef W3_MPI too.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdint>

namespace ww::init {

/// W3MPIO(IMOD), :2045-5384, as switch_lab_shrd compiles it: no operation. imod is
/// INTEGER, INTENT(IN) (:2191) and unread, as in the Fortran. Host only; no hook, since the
/// compiled routine calls nothing. The W3_MPI body (MPI_SEND_INIT/MPI_RECV_INIT requests,
/// W3XDMA, W3XETA, W3SETA, EXTCDE(10) and EXTCDE(11)) is not ported (port report, "Not ported").
inline void w3mpio(std::int32_t imod) { (void)imod; }

}  // namespace ww::init
