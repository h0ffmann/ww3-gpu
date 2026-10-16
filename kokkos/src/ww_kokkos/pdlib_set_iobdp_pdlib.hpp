// kokkos/src/ww_kokkos/pdlib_set_iobdp_pdlib.hpp
// SET_IOBDP_PDLIB: the per-rank wet/dry depth flag of the PDLIB nodes. For each local node
// IP = 1..npa, IOBDP_LOC(IP) = 0 when the depth DW of its global node IPLG(IP) is below
// DMIN + DTHR, 1 otherwise. Phase 1, translated, not regtested.
// WW3 heritage: WW3/model/src/w3profsmd_pdlib.F90:6890-6975 at h0ffmann/WW3 8e433f99, text
// identical at NOAA-EMC/WW3 03adda91; A. Roland (BGS IT&E GmbH), M. Dutour-Sikiric (IRB).
// Evidence: kokkos/port_report/SET_IOBDP_PDLIB_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement WW3/regtests/ww3_tp2.17/input/switch_PDLIB compiles (no S). Not
// ported: W3_S (USE STRACE, :6935-6937; IENT, :6951-6953; CALL STRACE, :6960-6962). No callee,
// no MPI, no stop: no hook table and no exit code.
//
// The comparison is in double, as the object does it: DMIN (REAL) widened, plus DTHR (REAL*8
// from a default-REAL literal), compared with DW (REAL) widened. The object computes the sum
// once before the loop (cvtss2sd, addsd) and sets the flag with comisd/setbe, so a NaN depth
// gives 1.
// Structural change, no arithmetic: the IP loop (:6963-6971) is one parallel_for; each element
// of IOBDP_LOC is written once from its own IP and the sources are not written, so the order
// cannot change a bit.
#pragma once

#include <Kokkos_Core.hpp>

#include <cstdint>

namespace ww::pdlib {

namespace sip_detail {
// :6959, REAL*8 DTHR = 10E-6: the literal is default REAL, widened to double after rounding;
// the object adds 9.99999974737875163555145263671875e-6.
inline constexpr double kDthr = static_cast<double>(10E-6f);
static_assert(kDthr == 0x1.4f8b58p-17, "DTHR is (double)(float)1e-5");
}  // namespace sip_detail

/// The scalars the routine reads: yowNodepool npa (PDLIB/yownodepool.F90:99) and W3GDATMD DMIN
/// (REAL, w3gdatmd.F90:1187, pointed at GRIDS(IMOD)%DMIN by W3SETG, :2353).
struct SetIobdpConfig {
  int npa = 0;
  float dmin = 0.0f;
};

/// @brief SET_IOBDP_PDLIB, w3profsmd_pdlib.F90:6890-6975, under switch_PDLIB.
/// @param c         npa, DMIN.
/// @param iplg      yowNodepool iplg(1:npa) (PDLIB/yownodepool.F90:116), index 0 is IP = 1, the
///                  stored global node ids 1-based.
/// @param dw        W3ADATMD DW(0:NSEA) (w3adatmd.F90:587, allocated at :1004), dw(k) = DW(k):
///                  dw(ip_glob) is DW(IP_glob) as the Fortran indexes it, by global node.
/// @param iobdp_loc W3GDATMD IOBDP_LOC(1:npa), INTEGER*1 (w3gdatmd.F90:1121), index 0 is IP = 1.
/// @pre IPLG(IP) is in 0..extent of dw - 1 and iobdp_loc has at least npa elements, as in the
///      Fortran, which checks neither.
template <class ExecSpace>
void set_iobdp_pdlib(
    const SetIobdpConfig& c,
    const Kokkos::View<const int*, Kokkos::LayoutLeft, typename ExecSpace::memory_space>& iplg,
    const Kokkos::View<const float*, Kokkos::LayoutLeft, typename ExecSpace::memory_space>& dw,
    const Kokkos::View<std::int8_t*, Kokkos::LayoutLeft, typename ExecSpace::memory_space>&
        iobdp_loc) {
  if (c.npa <= 0) return;  // DO JSEA=1,NPA with no trip
  // DMIN + DTHR in double, read once before the loop as the object does.
  const double thr = static_cast<double>(c.dmin) + sip_detail::kDthr;
  Kokkos::parallel_for(
      "ww::pdlib::set_iobdp_pdlib", Kokkos::RangePolicy<ExecSpace>(1, c.npa + 1),
      KOKKOS_LAMBDA(const int jsea) {
        const int ip = jsea;               // :6964
        const int ip_glob = iplg(ip - 1);  // :6965
        if (static_cast<double>(dw(ip_glob)) < thr) {  // :6966
          iobdp_loc(ip - 1) = 0;  // :6967
        } else {
          iobdp_loc(ip - 1) = 1;  // :6969
        }
      });
  ExecSpace().fence();
}

}  // namespace ww::pdlib
