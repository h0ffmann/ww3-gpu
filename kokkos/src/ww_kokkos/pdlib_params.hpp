// kokkos/src/ww_kokkos/pdlib_params.hpp
// The unstructured-mesh (PDLIB) arrays and per-call state that PDLIB_W3XYPUG,
// PDLIB_W3XYPFSN2 and PDLIB_W3XYPFSPSI2 read, as Kokkos views, and the
// W3PARALL constants they share. Phase 1: one spectral component per call, the
// per-rank arithmetic only; every MPI call stays in Fortran (the kernels'
// headers name the call sites).
//
// WW3 heritage: WW3-noaa/model/src/w3profsmd_pdlib.F90:626-1502 (the three
// routines, their USE lists at :652-671, :921-940, :1258-1274) at NOAA-EMC/WW3
// 03adda91, switch PDLIB of regtests/ww3_tp2.17/input/switch_PDLIB; the yow*
// modules under model/src/PDLIB/ at the same commit. The fork WW3/ (c9a6a0c6)
// has the same w3profsmd_pdlib.F90 byte for byte (v, diff, 2026-10-16).
//
// Precision. yowDatapool fixes rkind = 4 (PDLIB/yowdatapool.F90:48) and
// rtype = MPI_REAL4 (:77), so PDLIB_SI and PDLIB_IEN are REAL like the W3*
// arrays: float here. The REAL(8) arithmetic of the routines comes from double
// literals (1.D0, 2.d0, 0.5d0, 1.0d0) and DBLE() inside REAL statements; each
// kernel header says where and keeps double exactly there.
//
// Layout. Every array is a LayoutLeft view with the Fortran's extents, so
// X(i,j) of the Fortran is x(i-1, j-1) here; an array with lower bound 0 keeps
// its 0 at index 0 (CLATS, BBPI0, BBPIN). Index values stored in the arrays
// (INE, PDLIB_IE_CELL, ISBPI, MAPSF, IPGL_npa, ...) stay 1-based as Fortran
// wrote them; the kernels subtract 1 where they index.
#pragma once

#include <Kokkos_Core.hpp>

#include <cstddef>
#include <cstdint>

#include "real.hpp"

namespace ww::pdlib {

// W3PARALL PARAMETERs (w3parall.F90:92-97). Each is a REAL PARAMETER, so the
// double initialiser is rounded to REAL once at compile time.
inline constexpr Real kOneSixth = static_cast<Real>(1.0 / 6.0);  // ONESIXTH = 1.0d0/6.0d0, :92
inline constexpr Real kZero = static_cast<Real>(0.0);            // ZERO = 0.0d0, :94
inline constexpr Real kThr = 0x1p-126f;                          // THR = TINY(1.0), :97
static_assert(kOneSixth == 0x1.555556p-3f, "ONESIXTH as REAL, 0x3e2aaaab");
static_assert(kThr == 1.17549435082228750796873653722224568e-38f, "TINY(1.0), 0x00800000");

// Literals shared by FSN2 and PSI2 (w3profsmd_pdlib.F90:999, :1053, :1057,
// :1148 and the same statements of PSI2). 10.E10 and 10.E-10 are REAL literals
// (no d exponent) widened by DBLE: their double value is the float's.
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

  /// INE(3,ne), integer (PDLIB/yowelementpool.F90:56): local node ids 1..npa
  /// of the three corners, counter-clockwise. ine(k-1, ie-1) = INE(k,IE).
  I2 ine;
  /// PDLIB_IEN(6,ne), REAL(rkind=4) (PDLIB/yownodepool.F90:80, allocated
  /// yowpdlibmain.F90:1369, filled :1393-1398): the inward edge normals
  /// (-DY, DX) of the edge opposite each corner, pairs (1,2), (3,4), (5,6).
  R2 ien;
  /// PDLIB_SI(npa), REAL(rkind=4) (yownodepool.F90:80, :1369, summed
  /// :1412-1414): median-dual area of each node.
  R1 si;
  /// PDLIB_CCON(npa), integer (yownodepool.F90:81, :1373, :1408-1410): the
  /// number of elements touching each node.
  I1 ccon;
  /// Not a Fortran array: cell_off(ip-1) = sum of PDLIB_CCON(1:ip-1), extent
  /// npa+1, the 0-based start of node ip in ie_cell/pos_cell, as the Fortran's
  /// running J of yowpdlibmain.F90:1507-1514 counts it. Built by
  /// build_cell_off below.
  I1 cell_off;
  /// PDLIB_IE_CELL(COUNT_MAX), PDLIB_POS_CELL(COUNT_MAX), integer
  /// (yownodepool.F90:83, allocated yowpdlibmain.F90:1497, filled :1507-1514
  /// from CELLVERTEX, :1478-1485): for node IP, entries J = cell_off+1 ..
  /// cell_off+CCON(IP) give the element IE and the corner POS (1..3) with
  /// INE(POS,IE) = IP. CELLVERTEX is filled in a loop over IE = 1..ne, so the
  /// elements of each node are in increasing IE: the order the gather-form
  /// accumulations below depend on (check_cell_adjacency verifies it).
  I1 ie_cell;
  I1 pos_cell;
  /// iplg(npa), integer (yownodepool.F90:116): local -> global node id
  /// (ghosts included, yowrankModule.F90:230-233). Read by PDLIB_W3XYPUG
  /// (w3profsmd_pdlib.F90:747); read but dead in FSN2 (:1055) and PSI2.
  I1 iplg;
  /// IPGL_npa(np_global), integer (PDLIB/yowrankModule.F90:70, filled
  /// :227-233): global -> local node id 1..npa on this rank, 0 elsewhere.
  I1 ipgl_npa;
  /// IOBP_LOC(npa), INTEGER*2 (w3gdatmd.F90:1120, allocated
  /// w3profsmd_pdlib.F90:596): boundary-node type.
  S1 iobp_loc;
  /// IOBPD_LOC(NTH,npa), INTEGER*1 (w3gdatmd.F90:1119, w3profsmd_pdlib.F90:598):
  /// 1 when direction ITH may propagate at the node, 0 when it comes from land.
  B2 iobpd_loc;
  /// IOBDP_LOC(npa), INTEGER*1 (w3gdatmd.F90:1121, :600): 0 at dry nodes.
  B1 iobdp_loc;
  /// IOBPA_LOC(npa), INTEGER*1 (w3gdatmd.F90:1122, :602): 1 at active
  /// boundary nodes, where the advection term is switched off.
  B1 iobpa_loc;
  /// MAPSF(NSEA,3), integer (w3gdatmd.F90:1736): column 1 is the global node
  /// id of sea point ISEA on an unstructured grid. mapsf(isea-1, 0).
  I2 mapsf;
  /// CLATS(0:NSEA), REAL (w3gdatmd.F90:1741): clats(isea) = CLATS(ISEA);
  /// 1 on a Cartesian mesh.
  R1 clats;
  /// MAPFS(MY,MX), integer (w3gdatmd.F90:1735), MY = 1 on an unstructured
  /// grid: mapfs(0, ip_glob-1) = MAPFS(1,IP_glob) = ISEA (PDLIB_W3XYPUG, :748).
  I2 mapfs;
  /// ECOS(NTH), ESIN(NTH), REAL (W3GDATMD): read by PDLIB_W3XYPUG (:741-742).
  R1 ecos;
  R1 esin;
  /// INDEX_MAP(NBND_MAP), integer (w3gdatmd.F90:1117, allocated
  /// w3profsmd_pdlib.F90:499): read by PDLIB_W3XYPUG only when the module
  /// flag MAPSTA_HACK (w3profsmd_pdlib.F90:113, .FALSE., never set) is true.
  I1 index_map;
};

/// What changes per time step or per call and lives in W3ADATMD / W3ODATMD.
/// ITER and CFLXYMAX are written by the kernels (LCALC), so they are not const.
template <class MemSpace>
struct PdlibState {
  using memory_space = MemSpace;
  /// ITER(NK,NTH), integer (w3adatmd.F90:657, allocated :1434): sub-steps of
  /// component (IK,ITH); written when LCALC, read always. iter(ik-1, ith-1).
  Kokkos::View<int**, Kokkos::LayoutLeft, MemSpace> iter;
  /// CFLXYMAX(NSEALM), REAL (w3adatmd.F90:623, allocated :1292): running
  /// maximum Courant number per local node: the N scheme writes IP = 1..np, the PSI
  /// scheme IP = 1..npa (:1383), inside the array while NSEALM >= npa ⚠.
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

/// True when ie_cell/pos_cell list, for each node, every element that holds
/// it in INE, in increasing IE, with the corner that holds it, and nothing
/// else, and no element repeats a node: the property the gather-form sums need to add in the Fortran's
/// order. ine is INE(3,ne) column major, 1-based values. Host only; the shim
/// calls it once after the mesh is copied.
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
      // A repeated corner cannot reach the solver: it makes the element area
      // DBLTMP exactly 0 and PDLIB_INIT stops (yowpdlibmain.F90:1399-1405). The
      // gather assumes each element names a node once, so refuse such a mesh.
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
