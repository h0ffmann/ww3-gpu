// kokkos/src/ww_kokkos/pdlib_pdlib_mapsta_init.hpp
// PDLIB_MAPSTA_INIT: the per-rank status map of a PDLIB unstructured grid. Allocates
// MAPSTA_LOC(npa) of GRIDS(IMOD) and fills it with MAPSTA(1,iplg(IP)) for each local node IP
// (own plus ghost); counts in NBND_MAP the global nodes IX that this rank holds and whose
// MAPSTA(1,IX) is below 1, and lists their local indices, in increasing IX, in
// INDEX_MAP(NBND_MAP). Integer work only, no floating point. Phase 1, translated, not
// regtested.
// WW3 heritage: WW3/model/src/w3profsmd_pdlib.F90:401-512 at h0ffmann/WW3 8e433f99, text
// identical at NOAA-EMC/WW3 03adda91; A. Roland (BGS IT&E GmbH), M. Dutour-Sikiric (IRB).
// Evidence: kokkos/port_report/PDLIB_MAPSTA_INIT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement WW3/regtests/ww3_tp2.17/input/switch_PDLIB compiles (no S, no
// DEBUGINIT). Not ported: W3_S (USE STRACE, :446-448; IENT, :463-465; CALL STRACE, :472-474)
// and W3_DEBUGINIT (the WRITE, :475-477). No callee is translated or hooked: the only call is
// PDLIB_ABORT (:483, :500; PDLIB/yowfunction.F90:47-53, which prints and ends in the STOP of
// PDLIB/yowerr.F90:159), and its istat is returned instead, for the shim to pass to
// PDLIB_ABORT, as the other PDLIB ports of this branch do: a host routine of the port never
// prints or stops.
//
// What the -O3 object does: the automatic array Status(NX) is malloc'd at entry, before the
// IAPROC test, and its result is not tested; it is freed on both returns. Each ALLOCATE with
// stat= (:482, :499) mallocs without testing prior association (POINTER components); a NULL
// return calls PDLIB_ABORT with 5 and 6 (.rodata words). NBND_MAP is stored in GRIDS(IMOD) and
// incremented there, one load and store per hit.
// Structural change, no arithmetic: each of the three loops (:487-491, :493-497, :503-508) runs
// in one Kokkos work item, in the Fortran's order. Status(IP_glob) = IP keeps the last IP of a
// repeated IP_glob, INDEX_MAP is filled in increasing IX, and the count is the Fortran's
// sequence of +1, so no in-bounds input can make the bytes differ; spreading the loops over
// threads is phase 2.
#pragma once

#include <Kokkos_Core.hpp>

#include <cstddef>
#include <exception>
#include <string>

namespace ww::pdlib {

/// The scalars the routine reads: argument IMOD; W3ODATMD IAPROC, NAPROC (w3odatmd.F90:444);
/// yowNodepool npa (PDLIB/yownodepool.F90:99); W3GDATMD NX (w3gdatmd.F90:648).
struct MapstaInitConfig {
  int imod = 1;
  int iaproc = 1, naproc = 1;
  int npa = 0;
  int nx = 0;
};

/// The components of GRIDS(IMOD) the routine writes (w3gdatmd.F90:756-758): NBND_MAP, and
/// INDEX_MAP(NBND_MAP) and MAPSTA_LOC(npa), allocated here. LayoutLeft, index 0 is Fortran
/// index 1; INDEX_MAP holds 1-based local node indices.
template <class MemSpace>
struct MapstaInitGrid {
  using memory_space = MemSpace;
  using I1 = Kokkos::View<int*, Kokkos::LayoutLeft, MemSpace>;
  I1 mapsta_loc;  ///< GRIDS(IMOD)%MAPSTA_LOC(npa)
  I1 index_map;   ///< GRIDS(IMOD)%INDEX_MAP(NBND_MAP)
  int nbnd_map = 0;
};

/// W3GDATMD's module pointers (w3gdatmd.F90:1116-1118, :1166). mapsta is read: it is whatever
/// W3SETG last associated (MAPSTA => GRIDS(IMOD)%MAPSTA, :2436), MAPSTA(NY,NX); the routine
/// reads row 1. mapsta_loc, index_map and nbnd_map are associated here with GRIDS(IMOD)'s
/// (:484-485, :501).
template <class MemSpace>
struct MapstaInitModulePointers {
  using memory_space = MemSpace;
  Kokkos::View<const int**, Kokkos::LayoutLeft, MemSpace> mapsta;
  Kokkos::View<int*, Kokkos::LayoutLeft, MemSpace> mapsta_loc;
  Kokkos::View<int*, Kokkos::LayoutLeft, MemSpace> index_map;
  int* nbnd_map = nullptr;  ///< NBND_MAP => GRIDS(IMOD)%NBND_MAP, a host scalar
};

/// What pdlib_mapsta_init returns: 0, the Fortran returned; otherwise the istat of the
/// PDLIB_ABORT(istat) the Fortran calls and stops in.
inline constexpr int kMapstaInitOk = 0;
inline constexpr int kMapstaInitAllocMapstaLocFailed = 5;  ///< PDLIB_ABORT(5), :483
inline constexpr int kMapstaInitAllocIndexMapFailed = 6;   ///< PDLIB_ABORT(6), :500

namespace pmi_detail {

/// One rank-1 ALLOCATE(..., stat=istat) of a POINTER, or the automatic array's malloc: false
/// when the allocation throws, the case in which malloc returns NULL. An extent below 0 is 0,
/// as the object's MAX_EXPR; the Fortran mallocs one byte for an empty array, a View none.
template <class View>
bool allocate_1d(View& v, const char* label, int n) {
  try {
    v = View(Kokkos::view_alloc(Kokkos::WithoutInitializing, std::string(label)),
             static_cast<std::size_t>(n > 0 ? n : 0));
  } catch (const std::exception&) {
    v = View();
    return false;
  }
  return true;
}

}  // namespace pmi_detail

/// @brief PDLIB_MAPSTA_INIT(IMOD), w3profsmd_pdlib.F90:401-512, under switch_PDLIB.
/// @param c    IMOD, IAPROC, NAPROC, npa, NX.
/// @param iplg yowNodepool iplg(1:npa) (PDLIB/yownodepool.F90:116), index 0 is IP = 1, the
///             stored global node ids 1-based.
/// @param g    GRIDS(IMOD): MAPSTA_LOC and INDEX_MAP allocated, NBND_MAP set.
/// @param mp   W3GDATMD's module pointers: MAPSTA read, MAPSTA_LOC, INDEX_MAP, NBND_MAP
///             associated with g's.
/// @return kMapstaInitOk, or the PDLIB_ABORT istat; on an istat the Fortran has stopped, and g
///         and mp are left as far as it had got.
/// @pre iplg(IP) is in 1..NX and in 1..mp.mapsta.extent(1), and mp.mapsta.extent(0) >= 1, as in
///      the Fortran, which checks neither.
template <class ExecSpace>
[[nodiscard]] int pdlib_mapsta_init(
    const MapstaInitConfig& c,
    const Kokkos::View<const int*, Kokkos::LayoutLeft, typename ExecSpace::memory_space>& iplg,
    MapstaInitGrid<typename ExecSpace::memory_space>& g,
    MapstaInitModulePointers<typename ExecSpace::memory_space>& mp) {
  using I1 = Kokkos::View<int*, Kokkos::LayoutLeft, typename ExecSpace::memory_space>;

  // INTEGER :: Status(NX) (:471): allocated at entry, before the IAPROC test. An automatic
  // array has no stat= and no PDLIB_ABORT code: the object does not test the malloc and then
  // writes through it, so that undefined path is an abort here, not a returned code.
  I1 status;
  const bool status_ok = pmi_detail::allocate_1d(status, "Status", c.nx);
  if (c.iaproc > c.naproc) return kMapstaInitOk;  // :478-480
  if (!status_ok) Kokkos::abort("PDLIB_MAPSTA_INIT: Status(NX) allocation failed");

  if (!pmi_detail::allocate_1d(g.mapsta_loc, "MAPSTA_LOC", c.npa)) {  // :482
    return kMapstaInitAllocMapstaLocFailed;                           // :483
  }
  mp.mapsta_loc = g.mapsta_loc;  // :484
  mp.nbnd_map = &g.nbnd_map;     // :485
  Kokkos::deep_copy(ExecSpace(), status, 0);  // Status = 0, :486

  const int npa = c.npa;
  const int nx = c.nx;
  const auto mapsta = mp.mapsta;

  {  // :487-491
    const auto mapsta_loc = mp.mapsta_loc;
    Kokkos::parallel_for(
        "ww::pdlib::pdlib_mapsta_init::local", Kokkos::RangePolicy<ExecSpace>(0, 1),
        KOKKOS_LAMBDA(const int) {
          for (int ip = 1; ip <= npa; ++ip) {
            const int ip_glob = iplg(ip - 1);
            status(ip_glob - 1) = ip;
            mapsta_loc(ip - 1) = mapsta(0, ip_glob - 1);
          }
        });
  }

  *mp.nbnd_map = 0;  // :492
  {                  // :493-497
    // A reduce over one iteration is how the count reaches the host, which sizes INDEX_MAP
    // (:499): its identity 0 is NBND_MAP = 0, and the join adds only that 0.
    int nbnd_map = 0;
    Kokkos::parallel_reduce(
        "ww::pdlib::pdlib_mapsta_init::count", Kokkos::RangePolicy<ExecSpace>(0, 1),
        KOKKOS_LAMBDA(const int, int& n) {
          for (int ix = 1; ix <= nx; ++ix) {
            if ((mapsta(0, ix - 1) < 1) && (status(ix - 1) > 0)) {
              n = n + 1;
            }
          }
        },
        nbnd_map);
    *mp.nbnd_map = nbnd_map;
  }

  if (!pmi_detail::allocate_1d(g.index_map, "INDEX_MAP", *mp.nbnd_map)) {  // :499
    return kMapstaInitAllocIndexMapFailed;                                  // :500
  }
  mp.index_map = g.index_map;  // :501

  {  // :502-508
    const auto index_map = mp.index_map;
    Kokkos::parallel_for(
        "ww::pdlib::pdlib_mapsta_init::index", Kokkos::RangePolicy<ExecSpace>(0, 1),
        KOKKOS_LAMBDA(const int) {
          int ibnd_map = 0;
          for (int ix = 1; ix <= nx; ++ix) {
            if ((mapsta(0, ix - 1) < 1) && (status(ix - 1) > 0)) {
              ibnd_map = ibnd_map + 1;
              index_map(ibnd_map - 1) = status(ix - 1);
            }
          }
        });
    ExecSpace().fence();
  }
  return kMapstaInitOk;
}

}  // namespace ww::pdlib
