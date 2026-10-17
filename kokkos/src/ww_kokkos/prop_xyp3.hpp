// kokkos/src/ww_kokkos/prop_xyp3.hpp
// W3XYP3, ported to Kokkos: third-order ULTIMATE QUICKEST propagation in
// physical space of one spectral bin on a regular (or curvilinear) grid, with
// Tolman's garden-sprinkler averaging. Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3pro3md.F90:641-1477 at NOAA-EMC/WW3
// 03adda91; author H. L. Tolman, curvilinear version W. E. Rogers and T. J.
// Campbell. W3_S, W3_T* output, W3_OMPH directives and the W3_MGP/W3_MGG moving
// grid terms are not in switch_lab_shrd and are not ported; W3_UNO is not
// either, so the sweeps are W3QCK3 (prop_qck3.hpp). A kernel never prints: the
// tripole abort of :868-874 is the caller's check.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and assembly of
// w3pro3md.F90 (WW3's own CMake Release flags, switch_lab_shrd): no libm call
// but lroundf (NINT, :931; the C++ calls roundf and converts, which gives the
// same parity for every input), no FMA, no vectorised loop, MINVAL/MAXVAL inlined
// as in-order scans, SQRT of :890 dead and deleted, 0.575*GRAV and DERA*RADIUS
// folded, every MIN/MAX in the operand order of the emitted maxss/minss
// (spec_W3XYP3.md, section 3). The whole routine runs serially for one ISP, in
// source order, as W3WAVE calls it (w3wavemd.F90:1944-2001, no OpenMP there
// under this switch).
#pragma once

#include <Kokkos_Core.hpp>

#include "prop_loops.hpp"
#include "prop_params.hpp"
#include "prop_qck3.hpp"
#include "real.hpp"
#include "st4_params.hpp"  // fint, nint, max_r, min_r

namespace ww::prop {

/// Reals of work storage w3xyp3 needs, for the Fortran's automatic arrays
/// (w3pro3md.F90:846-858, malloc'ed per call) plus W3QCK3's FLA, in this order:
///   rdi1, rdi2, rdi3, rdi4          NSEA each            RDI1..RDI4(NSEA)
///   vlcflx, vlcfly, cxtot           (NX+1)*NY each       :856-857
///   cytot                           NX*NY                :857
///   aq                              NY*(NX+3)            AQ(1-NY:NY*(NX+2))
///   fla                             NY*(NX+1)            W3QCK3's FLA (qck3_work_reals)
KOKKOS_INLINE_FUNCTION constexpr int xyp3_work_reals(int nx, int ny, int nsea) {
  return 4 * nsea + 3 * (nx + 1) * ny + nx * ny + ny * (nx + 3) + qck3_work_reals(nx, ny);
}

/// Ints of work storage, in this order:
///   iixy1, iixy2, iixy3, iixy4      NSEA each            IIXY1..IIXY4(NSEA)
///   mapstx                          NY*(NX+4)            MAPSTX(1-2*NY:NY*(NX+2))
KOKKOS_INLINE_FUNCTION constexpr int xyp3_work_ints(int nx, int ny, int nsea) {
  return 4 * nsea + ny * (nx + 4);
}

namespace xyp3_detail {

/// gfortran's inline MINVAL(CX(1:NSEA)) (:896; GIMPLE bb 9-40, object
/// 0x5390-0x5640): HUGE for an empty section; the scan for the first element
/// that is not NaN (x <= +Inf) starts the reduction there, NaN when there is
/// none; then in index order, minss %limit,%x: x < limit ? x : limit, so a NaN
/// x keeps the limit. Not vectorised, not reassociated.
KOKKOS_INLINE_FUNCTION Real minval_in_order(const Real* a, int n) {
  if (n <= 0) return Kokkos::finite_max_v<Real>;
  int i = 0;
  while (i < n && !(a[i] <= Kokkos::infinity_v<Real>)) ++i;
  if (i == n) return Kokkos::quiet_NaN_v<Real>;
  Real limit = a[i];
  for (++i; i < n; ++i) limit = (a[i] < limit) ? a[i] : limit;
  return limit;
}

/// MAXVAL likewise (:897): -HUGE, the scan with x >= -Inf, maxss: x > limit ? x : limit.
KOKKOS_INLINE_FUNCTION Real maxval_in_order(const Real* a, int n) {
  if (n <= 0) return -Kokkos::finite_max_v<Real>;
  int i = 0;
  while (i < n && !(a[i] >= -Kokkos::infinity_v<Real>)) ++i;
  if (i == n) return Kokkos::quiet_NaN_v<Real>;
  Real limit = a[i];
  for (++i; i < n; ++i) limit = (a[i] > limit) ? a[i] : limit;
  return limit;
}

/// SIGN(NY, IIXY) on INTEGERs (:1202, :1238), as the object emits it branch
/// free: m = (a ^ b) >> 31, (a + m) ^ m. |a| with the sign of b; b = 0 counts
/// as positive.
KOKKOS_INLINE_FUNCTION int isign(int a, int b) { return ((a ^ b) < 0) ? -a : a; }

/// X(IY,IX) of a (NY,NX) grid array with lower bounds (1,1).
KOKKOS_INLINE_FUNCTION Real grid2(const Real* x, int ny, int iy, int ix) {
  return x[(ix - 1) * ny + iy - 1];
}

/// One corner of the averaging stencil (:1004-1021 for the sum, :1035-1052 for
/// the difference, the same statements): the index offsets and the ratio and
/// magnitude of the averaging vector.
KOKKOS_INLINE_FUNCTION void corner(Real dxcgc, Real dycgc, int ny, int& iixa, int& iixb,
                                   Real& rda, Real& rdb) {
  constexpr Real r0 = static_cast<Real>(0.);
  constexpr Real r1 = static_cast<Real>(1.);
  constexpr Real r1em5 = static_cast<Real>(1.E-5);
  static_assert(r1em5 == 9.99999974737875163555145263671875e-6f, "1.E-5 as REAL, 0x3727c5ac");
  // :1004-1007, comiss: a NaN component compares false and keeps the + sign.
  const int ixc = (dxcgc < r0) ? -ny : ny;
  const int iyc = (dycgc < r0) ? -1 : 1;
  iixa = ixc + iyc;  // :1009
  // :1010-1021. ABS is an andps mask (fabsf); the quotient is formed, then its
  // absolute value. The else branch divides only above 1.E-5; the if branch
  // divides unconditionally.
  if (Kokkos::fabs(dxcgc) > Kokkos::fabs(dycgc)) {
    iixb = ixc;
    rda = Kokkos::fabs(dycgc / dxcgc);
    rdb = Kokkos::fabs(dxcgc);
  } else {
    iixb = iyc;
    rda = (Kokkos::fabs(dycgc) > r1em5) ? Kokkos::fabs(dxcgc / dycgc) : r1;
    rdb = Kokkos::fabs(dycgc);
  }
}

/// CXMIN, CXMAX, CYMIN, CYMAX (:896-899), broadcast from the thread that scans.
struct CRange {
  Real cxmin, cxmax, cymin, cymax;
};

}  // namespace xyp3_detail

/// W3XYP3(ISP, DTG, MAPSTA, MAPFS, VQ, VGX, VGY) for one spectral bin. MAPFS
/// is never read (:641-1477, none in the GIMPLE) and VGX, VGY only under
/// W3_MGP (:887-888); all three are left out.
///
/// Fortran arrays arrive 0-based; the explicit lower bounds become offsets:
///   mapsta -> MAPSTA(NY*NX):           MAPSTA(i) = mapsta[i - 1]
///   vq     -> VQ(1-NY : NY*(NX+2)):    VQ(i)     = vq[i + ny - 1], the caller's
///             FIELD (w3wavemd.F90:763), ghost columns included: NY*(NX+3) entries
///   wr, wi -> xyp3_work_reals / xyp3_work_ints entries, caller-owned; the four
///             Courant and speed arrays are zeroed here where the Fortran zeroes
///             them (:944-947), nothing else is initialised beyond what the
///             Fortran writes before it reads.
/// p, g, s: prop_params.hpp. The caller refuses p.iclose == kIcloseTrpl
/// (:868-874, EXTCDE), checks s.nbi > 0 only when s.flbpi, and computes the
/// three DSEC21 values of Step.
///
/// The averaging scatters into coinciding targets (:1154-1276) and the
/// boundary update takes the last write of a point listed twice (:1406-1414):
/// their store order is binding, so they run on one thread, in source order,
/// in both forms below (one()). Every other loop writes one element per
/// iteration, of an array no other iteration of that loop reads: the per-ISEA
/// loops through MAPSF(:,3), injective because MAPFS(MAPSF(ISEA,3)) = ISEA; the
/// per-cell copies; the global fold per IY (its indices are IY modulo NY); and
/// the W3QCK3 sweeps (prop_qck3.hpp). Those are the loops W3_OMPH runs as
/// PARALLEL DO (:978, :1077, :1106, :1126, :1298, :1365, :1427), plus the copies
/// and the fold. w3xyp3_loops with TeamLoops splits them over a team, a barrier
/// between loops (PATCH.md section 16); w3xyp3 runs the whole body on the calling
/// thread, as translated. Parallelising the averaging over ISEA would reorder
/// its sums: ask the owner, own PR.
template <class L>
KOKKOS_INLINE_FUNCTION void w3xyp3_loops(const L& lp, const Params& p, const Grid& g,
                                         const Step& s, int isp, Real dtg, const int* mapsta,
                                         Real* vq, Real* wr, int* wi) {
  using ww::st4::fint;
  using ww::st4::max_r;
  using ww::st4::min_r;
  using ww::st4::nint;
  using xyp3_detail::corner;
  using xyp3_detail::grid2;
  using xyp3_detail::isign;
  constexpr Real r0 = static_cast<Real>(0.);
  constexpr Real r05 = static_cast<Real>(0.5);
  constexpr Real r1 = static_cast<Real>(1.);
  constexpr Real r3 = static_cast<Real>(3.);
  constexpr Real r6 = static_cast<Real>(6.);
  constexpr Real r1em3 = static_cast<Real>(0.001);
  static_assert(r1em3 == 1.000000047497451305389404296875e-3f, "0.001 as REAL, 0x3a83126f");

  const int nx = p.nx;
  const int ny = p.ny;
  const int nsea = p.nsea;
  const int nxy = nx * ny;
  const int nxy1 = (nx + 1) * ny;  // VLCFLX, VLCFLY, CXTOT (:856-857)
  const int nq = ny * (nx + 3);    // VQ, AQ with the three ghost columns
  const int qoff = ny - 1;         // VQ(i), AQ(i) -> [i + qoff]
  const int moff = 2 * ny - 1;     // MAPSTX(i) -> [i + moff]
  const int cgld = p.nk + 2;       // CG(IK,ISEA) -> cg[ISEA*cgld + IK]

  // Work arrays, in the order xyp3_work_reals / xyp3_work_ints document.
  Real* const rdi1 = wr;
  Real* const rdi2 = rdi1 + nsea;
  Real* const rdi3 = rdi2 + nsea;
  Real* const rdi4 = rdi3 + nsea;
  Real* const vlcflx = rdi4 + nsea;
  Real* const vlcfly = vlcflx + nxy1;
  Real* const cxtot = vlcfly + nxy1;
  Real* const cytot = cxtot + nxy1;
  Real* const aq = cytot + nxy;
  Real* const fla = aq + nq;
  int* const iixy1 = wi;
  int* const iixy2 = iixy1 + nsea;
  int* const iixy3 = iixy2 + nsea;
  int* const iixy4 = iixy3 + nsea;
  int* const mapstx = iixy4 + nsea;

  // 1.a Set constants (:878-931).
  const bool global = p.iclose != kIcloseNone;  // :878
  const int ith = 1 + (isp - 1) % p.nth;        // :879
  const int ik = 1 + (isp - 1) / p.nth;         // :880
  const Real cg0 = kCg0Fac / g.sig[1];          // :882, 0.575*GRAV folded
  const Real cga = kCg0Fac / g.sig[ik];         // :883
  const Real ecosi = g.ecos[ith - 1];
  const Real esini = g.esin[ith - 1];
  const Real cgx = ecosi * cga;  // :884, emitted ECOS(ITH)*CGA; the same bits as CGA*ECOS
  const Real cgy = esini * cga;  // :885
  // :890 CGC = SQRT(CGX**2+CGY**2) is dead without MGP/MGG and the object has
  // no sqrtss: not translated.

  // :895-918. The compiler kept only ABS(CGX), ABS(CGY) after the IFs, since
  // CGX and CGY are read nowhere else than inside ABS at :921.
  Real acgx;
  Real acgy;
  Real cxc;
  Real cyc;
  if (p.flcur != 0) {
    // The four in-order scans on one thread, broadcast to the team.
    xyp3_detail::CRange cr{};
    lp.one_value(
        [&](xyp3_detail::CRange& v) {
          v.cxmin = xyp3_detail::minval_in_order(s.cx + 1, nsea);  // :896
          v.cxmax = xyp3_detail::maxval_in_order(s.cx + 1, nsea);  // :897
          v.cymin = xyp3_detail::minval_in_order(s.cy + 1, nsea);  // :898
          v.cymax = xyp3_detail::maxval_in_order(s.cy + 1, nsea);  // :899
        },
        cr);
    const Real cxmin = cr.cxmin;
    const Real cxmax = cr.cxmax;
    const Real cymin = cr.cymin;
    const Real cymax = cr.cymax;
    // :900-909 as emitted: addss, andps, then maxss %b,%a: a > b ? a : b, so a
    // tie or a NaN picks the CXMAX branch, as the Fortran's IF does.
    const Real ax = Kokkos::fabs(cgx + cxmin);
    const Real bx = Kokkos::fabs(cgx + cxmax);
    acgx = max_r(ax, bx);
    const Real ay = Kokkos::fabs(cgy + cymin);
    const Real by = Kokkos::fabs(cgy + cymax);
    acgy = max_r(ay, by);
    // :910-911, maxss %|cxmax|,%|cxmin|: |cxmin| > |cxmax| ? |cxmin| : |cxmax|.
    cxc = max_r(Kokkos::fabs(cxmin), Kokkos::fabs(cxmax));
    cyc = max_r(Kokkos::fabs(cymin), Kokkos::fabs(cymax));
  } else {
    acgx = Kokkos::fabs(cgx);
    acgy = Kokkos::fabs(cgy);
    cxc = r0;  // :917
    cyc = r0;  // :918
  }
  // :921, MAX(ABS(CGX), ABS(CGY), CXC, CYC, 0.001*CG0) emitted as a tree, not
  // the left-to-right chain (object 0x2b78-0x2bbb): maxss %|cgy|,%|cgx|;
  // maxss %cyc,%cxc; maxss %t2,%e; maxss %t3,%t1. Each is B > A ? B : A.
  const Real t1 = max_r(acgx, acgy);
  const Real t2 = max_r(cxc, cyc);
  const Real e = cg0 * r1em3;
  const Real t3 = max_r(e, t2);
  const Real cgn = max_r(t1, t3);
  // :923, DTCFL*CG0 first, then / CGN, then DTG / that; INT is cvttss2si. CG0,
  // the group speed at the first frequency, is the CFL reference for every IK.
  const int ntloc = 1 + fint(dtg / ((p.dtcfl * cg0) / cgn));
  const Real rntloc = static_cast<Real>(ntloc);  // one REAL(NTLOC), shared by :924, :967, :1397
  const Real dtloc = dtg / rntloc;                // :924
  Real dtrad = dtloc;                             // :925
  if (p.flagll != 0) dtrad = dtloc / kDeraRadius;  // :926, DERA*RADIUS folded
  // :928-931. DTTST is INTEGER: the REAL seconds truncated, converted back for
  // the division; NINT is lroundf; MOD(.,2) == 0 is the parity test.
  const int dttst = fint(s.dsec_day);
  bool yfirst = (nint(static_cast<Real>(dttst) / dtg) % 2) == 0;

  // 1.b Initialize arrays (:944-957).
  lp.par(0, nxy1, [&](int i) { vlcflx[i] = r0; });  // :944
  lp.par(0, nxy1, [&](int i) { vlcfly[i] = r0; });  // :945
  lp.par(0, nxy1, [&](int i) { cxtot[i] = r0; });   // :946
  lp.par(0, nxy, [&](int i) { cytot[i] = r0; });    // :947
  lp.par(1, nxy + 1, [&](int i) { mapstx[i + moff] = mapsta[i - 1]; });  // :949
  // The ghost columns, MAPSTX(1-2*NY:0) and (NX*NY+1:NX*NY+2*NY), outside :949's range.
  if (global) {
    // :952-953, two ghost columns each side copied from the far side.
    lp.par(0, 2 * ny, [&](int j) {
      mapstx[(1 - 2 * ny + j) + moff] = mapsta[(nx - 2) * ny + j];
      mapstx[(nxy + 1 + j) + moff] = mapsta[j];
    });
  } else {
    // :955-956.
    lp.par(0, 2 * ny, [&](int j) {
      mapstx[(1 - 2 * ny + j) + moff] = 0;
      mapstx[(nxy + 1 + j) + moff] = 0;
    });
  }

  // 1.c Pre-calculate interpolation info (:961-971). FGSE = 1 (:841) is
  // folded away without MGG.
  Real fth = (p.wdth * p.dth) * dtloc;                             // :961
  Real fcg = ((p.wdcg * r05) * (p.xfr - (r1 / p.xfr))) * dtloc;  // :962, the paren is a node
  if (p.flagll != 0) {
    fth = (fth / kRadius) / kDera;  // :964, two divisions in this order
    fcg = (fcg / kRadius) / kDera;  // :965
  }
  // :967, "required to match original (is this correct?)": FCG is divided by
  // NTLOC a second time, DTLOC already carrying 1/NTLOC. Kept.
  fcg = fcg / rntloc;
  const Real fthx = -(esini * fth);  // :968, the product then the negation
  const Real fthy = ecosi * fth;     // :969
  const Real fcgx = ecosi * fcg;     // :970
  const Real fcgy = esini * fcg;     // :971

  // :983-1059, the averaging stencil per sea point.
  lp.par(1, nsea + 1, [&](int isea) {
    const int ix = g.mapsf[isea - 1];
    const int iy = g.mapsf[nsea + isea - 1];
    const Real clat = g.clats[isea];
    const Real dpdx = grid2(g.dpdx, ny, iy, ix);
    const Real dpdy = grid2(g.dpdy, ny, iy, ix);
    const Real dqdx = grid2(g.dqdx, ny, iy, ix);
    const Real dqdy = grid2(g.dqdy, ny, iy, ix);
    // :990-997, two products then one add, each line.
    Real tmpx = fthx / clat;
    Real tmpy = fthy;
    const Real dxcgn = dpdx * tmpx + dpdy * tmpy;
    const Real dycgn = dqdx * tmpx + dqdy * tmpy;
    tmpx = fcgx / clat;
    tmpy = fcgy;
    const Real dxcgs = dpdx * tmpx + dpdy * tmpy;
    const Real dycgs = dqdx * tmpx + dqdy * tmpy;
    // :1001-1021, the "sum" corner.
    corner(dxcgn + dxcgs, dycgn + dycgs, ny, iixy1[isea - 1], iixy2[isea - 1], rdi1[isea - 1],
           rdi2[isea - 1]);
    // :1031-1052, the "difference" corner.
    corner(dxcgn - dxcgs, dycgn - dycgs, ny, iixy3[isea - 1], iixy4[isea - 1], rdi3[isea - 1],
           rdi4[isea - 1]);
  });

  // 2.a Velocities (:1080-1137). CXTOT, CYTOT by cell IXY; CG, CX, CLATS by ISEA.
  lp.par(1, nsea + 1, [&](int isea) {
    const int ixy = g.mapsf[2 * nsea + isea - 1];
    const Real cgi = s.cg[isea * cgld + ik];
    const Real clat = g.clats[isea];
    vq[ixy + qoff] = (vq[ixy + qoff] / cgi) * clat;  // :1082, divide then multiply
    cxtot[ixy - 1] = (ecosi * cgi) / clat;            // :1083
    cytot[ixy - 1] = esini * cgi;                     // :1084
  });
  if (p.flcur != 0) {
    lp.par(1, nsea + 1, [&](int isea) {
      const int ixy = g.mapsf[2 * nsea + isea - 1];
      cxtot[ixy - 1] = cxtot[ixy - 1] + (s.cx[isea] / g.clats[isea]);  // :1111
      cytot[ixy - 1] = cytot[ixy - 1] + s.cy[isea];                     // :1112
    });
  }
  lp.par(1, nsea + 1, [&](int isea) {
    const int ix = g.mapsf[isea - 1];
    const int iy = g.mapsf[nsea + isea - 1];
    const int ixy = g.mapsf[2 * nsea + isea - 1];
    const Real cxt = cxtot[ixy - 1];
    const Real cyt = cytot[ixy - 1];
    const Real cp = cxt * grid2(g.dpdx, ny, iy, ix) + cyt * grid2(g.dpdy, ny, iy, ix);  // :1133
    const Real cq = cxt * grid2(g.dqdx, ny, iy, ix) + cyt * grid2(g.dqdy, ny, iy, ix);  // :1134
    vlcflx[ixy - 1] = dtrad * cp;  // :1135, emitted DTRAD*CP
    vlcfly[ixy - 1] = dtrad * cq;  // :1136
  });
  // The extension column NX*NY+1..(NX+1)*NY of VLCFLX stays 0 until W3QCK3
  // with CLOSE copies column 1 into it (w3uqckmd.F90:1105); it then persists
  // across sub-steps, as here. VLCFLY's is never written.

  // 3. Loop over sub-steps (:1145-1418).
  for (int itloc = 1; itloc <= ntloc; ++itloc) {
    // 3.a Average. :1149-1150, over the full bounds, ghost columns included
    // (a memcpy and a memset in the object).
    lp.par(0, nq, [&](int i) { aq[i] = vq[i]; });
    lp.par(0, nq, [&](int i) { vq[i] = r0; });

    // 3.a.1 and 3.a.2 on one thread, in source order (the store order is binding).
    lp.one([&]() {
      // 3.a.1 Central points (:1154-1183): all eight neighbours sea, as W3MAP3
      // listed them, so the stencil scatters without a map test, into the ghost
      // columns too for a point on column 1 or NX (W3MAP3 wraps IX0/IX2 whether
      // or not the grid is global, :410-411). Store order: centre, +1, +2, +3,
      // +4, -1, -2, -3, -4; targets can coincide, so each += reads the previous
      // store.
      for (int ip = 1; ip <= g.ncent; ++ip) {
        const int isea = g.mapcxy[ip - 1];
        const int ixy = g.mapsf[2 * nsea + isea - 1];
        if (g.maptrn[ixy - 1] != 0) {
          vq[ixy + qoff] = aq[ixy + qoff];  // :1157-1158, averaging off next to an obstruction
        } else {
          const Real cgi = s.cg[isea * cgld + ik];
          const Real rd1 = rdi1[isea - 1];                   // :1160
          const Real rd2 = min_r(rdi2[isea - 1] * cgi, r1);  // :1161, minss %1.0,%x: x < 1 ? x : 1, NaN -> 1
          const Real rd3 = rdi3[isea - 1];                   // :1162
          const Real rd4 = min_r(cgi * rdi4[isea - 1], r1);  // :1163, emitted CG*RDI4
          const Real a = aq[ixy + qoff];
          const int i1 = iixy1[isea - 1];
          const int i2 = iixy2[isea - 1];
          const int i3 = iixy3[isea - 1];
          const int i4 = iixy4[isea - 1];
          // :1164-1165, (3.-RD2-RD4) is a paren node, then *AQ, then /3.
          vq[ixy + qoff] = vq[ixy + qoff] + (((r3 - rd2) - rd4) * a) / r3;
          // :1167-1173. AQ*RD2*RD1/6. is ((AQ*RD2)*RD1)/6, emitted with the
          // products commuted (same bits); computed once, reused for the - side
          // as the object does (CSE of identical expressions).
          const Real c1 = ((rd2 * a) * rd1) / r6;
          const Real c2 = (((r1 - rd1) * a) * rd2) / r6;
          const Real c3 = ((rd4 * a) * rd3) / r6;
          const Real c4 = (((r1 - rd3) * a) * rd4) / r6;
          vq[ixy + i1 + qoff] = vq[ixy + i1 + qoff] + c1;  // :1166
          vq[ixy + i2 + qoff] = vq[ixy + i2 + qoff] + c2;  // :1168
          vq[ixy + i3 + qoff] = vq[ixy + i3 + qoff] + c3;  // :1170
          vq[ixy + i4 + qoff] = vq[ixy + i4 + qoff] + c4;  // :1172
          vq[ixy - i1 + qoff] = vq[ixy - i1 + qoff] + c1;  // :1174
          vq[ixy - i2 + qoff] = vq[ixy - i2 + qoff] + c2;  // :1176
          vq[ixy - i3 + qoff] = vq[ixy - i3 + qoff] + c3;  // :1178
          vq[ixy - i4 + qoff] = vq[ixy - i4 + qoff] + c4;  // :1180
        }
      }

      // 3.a.2 Near-coast points (:1187-1276): a contribution whose target cell
      // is not sea in MAPSTX (ghost columns: the far side when global, 0 else)
      // goes back to the centre. Store order: centre, +1, -1, +2, -2, +3, -3,
      // +4, -4, different from the central loop's; both binding.
      for (int ip = g.ncent + 1; ip <= nsea; ++ip) {
        const int isea = g.mapcxy[ip - 1];
        const int ixy = g.mapsf[2 * nsea + isea - 1];
        if (mapsta[ixy - 1] <= 0) continue;  // :1191, the dummy MAPSTA, not MAPSTX
        if (g.maptrn[ixy - 1] != 0) {
          vq[ixy + qoff] = aq[ixy + qoff];  // :1192-1193
        } else {
          const Real cgi = s.cg[isea * cgld + ik];
          const Real rd1 = rdi1[isea - 1];                   // :1195
          const Real rd3 = rdi3[isea - 1];                   // :1196
          const Real rd2 = min_r(rdi2[isea - 1] * cgi, r1);  // :1197
          const Real rd4 = min_r(cgi * rdi4[isea - 1], r1);  // :1198
          const Real a = aq[ixy + qoff];
          const int i1 = iixy1[isea - 1];
          const int i2 = iixy2[isea - 1];
          const int i3 = iixy3[isea - 1];
          const int i4 = iixy4[isea - 1];
          vq[ixy + qoff] = vq[ixy + qoff] + (((r3 - rd2) - rd4) * a) / r3;  // :1199-1200
          // :1208 etc. The object emits c2 here as RD2*((1.-RD1)*AQ), the central
          // loop's product set commuted: the same bits.
          const Real c1 = ((rd2 * a) * rd1) / r6;
          const Real c2 = (((r1 - rd1) * a) * rd2) / r6;
          const Real c3 = ((rd4 * a) * rd3) / r6;
          const Real c4 = (((r1 - rd3) * a) * rd4) / r6;
          // :1202-1203, SIGN(NY, IIXY1) as integer sign transfer.
          int ixc = isign(ny, i1);
          int iyc = i1 - ixc;
          {  // :1204-1212, the diagonal needs the target sea and one of its two axial cells sea
            const bool ok = mapstx[ixy + i1 + moff] >= 1 &&
                            !(mapstx[ixy + ixc + moff] <= 0 && mapstx[ixy + iyc + moff] <= 0);
            const int t = ok ? ixy + i1 : ixy;
            vq[t + qoff] = vq[t + qoff] + c1;
          }
          {  // :1213-1221
            const bool ok = mapstx[ixy - i1 + moff] >= 1 &&
                            !(mapstx[ixy - ixc + moff] <= 0 && mapstx[ixy - iyc + moff] <= 0);
            const int t = ok ? ixy - i1 : ixy;
            vq[t + qoff] = vq[t + qoff] + c1;
          }
          {  // :1223-1229, the axial neighbour needs only itself sea
            const int t = (mapstx[ixy + i2 + moff] >= 1) ? ixy + i2 : ixy;
            vq[t + qoff] = vq[t + qoff] + c2;
          }
          {  // :1230-1236
            const int t = (mapstx[ixy - i2 + moff] >= 1) ? ixy - i2 : ixy;
            vq[t + qoff] = vq[t + qoff] + c2;
          }
          // :1238-1239.
          ixc = isign(ny, i3);
          iyc = i3 - ixc;
          {  // :1240-1248
            const bool ok = mapstx[ixy + i3 + moff] >= 1 &&
                            !(mapstx[ixy + ixc + moff] <= 0 && mapstx[ixy + iyc + moff] <= 0);
            const int t = ok ? ixy + i3 : ixy;
            vq[t + qoff] = vq[t + qoff] + c3;
          }
          {  // :1249-1257
            const bool ok = mapstx[ixy - i3 + moff] >= 1 &&
                            !(mapstx[ixy - ixc + moff] <= 0 && mapstx[ixy - iyc + moff] <= 0);
            const int t = ok ? ixy - i3 : ixy;
            vq[t + qoff] = vq[t + qoff] + c3;
          }
          {  // :1259-1265
            const int t = (mapstx[ixy + i4 + moff] >= 1) ? ixy + i4 : ixy;
            vq[t + qoff] = vq[t + qoff] + c4;
          }
          {  // :1266-1272
            const int t = (mapstx[ixy - i4 + moff] >= 1) ? ixy - i4 : ixy;
            vq[t + qoff] = vq[t + qoff] + c4;
          }
        }
      }
    });

    // 3.a.3 Restore boundary data (:1280-1282): active boundary points keep
    // AQ through the averaging, still go through the sweeps.
    lp.par(1, nxy + 1, [&](int ixy) {
      if (mapsta[ixy - 1] == 2) vq[ixy + qoff] = aq[ixy + qoff];
    });

    // 3.a.4 Global closure of the averaging (:1286-1291), IY in order: the
    // first right ghost column into column 1, the left one into column NX;
    // the second right ghost column is not folded and none is cleared. Every
    // index of one IY is IY modulo NY: two IY never touch the same element.
    if (global) {
      lp.par(1, ny + 1, [&](int iy) {
        vq[iy + qoff] = vq[iy + qoff] + vq[nxy + iy + qoff];
        vq[(nx - 1) * ny + iy + qoff] = vq[(nx - 1) * ny + iy + qoff] + vq[iy - ny + qoff];
      });
    }

    // 3.b Propagate. :1301-1306, VQ to straightened space.
    lp.par(1, nsea + 1, [&](int isea) {
      const int ix = g.mapsf[isea - 1];
      const int iy = g.mapsf[nsea + isea - 1];
      const int ixy = g.mapsf[2 * nsea + isea - 1];
      vq[ixy + qoff] = vq[ixy + qoff] * grid2(g.gsqrt, ny, iy, ix);
    });
    // :1312-1360, the two W3QCK3 sweeps of w3pro3md.F90:1315-1322 and
    // :1339-1346, VQ with its ghost columns shared between them. NDSE, NDST are
    // not read by W3QCK3 in this build and have no C++ counterpart.
    if (yfirst) {
      if (p.flcy != 0)
        w3qck3_loops(lp, nx, ny, nx, ny, vlcfly, g.atrny, vq, false, 1, g.mapaxy, g.nact, g.mapy2,
                     g.nmy0, g.nmy1, g.nmy2, fla);
      if (p.flcx != 0)
        w3qck3_loops(lp, nx, ny, nx, ny, vlcflx, g.atrnx, vq, global, ny, g.mapaxy, g.nact,
                     g.mapx2, g.nmx0, g.nmx1, g.nmx2, fla);
    } else {
      if (p.flcx != 0)
        w3qck3_loops(lp, nx, ny, nx, ny, vlcflx, g.atrnx, vq, global, ny, g.mapaxy, g.nact,
                     g.mapx2, g.nmx0, g.nmx1, g.nmx2, fla);
      if (p.flcy != 0)
        w3qck3_loops(lp, nx, ny, nx, ny, vlcfly, g.atrny, vq, false, 1, g.mapaxy, g.nact, g.mapy2,
                     g.nmy0, g.nmy1, g.nmy2, fla);
    }
    // :1368-1373, back to normal space.
    lp.par(1, nsea + 1, [&](int isea) {
      const int ix = g.mapsf[isea - 1];
      const int iy = g.mapsf[nsea + isea - 1];
      const int ixy = g.mapsf[2 * nsea + isea - 1];
      vq[ixy + qoff] = vq[ixy + qoff] / grid2(g.gsqrt, ny, iy, ix);
    });

    // 3.c Update boundaries (:1395-1415), the live #else branch of :1410-1413.
    if (s.flbpi != 0) {
      lp.one([&]() {
        // :1396-1397, DTG*REAL(NTLOC-ITLOC) then /REAL(NTLOC) then subtracted.
        Real rd1 = s.dsec_bpi0 - ((dtg * static_cast<Real>(ntloc - itloc)) / rntloc);
        Real rd2 = s.dsec_bpin;  // :1398
        if (rd2 > r1em3) {       // :1399
          // :1400, maxss %0,%q: q > 0 ? q : 0 (NaN -> 0); minss %1,%t: t < 1 ? t : 1.
          const Real q = rd1 / rd2;
          rd2 = min_r(max_r(q, r0), r1);
          rd1 = r1 - rd2;  // :1401
        } else {
          rd1 = r0;  // :1403
          rd2 = r1;  // :1404
        }
        // :1406-1414, in IBI order: a point listed twice takes the last write.
        // The paren wraps the sum; divide by CG, then multiply by CLATS.
        for (int ibi = 1; ibi <= s.nbi; ++ibi) {
          const int ib = s.isbpi[ibi - 1];
          const int ixy = g.mapsf[2 * nsea + ib - 1];
          const Real b0 = s.bbpi0[ibi * p.nspec + isp - 1];
          const Real bn = s.bbpin[ibi * p.nspec + isp - 1];
          vq[ixy + qoff] = (((b0 * rd1) + (bn * rd2)) / s.cg[ib * cgld + ik]) * g.clats[ib];
        }
      });
    }
    yfirst = !yfirst;  // :1417
  }

  // 4. Store results (:1430-1438): CG/CLATS first, then *VQ, then
  // maxss %0,%x: x > 0 ? x : 0, so -0.0 and NaN become +0.0. Cells with
  // MAPSTA <= 0 keep what the sweeps left there (W3SCAT reads back MAPSTA /= 0).
  lp.par(1, nsea + 1, [&](int isea) {
    const int ixy = g.mapsf[2 * nsea + isea - 1];
    if (mapsta[ixy - 1] > 0) {
      const Real x = (s.cg[isea * cgld + ik] / g.clats[isea]) * vq[ixy + qoff];
      vq[ixy + qoff] = max_r(x, r0);
    }
  });
}

/// W3XYP3 as translated: the whole body on the calling thread, in source order.
KOKKOS_INLINE_FUNCTION void w3xyp3(const Params& p, const Grid& g, const Step& s, int isp,
                                   Real dtg, const int* mapsta, Real* vq, Real* wr, int* wi) {
  w3xyp3_loops(SerialLoops{}, p, g, s, isp, dtg, mapsta, vq, wr, wi);
}

}  // namespace ww::prop
