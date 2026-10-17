// kokkos/src/ww_kokkos/prop_xyp2.hpp
// W3XYP2, ported to Kokkos: third-order ULTIMATE QUICKEST propagation in
// physical space of one spectral bin on a regular (or curvilinear) grid, with
// the swell-age diffusion correction for linear dispersion (w3pro2md.F90:547-548).
// Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3pro2md.F90:508-1264 at NOAA-EMC/WW3
// 03adda91 (the WW3/ fork pin has the same file); author H. L. Tolman,
// curvilinear version W. E. Rogers and T. J. Campbell. W3_S, W3_T* output,
// W3_OMPH directives, the W3_MGP moving grid terms, W3_TDYN, W3_DSS0, W3_XW0,
// W3_XW1 and W3_PDLIB are not in switch_lab_shrd and are not ported; W3_UNO is
// not either, so the sweeps are W3QCK3 (prop_qck3.hpp), not W3QCK1/W3QCK2. A
// kernel never prints: the tripole abort of :761-766 is the caller's check.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and assembly of
// w3pro2md.F90 (WW3's own CMake Release flags, switch_lab_shrd with PR3
// replaced by PR2): no libm call but lroundf (NINT, :827; the C++ calls roundf
// and converts, the same parity for every input), no FMA, no vectorised
// arithmetic (only the zero fills are vector stores), MINVAL/MAXVAL inlined as
// in-order scans, 0.575*GRAV, 0.5*GRAV, DERA*RADIUS and 1./RFAC**2 folded,
// every MIN/MAX in the operand order of the emitted maxss/minss. The whole
// routine runs serially for one ISP, in source order, as W3WAVE calls it
// (w3wavemd.F90:1988, no OpenMP there under this switch).
#pragma once

#include <Kokkos_Core.hpp>

#include "prop_params.hpp"
#include "prop_params2.hpp"
#include "prop_qck3.hpp"
#include "prop_xyp3.hpp"  // xyp3_detail: minval_in_order, maxval_in_order, grid2
#include "real.hpp"
#include "st4_params.hpp"  // fint, nint, max_r, min_r

namespace ww::prop {

/// Reals of work storage w3xyp2 needs, for the Fortran's automatic arrays
/// (w3pro2md.F90:731-741, malloc'ed per call) plus W3QCK3's FLA, in this order:
///   vlcflx, vlcfly, vdxx            (NX+1)*NY each       :731, :733
///   vdyy                            NX*NY                :734
///   vdxy, cxtot                     (NX+1)*NY each       :734, :736
///   cytot                           NX*NY                :736
///   vq_old                          NY*(NX+3)            VQ_OLD(1-NY:NY*(NX+2)), :737
///   vfdifx_fac, vfdify_fac,
///   vfdifc_fac                      NY*(NX+1) each       (1-NY:NX*NY), :739-741
///   fla                             NY*(NX+1)            W3QCK3's FLA (qck3_work_reals)
/// VFDIFX, VFDIFY, VFDIFC (:732-733) are zeroed at :860-862 and never read
/// again: dead, not allocated here.
KOKKOS_INLINE_FUNCTION constexpr int xyp2_work_reals(int nx, int ny) {
  return 5 * (nx + 1) * ny + 2 * nx * ny + ny * (nx + 3) + 3 * ny * (nx + 1) +
         qck3_work_reals(nx, ny);
}

/// W3XYP2(ISP, DTG, MAPSTA, MAPFS, VQ, VGX, VGY) for one spectral bin. VGX and
/// VGY are read only under W3_MGP (:779-780, :801-802, :890-891) and are left
/// out.
///
/// Fortran arrays arrive 0-based; the explicit lower bounds become offsets:
///   mapsta -> MAPSTA(NY*NX):           MAPSTA(i) = mapsta[i - 1]
///   mapfs  -> MAPFS(NY*NX):            MAPFS(i)  = mapfs[i - 1], read at :1140
///   vq     -> VQ(1-NY : NY*(NX+2)):    VQ(i)     = vq[i + ny - 1], the caller's
///             FIELD, ghost columns included: NY*(NX+3) entries. Unlike W3XYP3,
///             W3XYP2 never clears VQ: without closure W3QCK3 reads the ghost
///             columns as the caller left them.
///   wr     -> xyp2_work_reals entries, caller-owned; what the Fortran zeroes
///             (:858-867, :1107-1117) is zeroed here, nothing else.
/// p, g, s: prop_params.hpp; g2, s2: prop_params2.hpp. The caller refuses
/// p.iclose == kIcloseTrpl (:761-766, EXTCDE), checks s.nbi > 0 only when
/// s.flbpi, and computes the three DSEC21 values of Step.
///
/// The cross-term stencil reads VQ_OLD(IXY-NY-1) and VFDIFC_FAC(IXY-NY-1),
/// one below their lower bound 1-NY for IXY = 1; W3MAP2 lists in MAPAXY only
/// IY = 2..NY-1 (w3pro2md.F90:380-388), so with its maps the lowest index read
/// is 1-NY. A MAPAXY that lists row 1 is out of contract here as in the Fortran.
///
/// Two sweeps that share VQ and a scatter into VQ from VQ_OLD: the routine runs
/// as one serial body per ISP, as the Fortran. A parallel version over ISEA is
/// a change of structure: ask the owner, own PR.
KOKKOS_INLINE_FUNCTION void w3xyp2(const Params& p, const Grid& g, const Step& s, const Grid2& g2,
                                   const Step2& s2, int isp, Real dtg, const int* mapsta,
                                   const int* mapfs, Real* vq, Real* wr) {
  using ww::st4::fint;
  using ww::st4::max_r;
  using ww::st4::min_r;
  using ww::st4::nint;
  using xyp3_detail::grid2;
  constexpr Real r0 = static_cast<Real>(0.);
  constexpr Real r1 = static_cast<Real>(1.);
  constexpr Real r2 = static_cast<Real>(2.0);
  constexpr Real r025 = static_cast<Real>(0.25);
  constexpr Real r10 = static_cast<Real>(10.);
  constexpr Real r12 = static_cast<Real>(12.);
  constexpr Real r33 = static_cast<Real>(3.3);
  constexpr Real r23 = static_cast<Real>(2.3);
  constexpr Real r1em3 = static_cast<Real>(0.001);
  constexpr Real r09999 = static_cast<Real>(0.9999);
  static_assert(r33 == 3.2999999523162841796875e+0f, "3.3 as REAL");
  static_assert(r23 == 2.2999999523162841796875e+0f, "2.3 as REAL");
  static_assert(r1em3 == 1.000000047497451305389404296875e-3f, "0.001 as REAL");
  static_assert(r09999 == 9.9989998340606689453125e-1f, "0.9999 as REAL");

  const int nx = p.nx;
  const int ny = p.ny;
  const int nsea = p.nsea;
  const int nxy = nx * ny;
  const int nxy1 = (nx + 1) * ny;  // VLCFLX, VLCFLY, VDXX, VDXY, CXTOT, the FAC arrays
  const int nq = ny * (nx + 3);    // VQ, VQ_OLD with the three ghost columns
  const int qoff = ny - 1;         // VQ(i), VQ_OLD(i) -> [i + qoff]
  const int foff = ny - 1;         // VFDIF?_FAC(i) -> [i + foff]
  const int cgld = p.nk + 2;       // CG(IK,ISEA), WN(IK,ISEA) -> [ISEA*cgld + IK]

  // Work arrays, in the order xyp2_work_reals documents.
  Real* const vlcflx = wr;
  Real* const vlcfly = vlcflx + nxy1;
  Real* const vdxx = vlcfly + nxy1;
  Real* const vdyy = vdxx + nxy1;
  Real* const vdxy = vdyy + nxy;
  Real* const cxtot = vdxy + nxy1;
  Real* const cytot = cxtot + nxy1;
  Real* const vq_old = cytot + nxy;
  Real* const fx = vq_old + nq;  // VFDIFX_FAC
  Real* const fy = fx + nxy1;    // VFDIFY_FAC
  Real* const fc = fy + nxy1;    // VFDIFC_FAC
  Real* const fla = fc + nxy1;

  // 1.a Set constants (:770-849).
  const bool global = p.iclose != kIcloseNone;  // :770
  const int ith = 1 + (isp - 1) % p.nth;        // :771
  const int ik = 1 + (isp - 1) / p.nth;         // :772
  const Real cg0 = kCg0Fac / g.sig[1];          // :774, 0.575*GRAV folded
  const Real cga = kCg0Fac / g.sig[ik];         // :775
  const Real ecosi = g.ecos[ith - 1];
  const Real esini = g.esin[ith - 1];
  const Real cgx = ecosi * cga;  // :776, emitted ECOS(ITH)*CGA; the same bits as CGA*ECOS
  const Real cgy = esini * cga;  // :777

  // :783-807. The compiler kept only ABS(CGX), ABS(CGY) after the IFs, since
  // CGX and CGY are read nowhere else than inside ABS at :809.
  Real acgx;
  Real acgy;
  Real cxc;
  Real cyc;
  if (p.flcur != 0) {
    const Real cxmin = xyp3_detail::minval_in_order(s.cx + 1, nsea);  // :784
    const Real cxmax = xyp3_detail::maxval_in_order(s.cx + 1, nsea);  // :785
    const Real cymin = xyp3_detail::minval_in_order(s.cy + 1, nsea);  // :786
    const Real cymax = xyp3_detail::maxval_in_order(s.cy + 1, nsea);  // :787
    // :788-797 as emitted: addss, andps, then maxss %b,%a: a > b ? a : b, so a
    // tie or a NaN picks the CXMAX branch, as the Fortran's IF does.
    const Real ax = Kokkos::fabs(cgx + cxmin);
    const Real bx = Kokkos::fabs(cgx + cxmax);
    acgx = max_r(ax, bx);
    const Real ay = Kokkos::fabs(cgy + cymin);
    const Real by = Kokkos::fabs(cgy + cymax);
    acgy = max_r(ay, by);
    // :798-799, maxss %|cxmax|,%|cxmin|: |cxmin| > |cxmax| ? |cxmin| : |cxmax|.
    cxc = max_r(Kokkos::fabs(cxmin), Kokkos::fabs(cxmax));
    cyc = max_r(Kokkos::fabs(cymin), Kokkos::fabs(cymax));
  } else {
    acgx = Kokkos::fabs(cgx);
    acgy = Kokkos::fabs(cgy);
    cxc = r0;  // :805
    cyc = r0;  // :806
  }
  // :809, MAX(ABS(CGX), ABS(CGY), CXC, CYC, 0.001*CG0) emitted as a tree, the
  // same for both branches above (object 0x2366-0x23a3): maxss %cyc,%cxc;
  // maxss %|cgy|,%|cgx|; maxss %t2,%e; maxss %t1,%t3; then *0.9999. Each
  // maxss %b,%a is a > b ? a : b. The last operand order differs from W3XYP3's.
  const Real t2 = max_r(cxc, cyc);
  const Real t1 = max_r(acgx, acgy);
  const Real e = cg0 * r1em3;
  const Real t3 = max_r(e, t2);
  const Real cgn = max_r(t3, t1) * r09999;
  // :811, DTCFL*CG0 first, then / CGN, then DTG / that; INT is cvttss2si.
  const int ntloc = 1 + fint(dtg / ((p.dtcfl * cg0) / cgn));
  const Real rntloc = static_cast<Real>(ntloc);  // one REAL(NTLOC), shared by :812 and :1075
  const Real dtloc = dtg / rntloc;                // :812
  Real dtrad = dtloc;                             // :813
  if (p.flagll != 0) dtrad = dtloc / kDeraRadius;  // :814, DERA*RADIUS folded
  // :816-822, both branches constants: RFAC = DERA*RADIUS, DFAC = 1./RFAC**2.
  const Real rfac = (p.flagll != 0) ? kDeraRadius : r1;
  const Real dfac = (p.flagll != 0) ? kXyp2Dfac : r1;
  // :824-827. DTTST is INTEGER: the REAL seconds truncated, converted back for
  // the division; NINT is lroundf; MOD(.,2) == 0 is the parity test.
  const int dttst = fint(s.dsec_day);
  bool yfirst = (nint(static_cast<Real>(dttst) / dtg) % 2) == 0;

  // :838-849. CGD, DSSD, DNND are set and read only when DTME /= 0 (a NaN
  // DTME counts as nonzero, ucomiss/jne); the zero here is never read.
  Real cgd = r0;
  Real dssd = r0;
  Real dnnd = r0;
  if (g2.dtme != r0) {
    const Real dfrr = p.xfr - r1;  // :839
    cgd = kCgdFac / g.sig[ik];     // :841, 0.5*GRAV folded
    // :842-843, **2 one multiply of the paren node, emitted DTME*x2 (same
    // bits as x2*DTME), then /12. CELLP = 10. (:840) is folded into :954.
    const Real a = dfrr * cgd;
    dssd = (g2.dtme * (a * a)) / r12;
    const Real b = p.dth * cgd;
    dnnd = (g2.dtme * (b * b)) / r12;
  }

  // 1.b Initialize arrays (:858-867), the live ones, in source order.
  for (int i = 0; i < nxy1; ++i) vlcflx[i] = r0;  // :858
  for (int i = 0; i < nxy1; ++i) vlcfly[i] = r0;  // :859
  for (int i = 0; i < nxy1; ++i) vdxx[i] = r0;    // :863
  for (int i = 0; i < nxy; ++i) vdyy[i] = r0;     // :864
  for (int i = 0; i < nxy1; ++i) vdxy[i] = r0;    // :865
  for (int i = 0; i < nxy1; ++i) cxtot[i] = r0;   // :866
  for (int i = 0; i < nxy; ++i) cytot[i] = r0;    // :867

  // 2.a Velocities (:884-932). CXTOT, CYTOT by cell IXY; CG, CX, CLATS by ISEA.
  for (int isea = 1; isea <= nsea; ++isea) {
    const int ixy = g.mapsf[2 * nsea + isea - 1];
    const Real cgi = s.cg[isea * cgld + ik];
    const Real clat = g.clats[isea];
    vq[ixy + qoff] = (vq[ixy + qoff] / cgi) * clat;  // :886, divide then multiply
    cxtot[ixy - 1] = (ecosi * cgi) / clat;            // :887
    cytot[ixy - 1] = esini * cgi;                     // :888
  }
  if (p.flcur != 0) {
    for (int isea = 1; isea <= nsea; ++isea) {
      const int ixy = g.mapsf[2 * nsea + isea - 1];
      cxtot[ixy - 1] = cxtot[ixy - 1] + (s.cx[isea] / g.clats[isea]);  // :910
      cytot[ixy - 1] = cytot[ixy - 1] + s.cy[isea];                     // :911
    }
  }
  for (int isea = 1; isea <= nsea; ++isea) {
    const int ix = g.mapsf[isea - 1];
    const int iy = g.mapsf[nsea + isea - 1];
    const int ixy = g.mapsf[2 * nsea + isea - 1];
    const Real cxt = cxtot[ixy - 1];
    const Real cyt = cytot[ixy - 1];
    const Real cp = cxt * grid2(g.dpdx, ny, iy, ix) + cyt * grid2(g.dpdy, ny, iy, ix);  // :928
    const Real cq = cxt * grid2(g.dqdx, ny, iy, ix) + cyt * grid2(g.dqdy, ny, iy, ix);  // :929
    vlcflx[ixy - 1] = cp * dtrad;  // :930
    vlcfly[ixy - 1] = cq * dtrad;  // :931
  }

  // 2.b Diffusion coefficients (:940-983).
  if (g2.dtme != r0) {
    const Real ec2 = ecosi * ecosi;  // ECOS(ITH)**2, one multiply, hoisted by the compiler
    const Real es2 = esini * esini;
    for (int isea = 1; isea <= nsea; ++isea) {
      const int ix = g.mapsf[isea - 1];
      const int iy = g.mapsf[nsea + isea - 1];
      const int ixy = g.mapsf[2 * nsea + isea - 1];
      // :951-952, emitted minss ATRNX(IXY,-1),ATRNX(IXY,1); the same for ATRNY;
      // minss %my,%mx; then comiss: m > 0.95, false for a NaN m.
      const Real ax1 = g.atrnx[2 * nxy + ixy - 1];  // ATRNX(IXY, 1)
      const Real axm = g.atrnx[ixy - 1];            // ATRNX(IXY,-1)
      const Real ay1 = g.atrny[2 * nxy + ixy - 1];
      const Real aym = g.atrny[ixy - 1];
      const Real mx = min_r(ax1, axm);
      const Real my = min_r(ay1, aym);
      if (min_r(mx, my) > kXyp2TrnMin) {
        // :953-954, minss %hq,%hp: hp < hq ? hp : hq; CGD times it, then / CELLP.
        const Real hp = grid2(g2.hpfac, ny, iy, ix) * rfac;
        const Real hq = grid2(g2.hqfac, ny, iy, ix) * rfac;
        const Real dcell = (cgd * min_r(hp, hq)) / r10;
        // :955, ((3.3*U10)*WN)/SIG - 2.3; :956 minss %1,%x then maxss %0,%x:
        // a NaN XWIND becomes 1, then stays 1.
        Real xwind = (((r33 * s2.u10[isea - 1]) * s2.wn[isea * cgld + ik]) / g.sig[ik]) - r23;
        xwind = max_r(min_r(xwind, r1), r0);
        // :963, (CLATS/CLATMN)**2 one multiply, then minss %1,%t.
        const Real cr = g.clats[isea] / g2.clatmn;
        const Real tfac = min_r(cr * cr, r1);
        // :964, :968, ((1.-XWIND)*DSSD)*TFAC added to XWIND*DCELL, which the
        // object computes once for both lines (the same bits).
        const Real xd = xwind * dcell;
        const Real omx = r1 - xwind;
        const Real dss = xd + ((omx * dssd) * tfac);
        const Real dnn = xd + ((omx * dnnd) * tfac);
        const Real clat = g.clats[isea];
        // :970-974: the paren sum, then *DTLOC; :972 then / CLATS**2 (one
        // multiply); :973 ((DTLOC*(DSS-DNN))*ESIN)*ECOS / CLATS, emitted with
        // the products commuted.
        vdxx[ixy - 1] = ((dss * ec2) + (dnn * es2)) * dtloc;
        vdyy[ixy - 1] = (((dss * es2) + (dnn * ec2)) * dtloc) / (clat * clat);
        vdxy[ixy - 1] = ((((dss - dnn) * dtloc) * esini) * ecosi) / clat;
      }
    }
  }

  // 3. Loop over sub-steps (:987-1212).
  for (int itloc = 1; itloc <= ntloc; ++itloc) {
    // 3.a Propagate. :995-1000, VQ to straightened space.
    for (int isea = 1; isea <= nsea; ++isea) {
      const int ix = g.mapsf[isea - 1];
      const int iy = g.mapsf[nsea + isea - 1];
      const int ixy = g.mapsf[2 * nsea + isea - 1];
      vq[ixy + qoff] = vq[ixy + qoff] * grid2(g.gsqrt, ny, iy, ix);
    }
    // :1006-1054, the two W3QCK3 sweeps in YFIRST order, VQ with its ghost
    // columns shared between them. NDSE, NDST are not read by W3QCK3 in this
    // build and have no C++ counterpart.
    if (yfirst) {
      if (p.flcy != 0)
        w3qck3(nx, ny, nx, ny, vlcfly, g.atrny, vq, false, 1, g.mapaxy, g.nact, g.mapy2, g.nmy0,
               g.nmy1, g.nmy2, fla);
      if (p.flcx != 0)
        w3qck3(nx, ny, nx, ny, vlcflx, g.atrnx, vq, global, ny, g.mapaxy, g.nact, g.mapx2,
               g.nmx0, g.nmx1, g.nmx2, fla);
    } else {
      if (p.flcx != 0)
        w3qck3(nx, ny, nx, ny, vlcflx, g.atrnx, vq, global, ny, g.mapaxy, g.nact, g.mapx2,
               g.nmx0, g.nmx1, g.nmx2, fla);
      if (p.flcy != 0)
        w3qck3(nx, ny, nx, ny, vlcfly, g.atrny, vq, false, 1, g.mapaxy, g.nact, g.mapy2, g.nmy0,
               g.nmy1, g.nmy2, fla);
    }
    // :1060-1065, back to normal space.
    for (int isea = 1; isea <= nsea; ++isea) {
      const int ix = g.mapsf[isea - 1];
      const int iy = g.mapsf[nsea + isea - 1];
      const int ixy = g.mapsf[2 * nsea + isea - 1];
      vq[ixy + qoff] = vq[ixy + qoff] / grid2(g.gsqrt, ny, iy, ix);
    }

    // 3.b Update boundaries (:1073-1094), the live #else branch of :1087-1092.
    if (s.flbpi != 0) {
      // :1074-1075, DTG*REAL(NTLOC-ITLOC) then /REAL(NTLOC) then subtracted.
      Real rd1 = s.dsec_bpi0 - ((dtg * static_cast<Real>(ntloc - itloc)) / rntloc);
      Real rd2 = s.dsec_bpin;  // :1076
      if (rd2 > r1em3) {       // :1077
        // :1078, maxss %0,%q: q > 0 ? q : 0 (NaN -> 0); minss %1,%t: t < 1 ? t : 1.
        const Real q = rd1 / rd2;
        rd2 = min_r(max_r(q, r0), r1);
        rd1 = r1 - rd2;  // :1079
      } else {
        rd1 = r0;  // :1081
        rd2 = r1;  // :1082
      }
      // :1084-1093, in IBI order: a point listed twice takes the last write.
      // The paren wraps the sum; divide by CG, then multiply by CLATS.
      for (int ibi = 1; ibi <= s.nbi; ++ibi) {
        const int ib = s.isbpi[ibi - 1];
        const int ixy = g.mapsf[2 * nsea + ib - 1];
        const Real b0 = s.bbpi0[ibi * p.nspec + isp - 1];
        const Real bn = s.bbpin[ibi * p.nspec + isp - 1];
        vq[ixy + qoff] = (((b0 * rd1) + (bn * rd2)) / s.cg[ib * cgld + ik]) * g.clats[ib];
      }
    }

    // 3.c Diffusion correction (:1098-1209).
    if (g2.dtme != r0) {
      // :1100-1104, column 1 into the first right ghost column, IY in order.
      if (global) {
        for (int iy = 1; iy <= ny; ++iy) vq[iy + nxy + qoff] = vq[iy + qoff];
      }
      // :1107-1121, each FAC zeroed over its full bounds, then 1 at the listed
      // cells (MAPX2's first NMX0, MAPY2's first NMY0, MAPXY's NMXY).
      for (int i = 0; i < nxy1; ++i) fx[i] = r0;
      for (int ip = 1; ip <= g.nmx0; ++ip) fx[g.mapx2[ip - 1] + foff] = r1;
      for (int i = 0; i < nxy1; ++i) fy[i] = r0;
      for (int ip = 1; ip <= g.nmy0; ++ip) fy[g.mapy2[ip - 1] + foff] = r1;
      for (int i = 0; i < nxy1; ++i) fc[i] = r0;
      for (int ip = 1; ip <= g2.nmxy; ++ip) fc[g2.mapxy[ip - 1] + foff] = r1;
      // :1122-1128, column NX into the left ghost column of FX and FC.
      if (global) {
        const int iy0 = (nx - 1) * ny;
        for (int iy = 1; iy <= ny; ++iy) {
          fx[iy - ny + foff] = fx[iy + iy0 + foff];
          fc[iy - ny + foff] = fc[iy + iy0 + foff];
        }
      }
      // :1131, over the full bounds, ghost columns included.
      for (int i = 0; i < nq; ++i) vq_old[i] = vq[i];

      // :1138-1203. Reads only VQ_OLD, writes VQ(IXY) once per active cell.
      for (int ip = 1; ip <= g.nact; ++ip) {
        const int ixy = g.mapaxy[ip - 1];
        const int isea = mapfs[ixy - 1];  // :1140, the dummy MAPFS
        const int ix = g.mapsf[isea - 1];
        const int iy = g.mapsf[nsea + isea - 1];
        const int i = ixy + qoff;  // VQ_OLD(IXY)
        const int k = ixy + foff;  // FAC(IXY)
        const Real q0 = vq_old[i];
        // :1162-1185, every sum left to right as written, each product as one
        // multiply (the object commutes some factors: the same bits).
        const Real qxx = (((fx[k] * vq_old[i + ny]) - (fx[k] * q0)) - (fx[k - ny] * q0)) +
                         (fx[k - ny] * vq_old[i - ny]);
        const Real qyy = (((fy[k] * vq_old[i + 1]) - (fy[k] * q0)) - (fy[k - 1] * q0)) +
                         (fy[k - 1] * vq_old[i - 1]);
        Real qxy = fc[k] * q0;
        qxy = qxy + fc[k - ny - 1] * q0;
        qxy = qxy - fc[k - 1] * q0;
        qxy = qxy - fc[k - ny] * q0;
        qxy = qxy + fc[k - ny] * vq_old[i + 1];
        qxy = qxy - fc[k] * vq_old[i + 1];
        qxy = qxy + fc[k - 1] * vq_old[i - 1];
        qxy = qxy - fc[k - ny - 1] * vq_old[i - 1];
        qxy = qxy + fc[k - 1] * vq_old[i + ny];
        qxy = qxy - fc[k] * vq_old[i + ny];
        qxy = qxy + fc[k - ny] * vq_old[i - ny];
        qxy = qxy - fc[k - ny - 1] * vq_old[i - ny];
        qxy = qxy + fc[k] * vq_old[i + ny + 1];
        qxy = qxy - fc[k - 1] * vq_old[i + ny - 1];
        qxy = qxy + fc[k - ny - 1] * vq_old[i - ny - 1];
        qxy = qxy - fc[k - ny] * vq_old[i - ny + 1];
        qxy = r025 * qxy;  // :1188
        // :1190-1199. Each X*X*Q is (X*X)*Q; 2.0*X*Y*Q is ((2.0*X)*Y)*Q, the
        // 2.0*X emitted as X*2.0; 2.0*VDXY*(...) is (2.0*VDXY)*(...); the three
        // terms added left to right.
        const Real dpdx = grid2(g.dpdx, ny, iy, ix);
        const Real dqdx = grid2(g.dqdx, ny, iy, ix);
        const Real dpdy = grid2(g.dpdy, ny, iy, ix);
        const Real dqdy = grid2(g.dqdy, ny, iy, ix);
        const Real sx = (((dpdx * dpdx) * qxx) + (((r2 * dpdx) * dqdx) * qxy)) + ((dqdx * dqdx) * qyy);
        const Real sy = (((dpdy * dpdy) * qxx) + (((r2 * dpdy) * dqdy) * qxy)) + ((dqdy * dqdy) * qyy);
        const Real sc = (((dpdx * dpdy) * qxx) + ((dqdx * dqdy) * qyy)) +
                        (((dpdx * dqdy) + (dqdx * dpdy)) * qxy);
        const Real dvq = ((vdxx[ixy - 1] * sx) + (vdyy[ixy - 1] * sy)) + ((r2 * vdxy[ixy - 1]) * sc);
        vq[i] = q0 + (dvq * dfac);  // :1201
      }
    }
    yfirst = !yfirst;  // :1211
  }

  // 4. Store results (:1224-1234): CG/CLATS first, then *VQ, then
  // maxss %0,%x: x > 0 ? x : 0, so -0.0 and NaN become +0.0. Cells with
  // MAPSTA <= 0 keep what the sweeps left there.
  for (int isea = 1; isea <= nsea; ++isea) {
    const int ixy = g.mapsf[2 * nsea + isea - 1];
    if (mapsta[ixy - 1] > 0) {
      const Real x = (s.cg[isea * cgld + ik] / g.clats[isea]) * vq[ixy + qoff];
      vq[ixy + qoff] = max_r(x, r0);
    }
  }
}

}  // namespace ww::prop
