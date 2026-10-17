// kokkos/src/ww_kokkos/prop_xypug.hpp
// W3XYPUG, ported to Kokkos: the explicit spatial propagation driver of one
// spectral component on an unstructured grid without PDLIB. Turns VQ into the
// action AC, builds the advection velocities C, calls the selected
// fluctuation-splitting scheme, stores VQ back. Phase 1, translated, not
// regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:69-272 at NOAA-EMC/WW3
// 03adda91 (byte-identical in WW3/ at the fork pin); author A. Roland. Called
// by W3WAVE (w3wavemd.F90:1970, :1973, :1976, one per PR1/PR2/PR3 switch).
// W3_S (STRACE) and the W3_MGP moving-grid terms (:212-215) are not in the
// switch of ww3_tp2.6 and are not ported.
//
// The four schemes it calls, W3XYPFSN2, W3XYPFSPSI2, W3XYPFSFCT2 and
// W3XYPFSNIMP (w3profsmd.F90:470, :730, :1290, :988), have no Kokkos
// translation (their PDLIB variants in pdlib_fsn2.hpp etc. are other
// routines), and neither has DSEC21 (w3timemd.F90:330): they are host hooks,
// called at their place in the Fortran's sequence. SETDEPTH is ported
// (profs_setdepth.hpp) and called directly. The routine is therefore a host
// function, serial, in the Fortran's structure and statement order.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and assembly of
// w3profsmd.F90 (WW3's CMake Release flags, switch of ww3_tp2.6): no FMA, no
// libm call; the vectorised AC and C loops (:234-236) are element-wise
// products, so the lanes cannot change a bit; the store MAX is maxss with +0
// as the source operand (:270). Evidence: kokkos/port_report/W3XYPUG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <Kokkos_Core.hpp>

#include "profs_setdepth.hpp"
#include "real.hpp"

namespace ww::prop {

/// The module state W3XYPUG reads (w3profsmd.F90:135-146). Fortran arrays
/// arrive 0-based; the explicit lower bounds become offsets:
///   mapsf  MAPSF(MSEA,3) (w3gdatmd.F90:1736), column major with leading
///          extent msea: MAPSF(ISEA,J) = mapsf[(J-1)*msea + ISEA-1]
///   clats  CLATS(0:MSEA) (w3gdatmd.F90:1741): CLATS(ISEA) = clats[ISEA]
///   ecos, esin  ECOS(ITH) = ecos[ITH-1]
///   iobp   IOBP(MX), INTEGER*2 (w3gdatmd.F90:1132, :3256): IOBP(i) = iobp[i-1]
///   iobdp  IOBDP(MX), INTEGER*1 (w3gdatmd.F90:1133, :3258), written by SETDEPTH
///   dw     DW(0:NSEA) (w3adatmd.F90:1004): DW(k) = dw[k], read by SETDEPTH
///   cg     CG(0:NK+1,0:NSEA) (w3adatmd.F90:1325): CG(IK,ISEA) = cg[ISEA*(nk+2) + IK]
///   cx, cy CX(0:NSEA) (w3adatmd.F90:1008): CX(ISEA) = cx[ISEA]; read only when flcur
///   time, tbpi0, tbpin  W3WDATMD TIME(2), W3ODATMD TBPI0(2), TBPIN(2)
/// LOGICALs are int (gfortran's 4-byte LOGICAL; the GIMPLE tests them != 0).
struct XypugState {
  int nx, ny, nsea, msea, nk, nth, mx;
  const int* mapsf;
  const Real* clats;
  const Real* ecos;
  const Real* esin;
  const std::int16_t* iobp;
  std::int8_t* iobdp;
  const Real* dw;
  Real dmin;  ///< W3GDATMD DMIN, read by SETDEPTH (w3profsmd.F90:1650, :1674)
  const Real* cg;
  const Real* cx;
  const Real* cy;
  int flcur;                      ///< W3IDATMD FLCUR (:146)
  int flbpi;                      ///< W3ODATMD FLBPI (:144)
  int fsn, fspsi, fsfct, fsnimp;  ///< W3GDATMD (:141), the first true one wins (:251-259)
  const int* time;
  const int* tbpi0;
  const int* tbpin;
};

/// The Fortran calls W3XYPUG makes and this repository has not translated.
/// ctx is the shim's, passed back unchanged. c is C(NX,2), column major
/// (C(IX,J) = c[(J-1)*nx + IX-1]), read only; ac is AC(NX), DOUBLE
/// PRECISION, INOUT. The schemes' dummies are assumed shape, C(:,:) and AC(:)
/// (w3profsmd.F90:547-550 for W3XYPFSN2, the other three alike), so the shim
/// passes the two contiguous arrays as the Fortran does. RD1, RD2 and LCALC
/// are INTENT(IN) there and go by value. The schemes read and write module
/// state of their own (IOBPD, the boundary arrays); the shim keeps it.
struct XypugHooks {
  void* ctx = nullptr;
  /// W3TIMEMD DSEC21(T1, T2), seconds from T1 to T2 as the REAL WW3 returns.
  float (*dsec21)(void* ctx, const int* t1, const int* t2) = nullptr;
  /// One signature for the four (:252, :254, :256, :258).
  using Scheme = void (*)(void* ctx, int isp, const Real* c, int lcalc, Real rd1, Real rd2,
                          Real dt, double* ac);
  Scheme w3xypfsn2 = nullptr;
  Scheme w3xypfspsi2 = nullptr;
  Scheme w3xypfsfct2 = nullptr;
  Scheme w3xypfsnimp = nullptr;
};

/// Reals of work storage, for the Fortran's automatic arrays (:171, :176-177), in
/// this order: vlcflx (NX+1)*NY, vlcfly NX*NY, c 2*NX.
inline constexpr int xypug_work_reals(int nx, int ny) { return (nx + 1) * ny + nx * ny + 2 * nx; }

/// Doubles of work storage: AC(NX) (:177).
inline constexpr int xypug_work_doubles(int nx) { return nx; }

/// W3XYPUG(ISP, FACX, FACY, DTG, VQ, VGX, VGY, LCALC). VGX, VGY are read only
/// under W3_MGP (:212-215) and are left out. W3WAVE passes FACX twice
/// (w3wavemd.F90:1970), so facy == facx at every call; both are kept.
///   vq   VQ(1-NY : NY*(NX+2)) (:159), W3WAVE's FIELD: VQ(i) = vq[i + ny - 1],
///        NY*(NX+3) entries
///   wr, wd  xypug_work_reals / xypug_work_doubles entries, caller-owned;
///        VLCFLX and VLCFLY are zeroed here where the Fortran zeroes them
///        (:197-198); C and AC are written in full before they are read.
/// The hooks the flags reach must be set: a missing one aborts, where the
/// Fortran would have called the routine. With no scheme flag set no scheme
/// runs and VQ is still written back, as in the Fortran.
inline void w3xypug(const XypugState& p, const XypugHooks& h, int isp, Real facx, Real facy,
                    Real dtg, Real* vq, int lcalc, Real* wr, double* wd) {
  constexpr Real r0 = static_cast<Real>(0.);
  constexpr Real r1 = static_cast<Real>(1.);
  const int nx = p.nx;
  const int ny = p.ny;
  const int nk2 = p.nk + 2;
  const int* mapsf3 = p.mapsf + 2 * p.msea;  // MAPSF(:,3)
  // VQ(i), VLCFLX(i), VLCFLY(i), AC(i), IOBP(i), IOBDP(i): 1-based Fortran index.
  auto VQ = [=](int i) -> Real& { return vq[i + ny - 1]; };
  Real* vlcflx = wr;
  Real* vlcfly = vlcflx + (nx + 1) * ny;
  Real* c = vlcfly + nx * ny;
  double* ac = wd;

  // 1.a :187-193.
  const int ith = 1 + (isp - 1) % p.nth;
  const int ik = 1 + (isp - 1) / p.nth;
  const Real ccos = facx * p.ecos[ith - 1];
  const Real csin = facy * p.esin[ith - 1];
  const Real ccurx = facx;
  const Real ccury = facy;

  // 1.b :197-198.
  for (int i = 0; i < (nx + 1) * ny; ++i) vlcflx[i] = r0;
  for (int i = 0; i < nx * ny; ++i) vlcfly[i] = r0;

  // 1.c :202. SETDEPTH sets IOBDP(1:MX) from DW and DMIN.
  ww::profs::setdepth(p.dmin, p.iobdp, p.mx, nx, p.dw);

  // 2. :207-216. VQ/CG*CLATS is (VQ/CG)*CLATS, CCOS*CG/CLATS is (CG*CCOS)/CLATS
  // in the object (the product commutes, same bits).
  for (int isea = 1; isea <= p.nsea; ++isea) {
    const int ixy = mapsf3[isea - 1];
    const Real cgv = p.cg[isea * nk2 + ik];
    const Real cl = p.clats[isea];
    VQ(ixy) = VQ(ixy) / cgv * cl;
    vlcflx[ixy - 1] = ccos * cgv / cl;
    vlcfly[ixy - 1] = csin * cgv;
  }

  // :218-229. IOBP is INTEGER*2, compared with 1 as cmpw $0x1: a node with
  // IOBP 0 (coast) or 2, 3 (boundary types) gets no current.
  if (p.flcur) {
    for (int isea = 1; isea <= p.nsea; ++isea) {
      const int ixy = mapsf3[isea - 1];
      if (p.iobp[ixy - 1] == 1) {
        vlcflx[ixy - 1] = vlcflx[ixy - 1] + ccurx * p.cx[isea] / p.clats[isea];
        vlcfly[ixy - 1] = vlcfly[ixy - 1] + ccury * p.cy[isea];
      }
    }
  }

  // 3. :234-236. AC is DBLE(VQ)*DBLE(IOBDP) (cvtss2sd, cvtsi2sd, mulsd; exact
  // for an INTEGER*1 factor). C is VLCFL*REAL(IOBDP) in REAL, element-wise.
  for (int ix = 1; ix <= nx; ++ix) {
    ac[ix - 1] = static_cast<double>(VQ(ix)) * static_cast<double>(p.iobdp[ix - 1]);
  }
  for (int ix = 1; ix <= nx; ++ix) {
    c[ix - 1] = vlcflx[ix - 1] * static_cast<Real>(p.iobdp[ix - 1]);
  }
  for (int ix = 1; ix <= nx; ++ix) {
    c[nx + ix - 1] = vlcfly[ix - 1] * static_cast<Real>(p.iobdp[ix - 1]);
  }

  // 4. :241-247. RD1 before RD2, as the object calls DSEC21.
  Real rd1;
  Real rd2;
  if (p.flbpi) {
    if (!h.dsec21) Kokkos::abort("ww::prop::w3xypug: dsec21 hook missing (:242)");
    rd1 = h.dsec21(h.ctx, p.tbpi0, p.time);
    rd2 = h.dsec21(h.ctx, p.tbpi0, p.tbpin);
  } else {
    rd1 = r1;
    rd2 = r0;
  }

  // :251-259, the first true flag wins.
  XypugHooks::Scheme scheme = nullptr;
  if (p.fsn) {
    scheme = h.w3xypfsn2;
  } else if (p.fspsi) {
    scheme = h.w3xypfspsi2;
  } else if (p.fsfct) {
    scheme = h.w3xypfsfct2;
  } else if (p.fsnimp) {
    scheme = h.w3xypfsnimp;
  }
  if (p.fsn || p.fspsi || p.fsfct || p.fsnimp) {
    if (!scheme) Kokkos::abort("ww::prop::w3xypug: scheme hook missing (:251-259)");
    scheme(h.ctx, isp, c, lcalc, rd1, rd2, dtg, ac);
  }

  // :261-264. ISEA = MAPFS(1,IX) is a dead store (DEBUG only in the GIMPLE)
  // and is left out. REAL(AC) is cvtsd2ss, round to nearest. Every node is
  // stored, sea or not.
  for (int ix = 1; ix <= nx; ++ix) VQ(ix) = static_cast<Real>(ac[ix - 1]);

  // 6. :268-271. CG/CLATS*VQ is (CG/CLATS)*VQ; MAX(0., x) is maxss %xmm1(+0),
  // %xmm0(x): x > 0 ? x : +0, so NaN and -0 store +0.
  for (int isea = 1; isea <= p.nsea; ++isea) {
    const int ixy = mapsf3[isea - 1];
    const Real x = p.cg[isea * nk2 + ik] / p.clats[isea] * VQ(ixy);
    VQ(ixy) = (x > r0) ? x : r0;
  }
}

}  // namespace ww::prop
