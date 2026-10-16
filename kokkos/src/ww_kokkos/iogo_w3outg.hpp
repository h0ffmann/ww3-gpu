// kokkos/src/ww_kokkos/iogo_w3outg.hpp
// W3OUTG: the gridded output parameters of every local sea point from its action spectrum A:
// Hs, mean periods and wavelength, mean and peak frequency and direction, directional spread,
// near-bottom orbital amplitude and velocity, radiation stresses, Stokes drift and transport, mean
// square slopes, the second-order microseism pressure source, the space-time extremes, and the
// copy of the partition table into the partition fields. Phase 1, translated, not regtested.
// WW3 heritage: WW3/model/src/w3iogomd.F90:1201-2380 at h0ffmann/WW3 8e433f99; author H. L. Tolman
// and the authors of its history block. Evidence: kokkos/port_report/W3OUTG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement switches/switch_lab_shrd compiles (SHRD; no OMPG, O8, O9, IG1, T, S):
// sections 1-4 and 6, the space-time extremes (:1694-1931) and the calls of :2347-2362, SKEWNESS
// through iogo_skewness.hpp. Not ported, by switch: the W3_IG1 HSIG (:1556-1558), the W3_O9
// negative Hs (:1995-2003), the W3_O8 low-Hs filter (:2058-2068), the W3_T prints (:2219-2237,
// :2368-2375), W3_S STRACE, and the W3_OMPG directives (they do not change a point's arithmetic).
// Not translated: CALC_U3STOKES (:4112-4341) and CALC_WBT (:4367-4607) are hooks the caller
// provides (W3OutgHooks); a requested hook that is missing aborts. CALC_WBTv2 of W3SNL5
// (snl5_calc_wbtv2.hpp) is a different routine (fixed BETA, no DMIN, another SIGP threshold).
// Locals whose value reaches no output are not computed: NKH (:1512-1513), FXPMC (:1388),
// ETMSSL and ETMSSCL (:1577-1589), ETUSCX and ETUSCY (:1617-1618), UBS (:1590, :1975),
// SCREST (:1809-1811).
#pragma once

#include <cstdint>

#include <Kokkos_Core.hpp>

#include "iogo_skewness.hpp"
#include "prop_params.hpp"
#include "real.hpp"
#include "st4_libmvec.hpp"
#include "st4_params.hpp"

#if defined(WW_ST4_HAVE_LIBMVEC)
extern "C" __m128 _ZGVbN4v_sinf(__m128);
extern "C" __m128 _ZGVbN4v_cosf(__m128);
#endif

namespace ww::iogo {

namespace w3outg_detail {
using ww::prop::kDera;
using ww::st4::kDwat;
using ww::st4::kGrav;
using ww::st4::kPi;
using ww::st4::kRade;
using ww::st4::kTpi;
using ww::st4::kTpiInv;

// W3ODATMD PARAMETER NOGRP (w3odatmd.F90:330).
inline constexpr int kNogrp = 10;

// Products of PARAMETERs that gfortran folds, checked against the object's bits.
inline constexpr Real k04Tpi = static_cast<Real>(0.4) * kTpi;     // 0.4*TPI :1577
inline constexpr Real kTpi2 = kTpi * kTpi;                         // TPI**2 :1597
inline constexpr Real kGrav2 = kGrav * kGrav;                      // GRAV**2 :1616
inline constexpr Real kRadeHalf = kRade * static_cast<Real>(0.5);  // RADE*0.5 :1682
inline constexpr Real kRade001 = static_cast<Real>(0.01) * kRade;  // 0.01*RADE :2010
inline constexpr Real kDwatGrav = kDwat * kGrav;                   // DWAT*GRAV :2038
inline constexpr Real kSqrtTpi = 0x1.40d932p+1f;                   // SQRT(TPI) :1836
inline constexpr Real kSqrt6 = 0x1.3988e2p+1f;                     // SQRT(6.) :1883
inline constexpr Real kPiSqrt6 = kPi / kSqrt6;                     // PI/SQRT(6.) :1914
static_assert(k04Tpi == 2.51327419281005859375f);
static_assert(kTpi2 == 3.9478420257568359375e+1f);
static_assert(kGrav2 == 9.615763092041015625e+1f);
static_assert(kRade == 5.72957763671875e+1f && kRadeHalf == 2.864788818359375e+1f);
static_assert(kRade001 == 5.729577541351318359375e-1f);
static_assert(kDwatGrav == 9.806e+3f);
static_assert(kSqrtTpi == 2.5066282749176025390625f && kSqrt6 == 2.4494898319244384765625f);
static_assert(kPiSqrt6 == 1.28254985809326171875f);
static_assert(kDera == 1.745329238474369049072265625e-2f);

/// MAX/MIN of two REALs as the object computes them: maxss (minss) with `a` the destination, so a
/// NaN or a tie of signed zeros gives `b`. Each call site passes the object's operand order.
KOKKOS_INLINE_FUNCTION Real gt_sel(Real a, Real b) { return a > b ? a : b; }
KOKKOS_INLINE_FUNCTION Real lt_sel(Real a, Real b) { return a < b ? a : b; }

/// SIN/COS(THMP(JSEA)) of :1733-1745. The object vectorises that JSEA loop by fours with
/// _ZGVbN4v_sinf/_ZGVbN4v_cosf when NSEAL >= 4, for JSEA <= 4*(NSEAL/4), and calls the scalar
/// sincosf for the rest; `lane` says which. A lane's result depends on its own input only, so the
/// value is broadcast to the four lanes.
KOKKOS_INLINE_FUNCTION void thmp_sincos(Real x, bool lane, Real& s, Real& c) {
  (void)lane;
  bool done = false;
#if defined(WW_ST4_HAVE_LIBMVEC)
  KOKKOS_IF_ON_HOST((if (lane) {
    alignas(16) float rs[4], rc[4];
    _mm_store_ps(rs, _ZGVbN4v_sinf(_mm_set1_ps(x)));
    _mm_store_ps(rc, _ZGVbN4v_cosf(_mm_set1_ps(x)));
    s = rs[0];
    c = rc[0];
    done = true;
  }))
#endif
  if (!done) {
    s = Kokkos::sin(x);
    c = Kokkos::cos(x);
  }
}
}  // namespace w3outg_detail

/// The scalars W3OUTG reads (W3GDATMD, W3ODATMD, CONSTANTS' UNDEF).
struct W3OutgScalars {
  int nk;        ///< NK
  int nth;       ///< NTH
  int nseal;     ///< NSEAL; under W3_SHRD NSEAL = NSEA and ISEA = JSEA
  int noswll;    ///< NOSWLL
  Real dth;      ///< DTH
  Real xfr;      ///< XFR
  Real fte;      ///< FTE
  Real ftwl;     ///< FTWL
  Real fttr;     ///< FTTR
  Real undef;    ///< UNDEF, a variable of CONSTANTS (constants.F90:85), not a PARAMETER
  Real stexu;    ///< STEXU
  Real steyu;    ///< STEYU
  Real stedu;    ///< STEDU
  Real wscut;    ///< WSCUT
  int p2msf2;    ///< P2MSF(2)
  int p2msf3;    ///< P2MSF(3)
  int e3df2[5];  ///< E3DF(2,1:5)
  int e3df3[5];  ///< E3DF(3,1:5)
};

/// The flags of the argument list and of W3ODATMD, on the host. flogrd and flogr2 are
/// FLOGRD(NOGRP,NGRPP) and FLOGR2(NOGRP,NGRPP), LOGICAL(4), column-major; true is nonzero.
struct W3OutgFlags {
  bool flpart;
  bool floutg;
  bool floutg2;
  const std::int32_t* flogrd;
  const std::int32_t* flogr2;
};

/// The routines W3OUTG calls that are not translated here. They run on the host after a fence, in
/// the Fortran's order: CALC_U3STOKES(A,1) if FLOLOC(6,8) (:2347-2349), CALC_U3STOKES(A,2) if
/// FLOLOC(6,12) (:2351-2353), SKEWNESS (translated, :2355-2357), CALC_WBT(A) if FLOLOC(2,17)
/// (:2362). A null hook whose flag is set aborts.
struct W3OutgHooks {
  void (*calc_u3stokes)(void* ctx, int uss_switch) = nullptr;
  void (*calc_wbt)(void* ctx) = nullptr;
  void* ctx = nullptr;
};

/// The arrays. Each view is the Fortran array with index 0 at its lower bound, LayoutLeft:
///   a           A(NTH,NK,0:NSEAL)              a(ith-1, ik-1, jsea)
///   cg, wn      CG, WN(0:NK+1,0:NSEA)          cg(ik, isea)
///   dw          DW(0:NSEA)                     dw(isea)
///   sig         SIG(0:NK+1)                    sig(ik)
///   dden, dsii  DDEN, DSII(NK)                 dsii(ik-1)
///   ecos, esin, ec2, es2, esc                  ecos(ith-1)
///   mapsf       MAPSF(NSEA,3); mapsta MAPSTA(NY,NX)
///   icprt       ICPRT(NSEALM+1,2); dtprt DTPRT(DIMP,:)
///   ebd         work, the local EBD(NK,NSEAL)  ebd(ik-1, jsea-1)
///   1-D fields  (NSEALM), field(jsea-1); all of the same extent
///   ef .. sth2m EF..STH2M(NSEALM,E3DF(2,k):E3DF(3,k)), ef(jsea-1, ik-E3DF(2,1))
///   p2sms       P2SMS(NSEALM,P2MSF(2):P2MSF(3)), p2sms(jsea-1, ik-P2MSF(2))
///   phs .. pep  P*(NSEALM,0:NOSWLL), phs(jsea-1, j); pwst, pnr (NSEALM)
///   skew, embia1, embia2 (NSEALM); fac0..fac3 SECONDHH's FAC0..FAC3 for NKHF = NK+13, which the
///   Fortran SKEWNESS rebuilds on every call (iogo_skewness.hpp); skw_* its work, one column per
///   point: skw_f2 (NTH*NKHF, NSEAL), skw_sighf, skw_dfimhf, skw_fak (NKHF, NSEAL).
template <class ExecSpace>
struct W3OutgArgs {
  using mem = typename ExecSpace::memory_space;
  template <class T>
  using V1 = Kokkos::View<T*, Kokkos::LayoutLeft, mem>;
  template <class T>
  using V2 = Kokkos::View<T**, Kokkos::LayoutLeft, mem>;
  template <class T>
  using V3 = Kokkos::View<T***, Kokkos::LayoutLeft, mem>;

  V3<const Real> a;
  V2<const Real> cg, wn;
  V1<const Real> dw, sig, dden, dsii, ecos, esin, ec2, es2, esc;
  V2<const int> mapsf, mapsta, icprt;
  V2<const Real> dtprt;

  V2<Real> ebd;

  V1<Real> hs, wlm, t02, t0m1, t01, fp0, thm, ths, thp0, hsig, qkk, qp, wbt;
  V1<Real> stmaxe, stmaxd, hcmaxe, hmaxe, hcmaxd, hmaxd;
  V1<Real> aba, abd, uba, ubd, sxx, syy, sxy, ussx, ussy, tusx, tusy;
  V1<Real> mssx, mssy, mssd, mscx, mscy, mscd, prms, tpms, bhd, cge;
  V2<Real> ef, th1m, sth1m, th2m, sth2m, p2sms;

  V2<Real> phs, ptp, plp, pdir, psi, pws, pthp0, pqp, ppe, pgw, psw, ptm1, pt1, pt2, pep;
  V1<Real> pwst, pnr;

  V1<Real> skew, embia1, embia2;
  V1<const Real> fac0, fac1, fac2, fac3;
  V2<Real> skw_f2, skw_sighf, skw_dfimhf, skw_fak;
};

/// The FLOLOC entries the point kernel tests, and the space-time test of :1695-1696.
struct W3OutgLoc {
  bool f3[5];  ///< FLOLOC(3,1:5): EF, TH1M, STH1M, TH2M, STH2M
  bool f69;    ///< FLOLOC(6,9): P2SMS
  bool ste;    ///< (STEXU > 0 .AND. STEYU > 0) .OR. STEDU > 0
};

/// Sections 2-4 of W3OUTG for JSEA = jsea (1-based), in the Fortran's order. The Fortran runs
/// each section as a loop over JSEA; a point reads only its own data, and every sum over ITH and IK
/// keeps the Fortran's order for the point.
template <class ExecSpace>
KOKKOS_INLINE_FUNCTION void w3outg_point(const W3OutgArgs<ExecSpace>& g, const W3OutgScalars& s,
                                         const W3OutgLoc& l, int jsea) {
  using namespace w3outg_detail;
  const Real zero = static_cast<Real>(0.);
  const Real half = static_cast<Real>(0.5);
  const Real one = static_cast<Real>(1.);
  const Real two = static_cast<Real>(2.);
  const Real k1em7 = static_cast<Real>(1.E-7);
  const int nk = s.nk;
  const int nth = s.nth;
  const int j0 = jsea - 1;
  const int isea = jsea;  // INIT_GET_ISEA under W3_SHRD (w3parall.F90:1484)
  const int ix = g.mapsf(isea - 1, 0);
  const int iy = g.mapsf(isea - 1, 1);
  const bool sea = g.mapsta(iy - 1, ix - 1) > 0;

  // 1. (:1392-1481), this point's share.
  Real et = zero, et02 = zero, ewn = zero, etr = zero, et1 = zero, eet1 = zero;
  Real etx = zero, ety = zero, etxx = zero, etyy = zero, etxy = zero;
  Real abr = zero, aba = zero, abd = zero, ubr = zero, uba = zero, ubd = zero;
  Real sxx = zero, syy = zero, sxy = zero, ussx = zero, ussy = zero, tusx = zero, tusy = zero;
  Real mscx = zero, mscy = zero, mscd = zero, prms = zero, tpms = zero, ettpmm = zero;
  Real etf = zero, qk2 = zero, bhd = zero;
  // The band sums of 2.a; section 3 reads those of IK = NK.
  Real ab = zero, abx = zero, aby = zero, abx2 = zero, aby2 = zero, ab2x = zero, ab2y = zero;
  Real abxx = zero, abyy = zero, abxy = zero, abyx = zero, abst = zero, qk1 = zero;

  // 2. (:1485-1692)
  for (int ik = 1; ik <= nk; ++ik) {
    ab = zero; abx = zero; aby = zero; abx2 = zero; aby2 = zero; ab2x = zero; ab2y = zero;
    abxx = zero; abyy = zero; abxy = zero; abyx = zero; abst = zero; qk1 = zero;
    const Real cgk = g.cg(ik, isea);
    const Real wnk = g.wn(ik, isea);
    const Real sigk = g.sig(ik);

    // 2.b (:1505-1541)
    for (int ith = 1; ith <= nth; ++ith) {
      const Real aj = g.a(ith - 1, ik - 1, jsea);
      ab = ab + aj;
      abx = abx + aj * g.ecos(ith - 1);
      aby = aby + aj * g.esin(ith - 1);
      abx2 = abx2 + aj * g.ec2(ith - 1);
      aby2 = aby2 + aj * g.es2(ith - 1);
      ab2x = ab2x + aj * (two * g.ec2(ith - 1) - one);
      ab2y = ab2y + aj * (two * g.esc(ith - 1));
      abyx = abyx + aj * g.esc(ith - 1);
      if (ith <= nth / 2) {
        const Real ah = g.a(ith + nth / 2 - 1, ik - 1, jsea);
        abst = abst + aj * ah;
        const Real q = aj + ah;
        qk1 = qk1 + q * q;
      }
      const Real factor = gt_sel(cgk / sigk * wnk, half);  // :1530
      abxx = abxx + ((one + g.ec2(ith - 1)) * factor - half) * aj;
      abyy = abyy + ((one + g.es2(ith - 1)) * factor - half) * aj;
      abxy = abxy + g.esc(ith - 1) * factor * aj;
    }

    // 2.c (:1551-1686)
    const Real dsik = g.dsii(ik - 1);
    const Real factor = g.dden(ik - 1) / cgk;
    Real ebd = ab * factor;
    et = et + ebd;
    etf = etf + ebd * cgk;
    ewn = ewn + ebd / wnk;
    etr = etr + ebd / sigk;
    et1 = et1 + ebd * sigk;
    eet1 = eet1 + ebd * ebd * sigk / dsik;
    et02 = et02 + ebd * (sigk * sigk);
    etx = etx + abx * factor;
    ety = ety + aby * factor;
    tusx = tusx + abx * factor * kGrav * wnk / sigk;
    tusy = tusy + aby * factor * kGrav * wnk / sigk;
    const Real wn2 = wnk * wnk;
    etxx = etxx + abx2 * factor * wn2;
    qk2 = qk2 + qk1 * factor * sigk / wnk;
    etyy = etyy + aby2 * factor * wn2;
    etxy = etxy + abyx * factor * wn2;

    const Real jac = kTpi * sigk / cgk;
    const Real factor2 = two * s.dth / kTpi2 * sigk * (jac * jac) * abst;  // :1597-1600
    prms = prms + factor2 * two * dsik;
    if (l.f69 && ik >= s.p2msf2 && ik <= s.p2msf3) g.p2sms(j0, ik - s.p2msf2) = factor2 * two * kTpi;
    if (factor2 > ettpmm) {
      ettpmm = factor2;
      tpms = kTpi / sigk;
    }

    if (ik == nk) {  // :1615-1633
      const Real s2 = sigk * sigk;
      const Real s5 = s2 * sigk * s2;  // SIG**5 as gfortran's power tree: (x*x*x)*(x*x)
      const Real fac5 = s5 / kGrav2 / dsik;
      const Real ma = abx2 * factor * fac5;
      const Real mc = aby2 * factor * fac5;
      const Real mb = abyx * factor * fac5;
      mscd = half * Kokkos::atan2(two * mb, ma - mc);
      const Real cs = Kokkos::cos(mscd);
      const Real sn = Kokkos::sin(mscd);
      mscx = ma * (cs * cs) + two * mb * sn * cs + ma * (sn * sn);
      mscy = mc * (cs * cs) - two * mb * sn * cs + ma * (sn * sn);
    }

    const Real kd = gt_sel(wnk * g.dw(isea), static_cast<Real>(0.001));  // :1639
    Real ussco;
    if (kd < static_cast<Real>(6.)) {
      const Real sh = Kokkos::sinh(kd);
      const Real fkd = factor / (sh * sh);
      abr = abr + ab * fkd;
      aba = aba + abx * fkd;
      abd = abd + aby * fkd;
      const Real sg2 = sigk * sigk;
      ubr = ubr + ab * sg2 * fkd;
      uba = uba + abx * sg2 * fkd;
      ubd = ubd + aby * sg2 * fkd;
      ussco = fkd * sigk * wnk * Kokkos::cosh(two * kd);
      bhd = bhd + kGrav * wnk * ebd / Kokkos::sinh(two * kd);
    } else {
      ussco = factor * sigk * two * wnk;
    }

    abxx = gt_sel(abxx, zero) * factor;  // :1655, ABXX the destination
    abyy = gt_sel(zero, abyy) * factor;  // :1656, 0. the destination
    abxy = abxy * factor;
    sxx = sxx + abxx;
    syy = syy + abyy;
    sxy = sxy + abxy;
    ebd = ebd / dsik;
    g.ebd(ik - 1, j0) = ebd;

    if (l.f3[0] && ik >= s.e3df2[0] && ik <= s.e3df3[0]) g.ef(j0, ik - s.e3df2[0]) = ebd * kTpi;

    ussx = ussx + abx * ussco;
    ussy = ussy + aby * ussco;

    if (l.f3[1] && ik >= s.e3df2[1] && ik <= s.e3df3[1])
      g.th1m(j0, ik - s.e3df2[1]) =
          Kokkos::fmod(static_cast<Real>(630.) - kRade * Kokkos::atan2(aby, abx), static_cast<Real>(360.));
    // MAX(1E-20,AB) (:1678, :1683): the object keeps two copies of this loop, chosen by FLOLOC(3,5),
    // and the maxss destination differs between them: AB when it is set, 1E-20 when it is not.
    const Real tiny = static_cast<Real>(1E-20);
    const Real abm = l.f3[4] ? gt_sel(ab, tiny) : gt_sel(tiny, ab);
    const Real m1 = Kokkos::sqrt(abx * abx + aby * aby) / abm;
    if (l.f3[2] && ik >= s.e3df2[2] && ik <= s.e3df3[2])
      g.sth1m(j0, ik - s.e3df2[2]) = Kokkos::sqrt(Kokkos::fabs(two * (one - m1))) * kRade;
    if (l.f3[3] && ik >= s.e3df2[3] && ik <= s.e3df3[3])
      g.th2m(j0, ik - s.e3df2[3]) =
          Kokkos::fmod(static_cast<Real>(270.) - kRadeHalf * Kokkos::atan2(ab2y, ab2x), static_cast<Real>(180.));
    const Real m2 = Kokkos::sqrt(ab2x * ab2x + ab2y * ab2y) / abm;
    if (l.f3[4] && ik >= s.e3df2[4] && ik <= s.e3df3[4])
      g.sth2m(j0, ik - s.e3df2[4]) = Kokkos::sqrt(Kokkos::fabs(half * (one - m2))) * kRade;
  }

  // Space-time extremes (:1694-1931), on ET, ET02, ET1, ETX, ETY before the tail of 3.b.
  if (l.ste) {
    const Real undef = s.undef;
    Real thmp = undef;
    if (sea && Kokkos::fabs(etx) + Kokkos::fabs(ety) > k1em7) thmp = Kokkos::atan2(ety, etx);
    Real sth;
    Real cth;
    thmp_sincos(thmp, s.nseal >= 4 && jsea <= 4 * (s.nseal / 4), sth, cth);

    Real mssxm = zero, mssym = zero, mssxtm = zero, mssytm = zero, mssxym = zero;
    for (int ik = 1; ik <= nk; ++ik) {
      Real abx2m = zero, aby2m = zero, abxm = zero, abym = zero, abxym = zero;
      for (int ith = 1; ith <= nth; ++ith) {
        const Real aj = g.a(ith - 1, ik - 1, jsea);
        const Real u = g.ecos(ith - 1) * cth + g.esin(ith - 1) * sth;
        const Real v = g.esin(ith - 1) * cth - g.ecos(ith - 1) * sth;
        abx2m = abx2m + aj * (u * u);
        aby2m = aby2m + aj * (v * v);
        abxm = abxm + aj * u;
        abym = abym + aj * v;
        abxym = abxym + aj * u * v;
      }
      const Real wnk = g.wn(ik, isea);
      const Real factor = g.dden(ik - 1) / g.cg(ik, isea);
      const Real wn2 = wnk * wnk;
      mssxm = mssxm + abx2m * factor * wn2;
      mssym = mssym + aby2m * factor * wn2;
      mssxtm = mssxtm + abxm * factor * wnk * g.sig(ik);
      mssytm = mssytm + abym * factor * wnk * g.sig(ik);
      mssxym = mssxym + abxym * factor * wn2;
    }

    Real t02p = undef, wl02x = undef, wl02y = undef, alpxt = undef, alpyt = undef, alpxy = undef;
    Real nv = undef, ns = undef, nb = undef, mu = undef, ni = undef, mode = undef;
    if (et02 > k1em7) t02p = kTpi * Kokkos::sqrt(et / et02);
    if (mssxm > k1em7) wl02x = kTpi * Kokkos::sqrt(et / mssxm);
    if (mssym > k1em7) wl02y = kTpi * Kokkos::sqrt(et / mssym);
    if (mssxm > k1em7 && et02 > k1em7) alpxt = mssxtm / Kokkos::sqrt(mssxm * et02);
    if (mssym > k1em7 && et02 > k1em7) alpyt = mssytm / Kokkos::sqrt(mssym * et02);
    if (mssxm > k1em7 && mssym > k1em7) alpxy = mssxym / Kokkos::sqrt(mssxm * mssym);

    Real stex = zero;
    Real stey = zero;
    if (s.stexu > zero && s.steyu > zero) {
      stex = s.stexu;
      stey = s.steyu;
    }
    const Real sted = gt_sel(s.stedu, zero);  // :1823-1827, which the object turns into maxss

    if (wl02x > k1em7 && wl02y > k1em7 && t02p > k1em7) {  // :1830-1844
      const Real ax2 = alpxt * alpxt;
      const Real ay2 = alpyt * alpyt;
      const Real axy2 = alpxy * alpxy;
      nv = kTpi * (stex * stey * sted) / (wl02x * wl02y * t02p) *
           Kokkos::sqrt(one - ax2 - ay2 - axy2 + two * alpxt * alpyt * alpxy);
      ns = kSqrtTpi * ((stex * sted) / (wl02x * t02p) * Kokkos::sqrt(one - ax2) +
                       (stey * sted) / (wl02y * t02p) * Kokkos::sqrt(one - ay2) +
                       (stex * stey) / (wl02x * wl02y) * Kokkos::sqrt(one - axy2));
      nb = stex / wl02x + stey / wl02y + sted / t02p;
    }
    if (et1 > k1em7) ni = Kokkos::sqrt(et * et02 / (et1 * et1) - one);
    if (et > k1em7) mu = et1 * et1 / kGrav * Kokkos::pow(et, static_cast<Real>(-1.5)) * (one - ni + ni * ni);

    if (stex == zero && stey == zero) {  // :1858-1869
      mode = Kokkos::sqrt(two * Kokkos::log(nb));
    } else if (sted == zero) {
      const Real t = two * Kokkos::log(ns);
      mode = Kokkos::sqrt(t + Kokkos::log(t + Kokkos::log(t)));
    } else if (wl02x > k1em7 && wl02y > k1em7 && t02p > k1em7) {
      const Real t = two * Kokkos::log(nv);
      mode = Kokkos::sqrt(t + two * Kokkos::log(t + two * Kokkos::log(t)));
    }

    const Real sqet = Kokkos::sqrt(et);
    const Real mode2 = mode * mode;
    const Real den = mode - (two * nv * mode + ns) / (nv * mode2 + ns * mode + nb);
    const Real mum = one + mu * mode;
    g.stmaxe(j0) = sqet * (mode + half * mu * mode2 + static_cast<Real>(0.5772) * mum / den);
    g.stmaxd(j0) = sqet * (kPi * mum / kSqrt6 / den);

    Real phist = zero;
    if (t02p > k1em7) {  // :1889-1905
      Real phi[21];
      for (int itl = 0; itl < 21; ++itl) phi[itl] = zero;
      Real tlphi = static_cast<Real>(0.3) * t02p;
      const Real xfr = s.xfr;
      const Real x2 = xfr * xfr;
      const Real x3 = xfr * x2;  // XFR**3 as gfortran's power tree
      for (int itl = 0; itl < 21; ++itl) {
        for (int ik = 1; ik <= nk - 3; ik += 4) {
          const Real sigk = g.sig(ik);
          phi[itl] = phi[itl] + (x3 * g.ebd(ik + 2, j0) * Kokkos::cos(x3 * sigk * tlphi) +
                                 x2 * g.ebd(ik + 1, j0) * Kokkos::cos(x2 * sigk * tlphi) +
                                 xfr * g.ebd(ik, j0) * Kokkos::cos(xfr * sigk * tlphi) +
                                 g.ebd(ik - 1, j0) * Kokkos::cos(sigk * tlphi)) *
                                    g.dsii(ik - 1);
        }
        tlphi = tlphi + t02p / static_cast<Real>(20.);
      }
      for (int itl = 0; itl < 21; ++itl) phi[itl] = phi[itl] / et;
      // MINVAL as the object computes it: start at the first element that is not a NaN, then
      // a < min ? a : min; 21 NaNs give NaN.
      int k = 0;
      while (k < 21 && phi[k] != phi[k]) ++k;
      Real mn = Kokkos::Experimental::quiet_NaN_v<Real>;
      if (k < 21) {
        mn = phi[k];
        for (int m = k + 1; m < 21; ++m) mn = lt_sel(phi[m], mn);
      }
      phist = Kokkos::fabs(mn);
    }

    const Real stmaxel = sqet * (mode + static_cast<Real>(0.5772) / den);
    const Real stmaxdl = sqet * (kPiSqrt6 / den);
    g.hcmaxe(j0) = stmaxel * (one + phist);
    g.hcmaxd(j0) = stmaxdl * (one + phist);
    const Real sq2 = Kokkos::sqrt(two * (one + phist));
    g.hmaxe(j0) = stmaxel * sq2;
    g.hmaxd(j0) = stmaxdl * sq2;
  }

  // 3.a (:1948-1952)
  const Real mssd = half * Kokkos::atan2(two * etxy, etxx - etyy);
  {
    const Real cs = Kokkos::cos(mssd);
    const Real sn = Kokkos::sin(mssd);
    g.mssx(j0) = etxx * (cs * cs) + two * etxy * sn * cs + etyy * (sn * sn);
    g.mssy(j0) = etyy * (cs * cs) - two * etxy * sn * cs + etxx * (sn * sn);
    g.mssd(j0) = mssd;
  }

  // 3.b tail (:1957-1975): AB, ABX, ABY, ABXX, ABYY, ABXY are those of IK = NK.
  const Real signk = g.sig(nk);
  const Real cgnk = g.cg(nk, isea);
  const Real ft1 = static_cast<Real>(0.3333) * (signk * signk) * s.dth * signk;  // :1390
  const Real eband = ab / cgnk;
  et = et + s.fte * eband;
  ewn = ewn + s.ftwl * eband;
  etf = etf + kGrav * s.fttr * eband;
  etr = etr + s.fttr * eband;
  et1 = et1 + ft1 * eband;
  const Real sn2 = signk * signk;
  et02 = et02 + eband * half * (sn2 * sn2) * s.dth;  // SIG(NK)**4 as (x*x)*(x*x)
  etx = etx + s.fte * abx / cgnk;
  ety = ety + s.fte * aby / cgnk;
  sxx = sxx + s.fte * abxx / cgnk;
  syy = syy + s.fte * abyy / cgnk;
  sxy = sxy + s.fte * abxy / cgnk;

  // :1982-1984
  g.sxx(j0) = sxx * kDwat * kGrav;
  g.syy(j0) = syy * kDwat * kGrav;
  g.sxy(j0) = sxy * kDwat * kGrav;

  // :1991-2050
  if (sea) {
    g.hs(j0) = static_cast<Real>(4.) * Kokkos::sqrt(et);
    if (et > k1em7) {
      const Real et2 = et * et;
      g.qp(j0) = (two / et2) * eet1;
      g.wlm(j0) = ewn / et * kTpi;
      g.t0m1(j0) = etr / et * kTpi;
      // Both maxss have the expression as destination (:2008-2009).
      Real ths = kRade * Kokkos::sqrt(
                             gt_sel(two * (one - Kokkos::sqrt(gt_sel((etx * etx + ety * ety) / et2, zero))), zero));
      if (ths < kRade001 * s.dth) ths = zero;
      g.ths(j0) = ths;
      g.qkk(j0) = Kokkos::sqrt(half * qk2) / et;
    } else {
      g.wlm(j0) = zero;
      g.t0m1(j0) = kTpi / signk;
      g.ths(j0) = zero;
    }
    g.thm(j0) = (Kokkos::fabs(etx) + Kokkos::fabs(ety) > k1em7) ? Kokkos::atan2(ety, etx) : zero;
    abr = Kokkos::sqrt(two * gt_sel(zero, abr));  // :2024, 0. the destination
    abd = (abr >= k1em7) ? Kokkos::atan2(abd, aba) : zero;
    aba = abr;
    ubr = Kokkos::sqrt(two * gt_sel(zero, ubr));  // :2031, 0. the destination
    ubd = (ubr >= k1em7) ? Kokkos::atan2(ubd, uba) : zero;
    uba = ubr;
    g.cge(j0) = kDwatGrav * etf;
    if (et02 > k1em7 && et > zero) {
      g.t02(j0) = kTpi * Kokkos::sqrt(et / et02);
      g.t01(j0) = kTpi * et / et1;
    } else {
      g.t02(j0) = kTpi / signk;
      g.t01(j0) = g.t02(j0);
    }
  }
  g.aba(j0) = aba;
  g.abd(j0) = abd;
  g.uba(j0) = uba;
  g.ubd(j0) = ubd;
  g.ussx(j0) = ussx;
  g.ussy(j0) = ussy;
  g.tusx(j0) = tusx;
  g.tusy(j0) = tusy;
  g.mscx(j0) = mscx;
  g.mscy(j0) = mscy;
  g.mscd(j0) = mscd;
  g.prms(j0) = prms;
  g.tpms(j0) = tpms;
  g.bhd(j0) = bhd;

  // 4. (:2070-2212)
  Real ec = g.ebd(nk - 1, j0);
  int ikp0 = nk;
  Real fp0 = s.undef;
  Real thp0 = s.undef;
  for (int ik = nk - 1; ik >= 1; --ik) {
    if (ec < g.ebd(ik - 1, j0)) {
      ec = g.ebd(ik - 1, j0);
      ikp0 = ik;
    }
  }
  if (ikp0 != nk) fp0 = g.sig(ikp0) * kTpiInv;
  const Real xl = one / s.xfr - one;
  const Real xh = s.xfr - one;
  const Real xl2 = xl * xl;
  const Real xh2 = xh * xh;
  if (ikp0 != nk) {
    const Real e0 = g.ebd(ikp0 - 1, j0);
    const Real el = (ikp0 == 1) ? -e0 : g.ebd(ikp0 - 2, j0) - e0;
    const Real eh = g.ebd(ikp0, j0) - e0;
    const Real denom = xl * eh - xh * el;
    fp0 = fp0 * (one + half * (xl2 * eh - xh2 * el) /
                           Kokkos::copysign(gt_sel(Kokkos::fabs(denom), static_cast<Real>(1.E-15)), denom));
  }
  Real etxp = zero;
  Real etyp = zero;
  if (ikp0 != nk) {
    for (int ith = 1; ith <= nth; ++ith) {
      etxp = etxp + g.a(ith - 1, ikp0 - 1, jsea) * g.ecos(ith - 1);
      etyp = etyp + g.a(ith - 1, ikp0 - 1, jsea) * g.esin(ith - 1);
    }
  }
  if (Kokkos::fabs(etxp) + Kokkos::fabs(etyp) > k1em7 && fp0 != s.undef) thp0 = Kokkos::atan2(etyp, etxp);
  if (!sea) {
    fp0 = s.undef;
    thp0 = s.undef;
  }
  g.fp0(j0) = fp0;
  g.thp0(j0) = thp0;
}

/// 6.b (:2270-2338) for JSEA = jsea, after the UNDEF fill of 6.a.
template <class ExecSpace>
KOKKOS_INLINE_FUNCTION void w3outg_part_point(const W3OutgArgs<ExecSpace>& g, const W3OutgScalars& s,
                                              int jsea) {
  using namespace w3outg_detail;
  const int j0 = jsea - 1;
  const int isea = jsea;
  const int ix = g.mapsf(isea - 1, 0);
  const int iy = g.mapsf(isea - 1, 1);
  const bool sea = g.mapsta(iy - 1, ix - 1) > 0;
  const int n1 = g.icprt(j0, 0);  // ICPRT(JSEA,1)
  const int n2 = g.icprt(j0, 1);  // ICPRT(JSEA,2)
  if (sea) {
    g.pnr(j0) = gt_sel(static_cast<Real>(n1 - 1), static_cast<Real>(0.));  // :2277
    if (n1 >= 1) g.pwst(j0) = g.dtprt(5, n2 - 1);
  }
  if (sea && n1 > 1) {
    int i = n2 + 1;
    const auto copy = [&](int j, int ip) {
      const auto d = [&](int k) { return g.dtprt(k - 1, ip - 1); };
      g.phs(j0, j) = d(1);
      g.ptp(j0, j) = d(2);
      g.plp(j0, j) = d(3);
      if (d(4) != s.undef) g.pdir(j0, j) = (static_cast<Real>(270.) - d(4)) * kDera;
      g.psi(j0, j) = d(5);
      g.pws(j0, j) = d(6);
      if (d(7) != s.undef) g.pthp0(j0, j) = (static_cast<Real>(270.) - d(7)) * kDera;
      g.psw(j0, j) = d(8);
      g.ppe(j0, j) = d(9);
      g.pqp(j0, j) = d(10);
      g.pgw(j0, j) = d(11);
      g.ptm1(j0, j) = d(12);
      g.pt1(j0, j) = d(13);
      g.pt2(j0, j) = d(14);
      g.pep(j0, j) = d(15);
    };
    if (g.dtprt(5, i - 1) >= s.wscut) {
      copy(0, i);
      i = i + 1;
    }
    for (int j = 1; j <= s.noswll; ++j) {
      if (i > n2 + n1 - 1) break;
      copy(j, i);
      i = i + 1;
    }
  }
}

/// W3OUTG(A, FLPART, FLOUTG, FLOUTG2), :1201-2380, as switch_lab_shrd compiles it.
template <class ExecSpace>
void w3outg(const ExecSpace& exec, const W3OutgScalars& s, const W3OutgFlags& f, const W3OutgArgs<ExecSpace>& g,
            const W3OutgHooks& hooks) {
  using namespace w3outg_detail;
  // :1381-1386
  const auto floloc = [&](int i, int j) {
    const int k = (i - 1) + (j - 1) * kNogrp;
    return (f.floutg && f.flogrd[k] != 0) || (f.floutg2 && f.flogr2[k] != 0);
  };
  W3OutgLoc l{};
  for (int k = 0; k < 5; ++k) l.f3[k] = floloc(3, k + 1);
  l.f69 = floloc(6, 9);
  l.ste = (s.stexu > static_cast<Real>(0.) && s.steyu > static_cast<Real>(0.)) || s.stedu > static_cast<Real>(0.);

  // 1. The whole-array assignments of :1394-1481, over NSEALM. CGE is not reset (the Fortran
  // writes it at sea points only).
  const Real undef = s.undef;
  const int nsealm = static_cast<int>(g.hs.extent(0));
  Kokkos::parallel_for(
      "ww::iogo::w3outg_init", Kokkos::RangePolicy<ExecSpace>(exec, 0, nsealm), KOKKOS_LAMBDA(const int j) {
        const Real z = static_cast<Real>(0.);
        g.aba(j) = z; g.abd(j) = z; g.uba(j) = z; g.ubd(j) = z;
        g.sxx(j) = z; g.syy(j) = z; g.sxy(j) = z;
        g.ussx(j) = z; g.ussy(j) = z; g.tusx(j) = z; g.tusy(j) = z;
        g.mssx(j) = z; g.mssy(j) = z; g.mssd(j) = z; g.mscx(j) = z; g.mscy(j) = z; g.mscd(j) = z;
        g.prms(j) = z; g.tpms(j) = z; g.bhd(j) = z;
        g.hs(j) = undef; g.wlm(j) = undef; g.t0m1(j) = undef; g.t01(j) = undef; g.t02(j) = undef;
        g.fp0(j) = undef; g.thm(j) = undef; g.ths(j) = undef; g.thp0(j) = undef; g.hsig(j) = undef;
        g.qkk(j) = undef; g.stmaxe(j) = undef; g.stmaxd(j) = undef; g.hcmaxe(j) = undef;
        g.hmaxe(j) = undef; g.hcmaxd(j) = undef; g.hmaxd(j) = undef; g.qp(j) = undef; g.wbt(j) = undef;
      });

  // 2.-4.
  Kokkos::parallel_for(
      "ww::iogo::w3outg", Kokkos::RangePolicy<ExecSpace>(exec, 1, s.nseal + 1),
      KOKKOS_LAMBDA(const int jsea) { w3outg_point<ExecSpace>(g, s, l, jsea); });

  // 6. (:2242-2340)
  if (f.flpart) {
    const int ncol = s.noswll + 1;
    Kokkos::parallel_for(
        "ww::iogo::w3outg_part_init", Kokkos::RangePolicy<ExecSpace>(exec, 0, nsealm), KOKKOS_LAMBDA(const int j) {
          for (int c = 0; c < ncol; ++c) {
            g.phs(j, c) = undef; g.ptp(j, c) = undef; g.plp(j, c) = undef; g.pdir(j, c) = undef;
            g.psi(j, c) = undef; g.pws(j, c) = undef; g.pthp0(j, c) = undef; g.pqp(j, c) = undef;
            g.ppe(j, c) = undef; g.pgw(j, c) = undef; g.psw(j, c) = undef; g.ptm1(j, c) = undef;
            g.pt1(j, c) = undef; g.pt2(j, c) = undef; g.pep(j, c) = undef;
          }
          g.pwst(j) = undef;
          g.pnr(j) = undef;
        });
    Kokkos::parallel_for(
        "ww::iogo::w3outg_part", Kokkos::RangePolicy<ExecSpace>(exec, 1, s.nseal + 1),
        KOKKOS_LAMBDA(const int jsea) { w3outg_part_point<ExecSpace>(g, s, jsea); });
  }

  // :2347-2362
  const bool u3s1 = floloc(6, 8);
  const bool u3s2 = floloc(6, 12);
  const bool skw = floloc(8, 7) || floloc(8, 8) || floloc(8, 9);
  const bool wbt = floloc(2, 17);
  if ((u3s1 || u3s2) && hooks.calc_u3stokes == nullptr)
    Kokkos::abort("ww::iogo::w3outg: CALC_U3STOKES requested (FLOLOC(6,8) or (6,12)) and not translated");
  if (wbt && hooks.calc_wbt == nullptr)
    Kokkos::abort("ww::iogo::w3outg: CALC_WBT requested (FLOLOC(2,17)) and not translated");
  if (u3s1 || u3s2) exec.fence("ww::iogo::w3outg before CALC_U3STOKES");
  if (u3s1) hooks.calc_u3stokes(hooks.ctx, 1);
  if (u3s2) hooks.calc_u3stokes(hooks.ctx, 2);
  if (skw) {
    const int nk = s.nk;
    const int nth = s.nth;
    const Real xfr = s.xfr;
    const Real dth = s.dth;
    Kokkos::parallel_for(
        "ww::iogo::w3outg_skewness", Kokkos::RangePolicy<ExecSpace>(exec, 1, s.nseal + 1),
        KOKKOS_LAMBDA(const int jsea) {
          skewness_point(&g.a(0, 0, jsea), &g.cg(0, jsea), nk, nth, &g.sig(0), xfr, dth, &g.ecos(0), &g.esin(0),
                         g.fac0.data(), g.fac1.data(), g.fac2.data(), g.fac3.data(), g.skew(jsea - 1),
                         g.embia1(jsea - 1), g.embia2(jsea - 1), &g.skw_f2(0, jsea - 1), &g.skw_sighf(0, jsea - 1),
                         &g.skw_dfimhf(0, jsea - 1), &g.skw_fak(0, jsea - 1));
        });
  }
  if (wbt) {
    exec.fence("ww::iogo::w3outg before CALC_WBT");
    hooks.calc_wbt(hooks.ctx);
  }
}

}  // namespace ww::iogo
