// kokkos/src/ww_kokkos/pdlib_pdlib_iobp_init.hpp
// PDLIB_IOBP_INIT: allocates the per-rank PDLIB boundary flags IOBP_LOC(NPA), IOBPD_LOC(NTH,NPA),
// IOBDP_LOC(NPA), IOBPA_LOC(NPA) of GRIDS(IMOD), points W3GDATMD's module pointers at them,
// copies IOBP and IOBPD from the global node IPLG(IP) of each local node IP, zeroes IOBDP_LOC,
// releases the global IOBP and IOBPD, and calls SET_IOBPA_PDLIB. Integer copies only, no
// floating point. Phase 1, translated, not regtested.
// WW3 heritage: WW3/model/src/w3profsmd_pdlib.F90:514-624 at h0ffmann/WW3 8e433f99, text
// identical at NOAA-EMC/WW3 03adda91; A. Roland (BGS IT&E GmbH), M. Dutour-Sikiric (IRB).
// Evidence: kokkos/port_report/PDLIB_IOBP_INIT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement WW3/regtests/ww3_tp2.17/input/switch_PDLIB compiles (no S, no
// DEBUGINIT). Not ported: W3_S (USE STRACE, :559-561; IENT, :577-579; CALL STRACE, :586-588)
// and W3_DEBUGINIT (the WRITE, :589-591). Not translated, behind IobpInitHooks: SET_IOBPA_PDLIB
// (:620; :6978-7061).
//
// Errors: PDLIB_ABORT(istat) prints and ends in the STOP of yowerr.F90:159
// (PDLIB/yowfunction.F90:47-53), and gfortran's failed DEALLOCATE ends the run with status 2.
// Here the code is returned and the caller (the shim) prints and stops, as BLOCK_SOLVER_INIT and
// DEALLOCATE_PDLIB_GLOBAL do: a host routine of the port never prints or stops.
//
// Each ALLOCATE(..., stat=istat) (:596-603) is its own statement: a failed allocation leaves the
// array unallocated and PDLIB_ABORT follows with that statement's code. The arrays are POINTER
// components, so gfortran does not test whether they are already associated; a second call
// replaces them (the Fortran leaks the old storage, a View releases its reference).
// DEALLOCATE without stat (:619) is a runtime error on a disassociated IOBP or IOBPD, IOBP
// tested (and released) first, as the object does.
// Structural change, no arithmetic: the IP loop (:610-614) is one parallel_for; each element of
// IOBP_LOC and IOBPD_LOC is written once from one source word, and the sources are not written,
// so the order cannot change a bit.
#pragma once

#include <Kokkos_Core.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>

namespace ww::pdlib {

/// The scalars the routine reads: argument IMOD; W3ODATMD IAPROC, NAPROC (w3odatmd.F90:444);
/// yowNodepool npa (PDLIB/yownodepool.F90:99); W3GDATMD NTH (w3gdatmd.F90:1234).
struct IobpInitConfig {
  int imod = 1;
  int iaproc = 1, naproc = 1;
  int npa = 0;
  int nth = 0;
};

/// The components of GRIDS(IMOD) the routine touches (w3gdatmd.F90:759-762, :774-775).
/// IOBP(MX) INTEGER*2 and IOBPD(NTH,MX) INTEGER*1 come from W3DIMUG (:3256-3257) and are
/// released here; the four *_LOC arrays are allocated here. LayoutLeft, index 0 is Fortran
/// index 1. The *_associated flags are what DEALLOCATE tests (the descriptor's data pointer);
/// a View of extent 0 does not carry it by itself.
template <class MemSpace>
struct IobpGridArrays {
  using memory_space = MemSpace;
  template <class T>
  using V = Kokkos::View<T, Kokkos::LayoutLeft, MemSpace>;

  V<std::int16_t*> iobp;       ///< GRIDS(IMOD)%IOBP(MX), INTEGER*2
  V<std::int8_t**> iobpd;      ///< GRIDS(IMOD)%IOBPD(NTH,MX), INTEGER*1
  bool iobp_associated = false;
  bool iobpd_associated = false;
  V<std::int16_t*> iobp_loc;   ///< GRIDS(IMOD)%IOBP_LOC(NPA), INTEGER*2
  V<std::int8_t**> iobpd_loc;  ///< GRIDS(IMOD)%IOBPD_LOC(NTH,NPA), INTEGER*1
  V<std::int8_t*> iobdp_loc;   ///< GRIDS(IMOD)%IOBDP_LOC(NPA), INTEGER*1
  V<std::int8_t*> iobpa_loc;   ///< GRIDS(IMOD)%IOBPA_LOC(NPA), INTEGER*1
};

/// W3GDATMD's module pointers (w3gdatmd.F90:1119-1122, :1132-1133). iobp and iobpd are read
/// as associated at entry (W3SETG points them at GRIDS(IMOD)%IOBP, IOBPD, :2412, :2422) and
/// nullified (:617-618); the four *_loc are associated with GRIDS(IMOD)'s here (:605-608).
template <class MemSpace>
struct IobpModulePointers {
  using memory_space = MemSpace;
  template <class T>
  using V = Kokkos::View<T, Kokkos::LayoutLeft, MemSpace>;

  V<std::int16_t*> iobp;
  V<std::int8_t**> iobpd;
  V<std::int16_t*> iobp_loc;
  V<std::int8_t**> iobpd_loc;
  V<std::int8_t*> iobdp_loc;
  V<std::int8_t*> iobpa_loc;
};

/// What pdlib_iobp_init returns: 0, the Fortran returned; 7, 8, 9, the istat of the
/// PDLIB_ABORT(istat) the Fortran calls and stops in (IOBDP_LOC and IOBPA_LOC share 9); 2, the
/// status gfortran exits with after "At line 619 of file w3profsmd_pdlib.F90" and "Fortran
/// runtime error: Attempt to DEALLOCATE unallocated 'grids'", which the caller writes.
inline constexpr int kIobpInitOk = 0;
inline constexpr int kIobpInitAllocIobpLoc = 7;   ///< PDLIB_ABORT(7), :597
inline constexpr int kIobpInitAllocIobpdLoc = 8;  ///< PDLIB_ABORT(8), :599
inline constexpr int kIobpInitAllocIobdpLoc = 9;  ///< PDLIB_ABORT(9), :601
inline constexpr int kIobpInitAllocIobpaLoc = 9;  ///< PDLIB_ABORT(9), :603
inline constexpr int kIobpInitDeallocate = 2;     ///< runtime error of the DEALLOCATE at :619
inline constexpr int kIobpInitDeallocateLine = 619;

/// The call that stays in Fortran, filled by a future shim. set_iobpa_pdlib takes no argument,
/// as in the Fortran: it writes IOBPA_LOC through the module pointer associated here, which the
/// shim reaches through ctx. A hook needed on a path and left null aborts.
struct IobpInitHooks {
  void* ctx = nullptr;
  void (*set_iobpa_pdlib)(void* ctx) = nullptr;  ///< SET_IOBPA_PDLIB, :620
};

namespace pii_detail {

/// ALLOCATE of one POINTER with stat=: false when the allocation throws, the case in which
/// gfortran's malloc returns NULL and istat is set. Extents below 0 are 0, as the object's
/// MAX_EXPR; the Fortran mallocs one byte for an empty array, a View none.
template <class View, class... N>
bool allocate_pointer(View& v, const char* label, N... n) {
  try {
    v = View(Kokkos::view_alloc(Kokkos::WithoutInitializing, std::string(label)),
             static_cast<std::size_t>(n > 0 ? n : 0)...);
  } catch (const std::exception&) {
    v = View();
    return false;
  }
  return true;
}

}  // namespace pii_detail

/// @brief PDLIB_IOBP_INIT(IMOD), w3profsmd_pdlib.F90:514-624, under switch_PDLIB.
/// @param c    IMOD, IAPROC, NAPROC, npa, NTH.
/// @param iplg yowNodepool iplg(1:npa) (PDLIB/yownodepool.F90:116), index 0 is IP = 1, the
///             stored global node ids 1-based.
/// @param g    GRIDS(IMOD): IOBP, IOBPD released, the four *_LOC allocated.
/// @param mp   W3GDATMD's module pointers: IOBP, IOBPD read then nullified, *_LOC associated.
/// @param h    SET_IOBPA_PDLIB.
/// @return kIobpInitOk, a PDLIB_ABORT istat or kIobpInitDeallocate; on any but the first the
///         Fortran has stopped, and g and mp are left as far as it had got.
/// @pre IPLG(IP) is in 1..extent of mp.iobp and of mp.iobpd's second dimension, and mp.iobpd's
///      first extent is at most NTH, as in the Fortran, which checks neither.
template <class ExecSpace>
[[nodiscard]] int pdlib_iobp_init(const IobpInitConfig& c,
                     const Kokkos::View<const int*, Kokkos::LayoutLeft,
                                        typename ExecSpace::memory_space>& iplg,
                     IobpGridArrays<typename ExecSpace::memory_space>& g,
                     IobpModulePointers<typename ExecSpace::memory_space>& mp,
                     const IobpInitHooks& h) {
  if (c.iaproc > c.naproc) return kIobpInitOk;  // :592-594

  // :596-603, one statement each, in this order.
  if (!pii_detail::allocate_pointer(g.iobp_loc, "IOBP_LOC", c.npa)) return kIobpInitAllocIobpLoc;
  if (!pii_detail::allocate_pointer(g.iobpd_loc, "IOBPD_LOC", c.nth, c.npa)) {
    return kIobpInitAllocIobpdLoc;
  }
  if (!pii_detail::allocate_pointer(g.iobdp_loc, "IOBDP_LOC", c.npa)) return kIobpInitAllocIobdpLoc;
  if (!pii_detail::allocate_pointer(g.iobpa_loc, "IOBPA_LOC", c.npa)) return kIobpInitAllocIobpaLoc;

  // :605-608
  mp.iobp_loc = g.iobp_loc;
  mp.iobpa_loc = g.iobpa_loc;
  mp.iobpd_loc = g.iobpd_loc;
  mp.iobdp_loc = g.iobdp_loc;

  // :610-614. IOBPD_loc(:,IP) = IOBPD(:,IP_glob) runs over IOBPD's first extent, as the object
  // reads it from IOBPD's descriptor; W3DIMUG allocates it NTH (w3gdatmd.F90:3257). gfortran
  // copies the section through a malloc'd temporary because the two pointers may alias; the
  // destination was allocated above and cannot overlap the source, so the copy is direct.
  if (c.npa >= 1) {
    const auto iobp = mp.iobp;
    const auto iobpd = mp.iobpd;
    const auto iobp_loc = mp.iobp_loc;
    const auto iobpd_loc = mp.iobpd_loc;
    const int nsec = static_cast<int>(iobpd.extent(0));
    Kokkos::parallel_for(
        "ww::pdlib::pdlib_iobp_init", Kokkos::RangePolicy<ExecSpace>(1, c.npa + 1),
        KOKKOS_LAMBDA(const int ip) {
          const int ip_glob = iplg(ip - 1);
          iobp_loc(ip - 1) = iobp(ip_glob - 1);
          for (int k = 0; k < nsec; ++k) iobpd_loc(k, ip - 1) = iobpd(k, ip_glob - 1);
        });
    ExecSpace().fence();
  }

  Kokkos::deep_copy(ExecSpace(), mp.iobdp_loc, std::int8_t{0});  // IOBDP_loc = 0, :616
  ExecSpace().fence();

  mp.iobp = {};   // IOBP => NULL(), :617
  mp.iobpd = {};  // IOBPD => NULL(), :618
  // DEALLOCATE(GRIDS(IMOD)%IOBP, GRIDS(IMOD)%IOBPD), :619: a NULL IOBPD is found after IOBP
  // has been freed and cleared.
  if (!g.iobp_associated) return kIobpInitDeallocate;
  g.iobp = {};
  g.iobp_associated = false;
  if (!g.iobpd_associated) return kIobpInitDeallocate;
  g.iobpd = {};
  g.iobpd_associated = false;

  if (h.set_iobpa_pdlib == nullptr) Kokkos::abort("PDLIB_IOBP_INIT: set_iobpa_pdlib hook missing");
  h.set_iobpa_pdlib(h.ctx);  // :620
  return kIobpInitOk;
}

}  // namespace ww::pdlib
