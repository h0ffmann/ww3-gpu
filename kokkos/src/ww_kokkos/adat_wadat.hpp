// kokkos/src/ww_kokkos/adat_wadat.hpp
// The W3ADATMD arrays of one model (TYPE WADAT) as Kokkos Views, and the set of
// models WADATS(NADATA) with the selected-model index IADATA. Phase 1: the
// storage W3DIMA (adat_w3dima.hpp) allocates, nothing more.
//
// WW3 heritage: TYPE WADAT, WW3/model/src/w3adatmd.F90:384-570 at the WW3/ pin
// 8e433f99 (same file as NOAA-EMC/WW3 03adda91); author H. L. Tolman. Only the
// components switches/switch_lab_shrd compiles and W3DIMA allocates are here: the
// W3_IC3, W3_SMC, W3_PR1, W3_PR2, W3_MPI and W3_PDLIB components are not compiled
// under that switch; the X* full-grid arrays (W3XDMA), the W3_NL1 tables (W3DMNL)
// and the AINIT2 arrays (W3DMNL2) are other routines' and not here.
//
// Layout: every View is LayoutLeft so a Fortran column-major index maps 1:1 once
// the lower bound is subtracted, and lives in ExecSpace::memory_space (no UVM).
// REAL is ww::Real (float, 4 bytes); INTEGER is int; LOGICAL(4) is int holding 0
// or 1, the convention of prop_params.hpp (Geometry::maptrn).
//
// Index maps (Fortran -> C++), the trap of this struct:
//   X(0:NSEA)                 X(isea)            -> x(isea)              (base 0 kept)
//   X(NSEALM) or X(NSEA)      X(jsea)            -> x(jsea-1)
//   X(NSEALM,0:NOSWLL)        X(jsea,ip)         -> x(jsea-1, ip)
//   X(NSEALM,LB:UB) (E3DF, P2MSF)  X(jsea,ik)    -> x(jsea-1, ik-<name>_lb)
//   X(NSEALM,n)               X(jsea,i)          -> x(jsea-1, i-1)
//   WN, CG(0:NK+1,0:NSEA)     WN(ik,isea)        -> wn(ik, isea)
//   ATRNX/Y(NY*NX,-1:1)       ATRNX(ixy,d)       -> atrnx(ixy-1, d+1)
//   DDDX..(NY,NX)             DDDX(iy,ix)        -> dddx(iy-1, ix-1)
//   DCDX/Y(0:NK+1,NY,NX)      DCDX(ik,iy,ix)     -> dcdx(ik, iy-1, ix-1)
//   under LPDLIB the same arrays are (1,NSEAL) and (0:NK+1,1,NSEAL), same maps
//   ALPHA(NK,NSEAL), ITER(NK,NTH)  (i,j)         -> (i-1, j-1)
//   SPPNT(NTH,NK,4)           (ith,ik,i)         -> (ith-1, ik-1, i-1)
//   MAPX2.. (NY*NX), MAPCXY(NSEA), MAPTH2((NK+2)*NTH), MAPWN2(NSPEC+NTH),
//   IAPPRO(NSPEC)             (i)                -> (i-1)
//
// A zero-extent Fortran array is still ALLOCATED; a zero-extent View carries no
// such flag. The tests that decide whether a conditional array exists (E3DF(1,:),
// P2MSF(1), US3DF(1), USSPF(1), FLCUR.., GTYPE, FL_ALL) are the record; a consumer
// asks them, not View::is_allocated().
#pragma once

#include <vector>

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::adat {

/// @brief The W3ADATMD arrays and scalars of one model, TYPE WADAT
///        (w3adatmd.F90:384-570), as allocated by W3DIMA.
/// @details Default-constructed Views are unallocated, which is the state W3NAUX
///          leaves (w3adatmd.F90:795-808 sets only the scalars below).
template <class ExecSpace>
struct Wadat {
  using Mem = typename ExecSpace::memory_space;
  using R1 = Kokkos::View<Real*, Kokkos::LayoutLeft, Mem>;
  using R2 = Kokkos::View<Real**, Kokkos::LayoutLeft, Mem>;
  using R3 = Kokkos::View<Real***, Kokkos::LayoutLeft, Mem>;
  using I1 = Kokkos::View<int*, Kokkos::LayoutLeft, Mem>;
  using I2 = Kokkos::View<int**, Kokkos::LayoutLeft, Mem>;

  // 1) Forcing fields, always (w3adatmd.F90:1004-1023)
  R1 dw, cx, cy, ua, ud, u10, u10d, as, taua, tauadir;
  // 2) Standard mean wave parameters (:1041-1074)
  R1 hs, wlm, t02, t0m1, t01, fp0, thm, ths, thp0, hsig, stmaxe, stmaxd, hmaxe, hmaxd,
      hcmaxe, hcmaxd, qp, wbt, wnmean;
  // 3) Frequency-dependent parameters, each only when E3DF(1,i) > 0 (:1083-1113).
  //    The second Fortran lower bound E3DF(2,i) is kept here.
  R2 ef, th1m, sth1m, th2m, sth2m;
  int ef_lb = 0, th1m_lb = 0, sth1m_lb = 0, th2m_lb = 0, sth2m_lb = 0;
  // 4) Spectral partitions (:1119-1155); second dimension 0:NOSWLL
  R2 phs, ptp, plp, pdir, psi, pws;
  R1 pwst, pnr;
  R2 pthp0, pqp, ppe, pgw, psw, ptm1, pt1, pt2, pep;
  // 5) Atmosphere-waves layer (:1161-1181)
  R1 charn, tws, cge, phiaw, tauwix, tauwiy, tauwnx, tauwny;
  R2 whitecap;
  // 6) Wave-ocean layer (:1187-1242)
  R1 sxx, syy, sxy, tauox, tauoy, bhd, phioc, tusx, tusy, ussx, ussy, tauocx, tauocy, prms,
      tpms, phice;
  R2 tauice;
  R2 p2sms, us3d, ussp;  // P2SMS second lower bound P2MSF(2) kept below
  int p2sms_lb = 0;
  // 7) Wave-bottom layer (:1248-1261)
  R1 aba, abd, uba, ubd;
  R2 bedforms;
  R1 phibbl;
  R2 taubbl;
  // 8) Spectrum parameters (:1267-1284)
  R1 mssx, mssy, mscx, mscy, mssd, mscd, qkk, skew, embia1, embia2;
  // 9) Numerical diagnostics (:1290-1301)
  R1 dtdyn, fcut, cflxymax, cflthmax, cflkmax;
  // 10) User defined (:1307-1310), wavenumbers (:1312)
  R2 usero, wn;
  // FL_ALL only (:1323-1454)
  R2 cg;
  R1 ca0, cai, cd0, cdi;            // FLCUR
  R1 ua0, uai, ud0, udi, as0, asi;  // FLWIND
  R1 ma0, mai, md0, mdi;            // FLTAUA
  R1 ra0, rai;                      // FLRHOA
  R2 atrnx, atrny;
  R2 dddx, dddy;
  R3 dcdx, dcdy;
  R2 dcxdx, dcydx, dcxdy, dcydy;
  R2 alpha;
  I1 mapx2, mapy2, mapaxy, mapcxy, mapth2, mapwn2;  // W3_PR3
  I1 maptrn;                                        // W3_PR3, LOGICAL(4) as 0/1
  I2 iter;                                          // GTYPE == UNGTYPE
  I1 iappro;
  R3 sppnt;

  // Scalars W3DIMA reads or writes, initial values as W3NAUX sets them
  // (w3adatmd.F90:801, 805, 807).
  int nsealm = 0;
  bool ainit = false;
  bool fl_all = false;
};

/// @brief WADATS(NADATA) with the module counters NADATA and IADATA
///        (w3adatmd.F90:377), the model selected by the last W3SETA.
/// @details wadats[imod-1] is WADATS(IMOD). iadata == -1 is the module's initial
///          value: no W3SETA yet, so the FL_ALL pointer alias is not associated.
///          iadata stands for the model the W3SETA aliases (FL_ALL, NSEALM) point
///          at. The Fortran IADATA is also written by W3XETA (w3adatmd.F90:3174),
///          which retargets only the X* arrays, so a W3XETA port must not write it.
template <class ExecSpace>
struct Wadats {
  std::vector<Wadat<ExecSpace>> wadats;
  int nadata = -1;
  int iadata = -1;
};

}  // namespace ww::adat
