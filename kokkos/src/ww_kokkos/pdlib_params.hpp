// kokkos/src/ww_kokkos/pdlib_params.hpp
// The PDLIB mesh arrays and per-call state the PDLIB kernels read, as Kokkos views, and the
// W3PARALL constants they share. WW3 heritage: WW3-noaa/model/src/w3profsmd_pdlib.F90 and
// model/src/PDLIB/yow* at NOAA-EMC/WW3 03adda91. Evidence: the port reports
// PDLIB_W3XYPFSN2_KOKKOS.md, PDLIB_W3XYPFSPSI2_KOKKOS.md. LayoutLeft with the Fortran's
// extents (a lower bound 0 kept at index 0); stored index values stay 1-based; rkind = 4
// (PDLIB/yowdatapool.F90:48), so every REAL array is float.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include <cstddef>
#include <cstdint>

#include "real.hpp"

namespace ww::pdlib {

// W3PARALL REAL PARAMETERs (w3parall.F90:92-97): the double initialiser rounds once to REAL.
inline constexpr Real kOneSixth = static_cast<Real>(1.0 / 6.0);  // ONESIXTH = 1.0d0/6.0d0, :92
inline constexpr Real kZero = static_cast<Real>(0.0);            // ZERO = 0.0d0, :94
inline constexpr Real kThr = 0x1p-126f;                          // THR = TINY(1.0), :97
static_assert(kOneSixth == 0x1.555556p-3f, "ONESIXTH as REAL, 0x3e2aaaab");
static_assert(kThr == 1.17549435082228750796873653722224568e-38f, "TINY(1.0), 0x00800000");

// Literals of FSN2 and PSI2 (:999, :1053, :1057, :1148); 10.E10, 10.E-10 are REAL literals.
inline constexpr Real kDtmaxInit = 10.E10f;   // DTMAX = DBLE(10.E10) stored to REAL, :999
inline constexpr Real kDtmaxExpInit = 1.E10f;  // DTMAXEXP = 1.E10, :1053
inline constexpr double kSiFloor = static_cast<double>(10.E-10f);  // DBLE(10.E-10), :1057
inline constexpr Real kRd2Floor = 0.001f;      // RD2 .GT. 0.001, :1148
static_assert(kDtmaxInit == 0x1.74876ep+36f, "10.E10 as REAL, 0x51ba43b7 (.LC191)");
static_assert(kDtmaxExpInit == 1.0e10f, "1.E10 as REAL is exact, 0x501502f9 (.LC85)");
static_assert(kSiFloor == 0x1.12e0bep-30, "10.E-10 as REAL widened, 0x3e112e0be0000000 (.LC194)");
static_assert(kRd2Floor == 0x1.0624dep-10f, "0.001 as REAL, 0x3a83126f (.LC94)");

/// The mesh of this rank, fixed after the PDLIB set-up (yowpdlibmain.F90).
/// npa = np + ng nodes (own + ghost), ne elements (yowelementpool.F90:50-56).
template <class MemSpace>
struct PdlibGeometry {
  using memory_space = MemSpace;
  using I1 = Kokkos::View<const int*, Kokkos::LayoutLeft, MemSpace>;
  using I2 = Kokkos::View<const int**, Kokkos::LayoutLeft, MemSpace>;
  using R1 = Kokkos::View<const Real*, Kokkos::LayoutLeft, MemSpace>;
  using R2 = Kokkos::View<const Real**, Kokkos::LayoutLeft, MemSpace>;
  using B1 = Kokkos::View<const std::int8_t*, Kokkos::LayoutLeft, MemSpace>;
  using B2 = Kokkos::View<const std::int8_t**, Kokkos::LayoutLeft, MemSpace>;
  using S1 = Kokkos::View<const std::int16_t*, Kokkos::LayoutLeft, MemSpace>;

  int npa = 0;        ///< yowNodepool npa: local nodes incl. ghosts
  int np = 0;         ///< yowNodepool np: nodes this rank owns, 1..np of 1..npa
  int ne = 0;         ///< yowElementpool ne: local elements
  int np_global = 0;  ///< yowNodepool np_global, the extent of ipgl_npa
  int nth = 0;        ///< W3GDATMD NTH
  int nk = 0;         ///< W3GDATMD NK
  int nsea = 0;       ///< W3GDATMD NSEA, the extent of mapsf / clats
  int fsbccfl = 0;    ///< W3GDATMD FSBCCFL, LOGICAL (w3gdatmd.F90:1048), 0/1

  /// INE(3,ne) (yowelementpool.F90:56): local node ids of the corners, counter-clockwise.
  I2 ine;
  /// PDLIB_IEN(6,ne) (yownodepool.F90:80): inward normals (-DY, DX) of the opposite edges.
  R2 ien;
  /// PDLIB_SI(npa) (yownodepool.F90:80): median-dual area of each node.
  R1 si;
  /// PDLIB_CCON(npa) (yownodepool.F90:81): number of elements touching each node.
  I1 ccon;
  /// Not Fortran: cell_off(ip-1) = sum of CCON(1:ip-1), extent npa+1 (build_cell_off).
  I1 cell_off;
  /// PDLIB_IE_CELL, PDLIB_POS_CELL (yownodepool.F90:83): element and corner of node IP at
  /// cell_off+1..cell_off+CCON(IP), in increasing IE: the order the gather sums rely on.
  I1 ie_cell;
  I1 pos_cell;
  /// iplg(npa) (yownodepool.F90:116): local -> global node id.
  I1 iplg;
  /// IPGL_npa(np_global) (yowrankModule.F90:70): global -> local node id, 0 off this rank.
  I1 ipgl_npa;
  /// IOBP_LOC(npa), INTEGER*2 (w3gdatmd.F90:1120): boundary-node type.
  S1 iobp_loc;
  /// IOBPD_LOC(NTH,npa), INTEGER*1 (w3gdatmd.F90:1119): 0 where direction ITH is from land.
  B2 iobpd_loc;
  /// IOBDP_LOC(npa), INTEGER*1 (w3gdatmd.F90:1121, :600): 0 at dry nodes.
  B1 iobdp_loc;
  /// IOBPA_LOC(npa), INTEGER*1 (w3gdatmd.F90:1122): 1 at active boundary nodes.
  B1 iobpa_loc;
  /// MAPSF(NSEA,3) (w3gdatmd.F90:1736): mapsf(isea-1, 0) is the global node of ISEA.
  I2 mapsf;
  /// CLATS(0:NSEA) (w3gdatmd.F90:1741): clats(isea); 1 on a Cartesian mesh.
  R1 clats;
  /// MAPFS(MY,MX), MY = 1 here (w3gdatmd.F90:1735): mapfs(0, ip_glob-1) = ISEA.
  I2 mapfs;
  /// ECOS(NTH), ESIN(NTH), REAL (W3GDATMD): read by PDLIB_W3XYPUG (:741-742).
  R1 ecos;
  R1 esin;
  /// INDEX_MAP (w3gdatmd.F90:1117): read only under MAPSTA_HACK, .FALSE., never set (:113).
  I1 index_map;
};

/// What changes per time step or per call and lives in W3ADATMD / W3ODATMD.
/// ITER and CFLXYMAX are written by the kernels (LCALC), so they are not const.
template <class MemSpace>
struct PdlibState {
  using memory_space = MemSpace;
  /// ITER(NK,NTH) (w3adatmd.F90:657): iter(ik-1, ith-1), written when LCALC.
  Kokkos::View<int**, Kokkos::LayoutLeft, MemSpace> iter;
  /// CFLXYMAX(NSEALM) (w3adatmd.F90:623): FSN2 writes 1..np, PSI2 1..npa, in bounds while
  /// NSEALM >= npa ⚠.
  Kokkos::View<Real*, Kokkos::LayoutLeft, MemSpace> cflxymax;
};

/// Boundary input of W3ODATMD (w3odatmd.F90:543-557, allocated :1450, :1486).
template <class MemSpace>
struct PdlibBoundary {
  using memory_space = MemSpace;
  int flbpi = 0;  ///< FLBPI, LOGICAL, 0/1
  int nbi = 0;    ///< NBI
  int nspec = 0;  ///< NSPEC, first extent of BBPI0/BBPIN
  /// ISBPI(NBI), integer: sea point ISEA of boundary point IBI. isbpi(ibi-1).
  Kokkos::View<const int*, Kokkos::LayoutLeft, MemSpace> isbpi;
  /// BBPI0(NSPEC,0:NBI), BBPIN(NSPEC,0:NBI), REAL: bbpi0(isp-1, ibi).
  Kokkos::View<const Real**, Kokkos::LayoutLeft, MemSpace> bbpi0;
  Kokkos::View<const Real**, Kokkos::LayoutLeft, MemSpace> bbpin;
};

/// cell_off from PDLIB_CCON, on the host: the running J of
/// yowpdlibmain.F90:1505-1514, 0-based. out must have npa+1 entries.
inline void build_cell_off(const int* ccon, int npa, int* out) {
  int j = 0;
  for (int ip = 0; ip < npa; ++ip) {
    out[ip] = j;
    j += ccon[ip];
  }
  out[npa] = j;
}

/// True when ie_cell/pos_cell list, for each node, every element holding it, in increasing
/// IE, with its corner, and no element repeats a node. Host only; the shim calls it once.
inline bool check_cell_adjacency(const int* ine, int ne, int npa, const int* cell_off,
                                 const int* ie_cell, const int* pos_cell) {
  // Rebuild CELLVERTEX's order (yowpdlibmain.F90:1478-1485) and compare.
  if (cell_off[0] != 0) return false;
  for (int ip = 0; ip < npa; ++ip) {
    if (cell_off[ip + 1] < cell_off[ip]) return false;
  }
  if (cell_off[npa] != 3 * ne) return false;
  // fill[ip] is the next free slot of node ip in the expected order.
  int* fill = new int[static_cast<std::size_t>(npa)];
  for (int ip = 0; ip < npa; ++ip) fill[ip] = cell_off[ip];
  bool ok = true;
  for (int ie = 1; ie <= ne && ok; ++ie) {
    for (int pos = 1; pos <= 3 && ok; ++pos) {
      const int ip = ine[(ie - 1) * 3 + (pos - 1)];
      if (ip < 1 || ip > npa) {
        ok = false;
        break;
      }
      // A repeated corner makes the area 0 and PDLIB_INIT stops (yowpdlibmain.F90:1399-1405);
      // the gather assumes each element names a node once.
      for (int q = 1; q < pos; ++q)
        if (ine[(ie - 1) * 3 + (q - 1)] == ip) ok = false;
      const int j = fill[ip - 1]++;
      if (j >= cell_off[ip] || ie_cell[j] != ie || pos_cell[j] != pos) ok = false;
    }
  }
  delete[] fill;
  return ok;
}

}  // namespace ww::pdlib
