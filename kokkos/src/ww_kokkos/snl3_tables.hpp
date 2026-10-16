// kokkos/src/ww_kokkos/snl3_tables.hpp
// INSNL3 with its internal MINLAM and MAXLAM: the quadruplet tables of the Generalized
// Multiple DIA (NL3), built on the host. Phase 1, translated, not regtested. WW3 heritage:
// WW3-noaa/model/src/w3snl3md.F90 at NOAA-EMC/WW3 03adda91 (INSNL3 :788-1649, MINLAM
// :1525-1578, MAXLAM :1593-1645); author H. L. Tolman. Evidence: kokkos/port_report/INSNL3_KOKKOS.md.
// W3SRCE's W3SNL3 reads struct Snl3Tables below (its fields are W3GDATMD's SNLPS and W3SNL3MD's
// SITMIN, XSIT). W3IOGR calls INSNL3 on every mod_def read (w3iogrmd.F90:1809), one table per
// grid (MPARS(IGRID)); the shim keeps one Snl3Tables per grid. W3_S, W3_T, W3_T1, W3_T2, W3_T3
// are not compiled in switch_NCEP_glwu and are not ported.
// Trap: INSNL3's NKD is W3GDATMD's NKD = 1300 when W3_ST4 is on (w3gdatmd.F90:637, use
// association hides the module's own NKD = 275, w3snl3md.F90:130, :872), and 275 otherwise;
// W3SNL3 always clamps JKD to 275 (:332, :425). Snl3Input::st4 selects it.
// ⚠ Host only. LOG of the four AUXFR goes through glibc's libmvec where the object does.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cmath>
#include <vector>

#include <Kokkos_Core.hpp>

#include "disp_distab.hpp"
#include "real.hpp"
#include "st4_params.hpp"

#if defined(__x86_64__) && defined(__SSE2__) && defined(__GLIBC__)
#include <xmmintrin.h>
extern "C" __m128 _ZGVbN4v_logf(__m128);
#define WW_SNL3_HAVE_LIBMVEC 1
#endif

namespace ww::snl3 {

using DeviceSpace = Kokkos::DefaultExecutionSpace::memory_space;
using RealView1D = Kokkos::View<Real*, DeviceSpace>;
using RealView3D = Kokkos::View<Real***, Kokkos::LayoutLeft, DeviceSpace>;
using IntView3D = Kokkos::View<int***, Kokkos::LayoutLeft, DeviceSpace>;

inline constexpr int kNkdSnl3 = 275;   // W3SNL3MD NKD :130, W3SNL3's clamp of JKD (:425)
inline constexpr int kNkdSt4 = 1300;   // W3GDATMD NKD :637 (W3_ST4), INSNL3's NKD then
inline constexpr Real kLamMax = static_cast<Real>(0.49999);  // LAMMAX :132
inline constexpr Real kDelthm = static_cast<Real>(90.);      // DELTHM :133

/// INSNL3's error exits, the EXTCDE codes (:926, :980, :989, :1445, :1449). 0 is success.
enum Insnl3Status : int { kInsnl3Ok = 0, kInsnl3Lam = 1000, kInsnl3Off = 1001, kInsnl3Quad = 1002,
                          kInsnl3Nthmax = 1010, kInsnl3Nqa = 1011 };

/// What W3SNL3 reads, as INSNL3 leaves it. Fortran indices map to 0-based ones by -1 on every
/// subscript: QST1(i,iqa,ikd) is qst1(i-1, iqa-1, ikd-1); XSI(ifr) is xsi(ifr-1).
/// Integer table values (QST1, QST4) are offsets in W3SNL3's expanded 1-D spectral space, kept
/// as the Fortran computes them: UE(ISPX+QST1) with ISPX = (IFR-1)*NTHEXP + ITH, the arrays
/// declared UE(NSPMIN:NSPMAX), DSB..DD4(NSPMIN:NSPMX2) (w3snl3md.F90:368-370).
struct Snl3Tables {
  int nfr = 0;     ///< NK
  int nth = 0;     ///< NTH
  int nkd = 0;     ///< extent of the third table index (1300 or 275, see the header)
  int nfrmin = 0;  ///< NFRMIN = 1 + IFRMIN (:1150), <= 1
  int nfrmax = 0;  ///< NFRMAX = NFR + IFRMAX - IFRMIN (:1151)
  int nfrcut = 0;  ///< NFRCUT = NFR - IFRMIN (:1152)
  int nthmax = 0;  ///< NTHMAX (:1148)
  int nthexp = 0;  ///< NTHEXP = NTH + 2*NTHMAX (:1153)
  int nspmin = 0;  ///< NSPMIN = 1 + (NFRMIN-1)*NTHEXP - NTHMAX (:1155)
  int nspmax = 0;  ///< NSPMAX = NFRMAX*NTHEXP - NTHMAX (:1156)
  int nspmx2 = 0;  ///< NSPMX2 = NFRCUT*NTHEXP - NTHMAX (:1157)
  int nqa = 0;     ///< NQA, quadruplet realizations (:1431)
  Real sitmin = 0;  ///< W3SNL3MD SITMIN (:934), module SAVE, folded constant
  Real xsit = 0;    ///< W3SNL3MD XSIT (:936), module SAVE, folded constant
  RealView1D xsi;   ///< XSI(1:NFRMAX), rad/s (:1169-1172)
  RealView1D frq;   ///< FRQ(1:NFRMAX), Hz (:1173); W3SNL3 does not read it
  IntView3D qst1;   ///< QST1(16,NQA,NKD) (:1191-1198)
  RealView3D qst2;  ///< QST2(16,NQA,NKD) (:1200-1207)
  RealView3D qst3;  ///< QST3(6,NQA,NKD) (:1211-1222)
  IntView3D qst4;   ///< QST4(16,NQA,NKD) (:1441)
  RealView3D qst5;  ///< QST5(16,NQA,NKD) (:1397, :1402)
  RealView3D qst6;  ///< QST6(16,NQA,NKD) (:1398, :1403)
};

/// INSNL3's inputs from W3GDATMD, host pointers. sig is SIG(1:NK), the others are (1:SNLNQ).
struct Snl3Input {
  int nk, nth;
  Real xfr, dth;
  const Real* sig;
  int snlnq;
  const Real *snll, *snlm, *snlt, *snlcd, *snlcs;
  bool st4;  ///< W3_ST4 in the switch: INSNL3's NKD is 1300 instead of 275
};

namespace insnl3_detail {
// Every MIN/MAX of INSNL3 has the value in minss/maxss's destination and the constant as
// source (w3snl3md.o, :1018-1037, :1327-1346, :1565-1570, :1633-1637): NaN gives the constant.
inline Real minss(Real d, Real s) { return (d < s) ? d : s; }
inline Real maxss(Real d, Real s) { return (d > s) ? d : s; }
// Two's complement negation and ABS as the object's negl/pabsd (INT_MIN stays INT_MIN).
inline int ineg(int x) { return static_cast<int>(0u - static_cast<unsigned>(x)); }
inline int iabs(int x) { return x < 0 ? ineg(x) : x; }
// REAL**INTEGER with a run-time exponent: libgcc's __powisf2, as the object calls it.
inline Real powi(Real x, int n) { return __builtin_powif(x, n); }

inline constexpr Real kDera = 0x1.1df46ap-6f;      // DERA = PI/180, constants.F90:78
inline constexpr Real kAtan1 = 0x1.921fb6p-1f;     // ATAN(1.) folded, :1566, :1634
inline constexpr Real kSqrtGrav = 0x1.90d376p+1f;  // SQRT(GRAV/DEPTH), DEPTH = 1, folded :951, :1240
inline constexpr Real kSitmin = 0x1.998eaep-6f;    // SQRT(KDMIN*TANH(KDMIN)) folded :934
inline constexpr Real kXsit1300 = 0x1.010634p+0f;  // (SITMAX/SITMIN)**(1./1299.) folded :936
inline constexpr Real kXsit275 = 0x1.04e46cp+0f;   // the same with NKD = 275
static_assert(kDera == ww::kPi / static_cast<Real>(180.));
static_assert(kSitmin == 2.499739639461040496826171875e-2f);
static_assert(kXsit1300 == 1.0040009021759033203125f);
static_assert(kXsit275 == 1.0191104412078857421875f);

/// MINLAM (:1525-1578); mu, theta in degrees.
inline Real minlam(Real mu, Real theta) {
  if (theta < static_cast<Real>(0.)) return static_cast<Real>(0.);
  const Real muloc = maxss(minss(mu, static_cast<Real>(1.)), static_cast<Real>(0.));
  const Real thetar = theta * kAtan1 / static_cast<Real>(45.);
  const Real a2 = (muloc + static_cast<Real>(1.)) * (muloc + static_cast<Real>(1.));
  const Real b2 = (static_cast<Real>(1.) - muloc) * (static_cast<Real>(1.) - muloc);
  Real bb = a2 * a2 + b2 * b2 + static_cast<Real>(2.) * a2 * b2 * ::cosf(thetar);
  bb = ::sqrtf(maxss(bb, static_cast<Real>(0.)));
  const Real aux = maxss(static_cast<Real>(0.5) * bb - static_cast<Real>(1.), static_cast<Real>(0.));
  return ::sqrtf(aux);
}

/// MAXLAM (:1593-1645); mu, theta in degrees. Evidence: kokkos/port_report/MAXLAM_KOKKOS.md.
inline Real maxlam(Real mu, Real theta) {
  if (theta < static_cast<Real>(0.)) return static_cast<Real>(0.5);
  const Real muloc = maxss(minss(mu, static_cast<Real>(1.)), static_cast<Real>(0.));
  const Real thetar = theta * kAtan1 / static_cast<Real>(45.);
  const Real a2 = (muloc + static_cast<Real>(1.)) * (muloc + static_cast<Real>(1.));
  const Real b2 = (static_cast<Real>(1.) - muloc) * (static_cast<Real>(1.) - muloc);
  Real bb = a2 * a2 + b2 * b2 + static_cast<Real>(2.) * a2 * b2 * ::cosf(thetar);
  bb = ::sqrtf(maxss(bb, static_cast<Real>(0.)));
  return static_cast<Real>(0.25) * bb;
}

/// The frequency-index loop of :1045-1050 and :1272-1277. gfortran -O3 vectorises it with
/// SLP: JFR and JFR1 come from four lanes of _ZGVbN4v_logf, while the exponents of the
/// __powisf2 calls in WFR come from scalar logf of the same AUXFR (w3snl3md.optimized,
/// :1046-1049, :1273-1276). libmvec and scalar logf can differ in the last bits, so both
/// are kept.
inline void freq_indices(const Real auxfr[4], Real xfrln, Real xfr, int jfr[4], int jfr1[4],
                         Real wfr[4], Real wfr1[4]) {
  const Real one = static_cast<Real>(1.);
  Real lv[4];
#if defined(WW_SNL3_HAVE_LIBMVEC)
  alignas(16) float a[4], r[4];
  for (int j = 0; j < 4; ++j) a[j] = auxfr[j];
  _mm_store_ps(r, _ZGVbN4v_logf(_mm_load_ps(a)));
  for (int j = 0; j < 4; ++j) lv[j] = r[j];
#else
  for (int j = 0; j < 4; ++j) lv[j] = ::logf(auxfr[j]);  // ⚠ not the object's lanes
#endif
  for (int j = 0; j < 4; ++j) {
    jfr[j] = ww::st4::fint(lv[j] / xfrln);
    jfr1[j] = ww::st4::fint(static_cast<Real>(jfr[j]) + ::copysignf(one, auxfr[j] - one));
  }
  for (int j = 0; j < 4; ++j) {
    const int js = ww::st4::fint(::logf(auxfr[j]) / xfrln);
    const int j1s = ww::st4::fint(static_cast<Real>(js) + ::copysignf(one, auxfr[j] - one));
    const Real p1 = powi(xfr, j1s);
    wfr[j] = (p1 - auxfr[j]) / (p1 - powi(xfr, js));
    wfr1[j] = one - wfr[j];
  }
}

/// DELTH(1:4) of :1014-1037 and :1323-1346, BB included.
inline void offset_angles(Real th12, Real off12, Real wn0, Real wn1, Real wn2, Real wn3, Real wn4,
                          Real delth[4]) {
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.);
  Real bb;
  if (th12 < zero) {
    bb = static_cast<Real>(2.) * wn0;
  } else {
    bb = wn1 * wn1 + wn2 * wn2 + static_cast<Real>(2.) * wn1 * wn2 * ::cosf(th12);
    bb = ::sqrtf(maxss(bb, zero));
  }
  if (th12 < zero && ::fabsf(off12) <= static_cast<Real>(1.E-4)) {
    delth[0] = zero;
    delth[1] = zero;
  } else {
    const Real cc = wn1, aa = wn2;
    const Real aux1 = (cc * cc + bb * bb - aa * aa) / (static_cast<Real>(2.) * bb * cc);
    const Real aux2 = (aa * aa + bb * bb - cc * cc) / (static_cast<Real>(2.) * bb * aa);
    delth[0] = -::acosf(maxss(minss(aux1, one), zero));
    delth[1] = ::acosf(maxss(minss(aux2, one), zero));
  }
  const Real cc = wn3, aa = wn4;
  const Real aux1 = (cc * cc + bb * bb - aa * aa) / (static_cast<Real>(2.) * bb * cc);
  const Real aux2 = (aa * aa + bb * bb - cc * cc) / (static_cast<Real>(2.) * bb * aa);
  delth[2] = -::acosf(maxss(minss(aux1, one), zero));
  delth[3] = ::acosf(maxss(minss(aux2, one), zero));
}

/// One TSTORE entry (:905-909).
struct Qst {
  int ofr[4], ofr1[4], oth[4], oth1[4];
  Real hfr[4], hfr1[4], hth[4], hth1[4];
  Real f1, f2, f3, f4, cqd, cqs;
};

/// The JTH/JTH1 sign flips of a realization (:1090-1105, :1375-1392). The DEFAULT case
/// (J = 1) flips everything in section 3 and nothing in section 2.
inline void flip(int jq, bool sec3, int jth[4], int jth1[4]) {
  const bool all = (jq == 3) || (sec3 && jq == 1);
  const bool pair = (jq == 2) || (jq == 4);
  for (int j = 0; j < 4; ++j) {
    if (all || (pair && j >= 2)) {
      jth[j] = ineg(jth[j]);
      jth1[j] = ineg(jth1[j]);
    }
  }
}
}  // namespace insnl3_detail

/// INSNL3 (:788-1649), host. Returns an Insnl3Status; on 0, out holds the tables on the device.
/// On an error the Fortran has written the message and aborted; out is then unspecified.
inline int insnl3(const Snl3Input& in, Snl3Tables& out) {
  using namespace insnl3_detail;
  using ww::st4::fint;
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.);
  const Real eps = static_cast<Real>(1.E-6);  // WAVNU2's EPS and NMAX, :956 and the others
  const int nmax = 25;
  const int nfr = in.nk;
  const int nth = in.nth;
  const int nkd = in.st4 ? kNkdSt4 : kNkdSnl3;
  const Real xsit = in.st4 ? kXsit1300 : kXsit275;
  const Real depth = one;  // :933
  int ierr = 0;

  // 1.a :922-927
  const Real xfrln = ::logf(in.xfr);
  if (kLamMax <= zero || kLamMax > static_cast<Real>(0.5) || kDelthm < zero) return kInsnl3Lam;

  // 2. :945-1137
  std::vector<Qst> tstore(static_cast<size_t>(in.snlnq) * 4 * static_cast<size_t>(nkd));
  auto ts = [&](int iqa, int ikd) -> Qst& {  // TSTORE(IQA,IKD), 1-based
    return tstore[static_cast<size_t>(iqa - 1) +
                  static_cast<size_t>(in.snlnq) * 4 * static_cast<size_t>(ikd - 1)];
  };
  int ifrmin = 0, ifrmax = 0, nthmax = 0, nqa = 0, nqd = 0, nqs = 0;
  int jfr[4], jfr1[4], jth[4] = {0, 0, 0, 0}, jth1[4] = {0, 0, 0, 0};
  Real wfr[4], wfr1[4], wth[4], wth1[4], auxfr[4], delth[4];
  Real s0 = kSitmin * kSqrtGrav / xsit;  // :951
  for (int ikd = 1; ikd <= nkd; ++ikd) {
    s0 = s0 * xsit;
    Real wn0, cg0;
    ww::disp::wavnu2(s0, depth, wn0, cg0, eps, nmax, ierr);
    nqa = 0;
    nqd = 0;
    nqs = 0;
    for (int iq = 1; iq <= in.snlnq; ++iq) {
      const Real off12 = in.snlm[iq - 1];
      const Real off34 = in.snll[iq - 1];
      const Real snlt = in.snlt[iq - 1];
      const Real th12 = snlt * kDera;
      if (in.snlcd[iq - 1] > zero) nqd = nqd + 1;
      if (in.snlcs[iq - 1] > zero) nqs = nqs + 1;
      if (th12 < zero) {  // :976-991
        if (off12 < zero || off12 > static_cast<Real>(0.5) || off34 < zero ||
            off34 > static_cast<Real>(0.5))
          return kInsnl3Off;
      } else {
        if (snlt > kDelthm || off12 < zero || off12 >= one || off34 < minlam(off12, snlt) ||
            off34 > maxlam(off12, snlt))
          return kInsnl3Quad;
      }
      // 2.c :1000-1037
      Real wn1, cg1, wn2, cg2, wn3, cg3, wn4, cg4;
      const Real s1 = s0 * (one + off12);
      ww::disp::wavnu2(s1, depth, wn1, cg1, eps, nmax, ierr);
      const Real s2 = s0 * (one - off12);
      ww::disp::wavnu2(s2, depth, wn2, cg2, eps, nmax, ierr);
      const Real s3 = s0 * (one + off34);
      ww::disp::wavnu2(s3, depth, wn3, cg3, eps, nmax, ierr);
      const Real s4 = s0 * (one - off34);
      ww::disp::wavnu2(s4, depth, wn4, cg4, eps, nmax, ierr);
      auxfr[0] = s1 / s0;
      auxfr[1] = s2 / s0;
      auxfr[2] = s3 / s0;
      auxfr[3] = s4 / s0;
      offset_angles(th12, off12, wn0, wn1, wn2, wn3, wn4, delth);
      // 2.d :1045-1053
      freq_indices(auxfr, xfrln, in.xfr, jfr, jfr1, wfr, wfr1);
      ifrmin = ww::st4::min_i(ifrmin, ww::st4::min_i(ww::st4::min_i(jfr1[0], jfr1[1]),
                                                     ww::st4::min_i(jfr1[2], jfr1[3])));
      ifrmax = ww::st4::max_i(ifrmax, ww::st4::max_i(ww::st4::max_i(jfr1[0], jfr1[1]),
                                                     ww::st4::max_i(jfr1[2], jfr1[3])));
      // 2.e :1064-1072. SIGN(1.,DELTH(2)) and (4) are folded to 1 by the object: ACOS >= +0.
      for (int j = 0; j < 4; ++j) {
        const Real aux1 = delth[j] / in.dth;
        jth[j] = fint(aux1);
        const Real sgn = (j == 1 || j == 3) ? one : ::copysignf(one, delth[j]);
        jth1[j] = fint(static_cast<Real>(jth[j]) + sgn);
        wth1[j] = ::fabsf(aux1) - static_cast<Real>(iabs(jth[j]));
        wth[j] = one - wth1[j];
      }
      for (int j = 0; j < 4; ++j) nthmax = ww::st4::max_i(nthmax, iabs(jth1[j]));
      // 2.f :1083-1129
      const int jj = (off12 == zero && snlt < zero) ? 2 : 4;
      const Real auxf = (wn0 * s0) / cg0;
      for (int j = 1; j <= jj; ++j) {
        flip(j, false, jth, jth1);
        nqa = nqa + 1;
        Qst& q = ts(nqa, ikd);
        for (int k = 0; k < 4; ++k) {
          q.ofr[k] = jfr[k];
          q.ofr1[k] = jfr1[k];
          q.hfr[k] = wfr[k];
          q.hfr1[k] = wfr1[k];
          q.oth[k] = jth[k];
          q.oth1[k] = jth1[k];
          q.hth[k] = wth[k];
          q.hth1[k] = wth1[k];
        }
        if (jj == 2) {
          q.cqd = in.snlcd[iq - 1] * static_cast<Real>(2.);
          q.cqs = in.snlcs[iq - 1] * static_cast<Real>(2.);
        } else {
          q.cqd = in.snlcd[iq - 1];
          q.cqs = in.snlcs[iq - 1];
        }
        q.f1 = auxf * cg1 / (wn1 * s1);
        q.f2 = auxf * cg2 / (wn2 * s2);
        q.f3 = auxf * cg3 / (wn3 * s3);
        q.f4 = auxf * cg4 / (wn4 * s4);
      }
    }
  }

  // 2.g :1148-1173
  nthmax = nthmax + 1;
  const int nfrmin = 1 + ifrmin;
  const int nfrmax = nfr + ifrmax - ifrmin;
  const int nfrcut = nfr - ifrmin;
  const int nthexp = nth + 2 * nthmax;
  out.nfr = nfr;
  out.nth = nth;
  out.nkd = nkd;
  out.nfrmin = nfrmin;
  out.nfrmax = nfrmax;
  out.nfrcut = nfrcut;
  out.nthmax = nthmax;
  out.nthexp = nthexp;
  out.nspmin = 1 + (nfrmin - 1) * nthexp - nthmax;
  out.nspmax = nfrmax * nthexp - nthmax;
  out.nspmx2 = nfrcut * nthexp - nthmax;
  out.sitmin = kSitmin;
  out.xsit = xsit;
  out.xsi = RealView1D("snl3_xsi", static_cast<size_t>(nfrmax));
  out.frq = RealView1D("snl3_frq", static_cast<size_t>(nfrmax));
  auto hxsi = Kokkos::create_mirror_view(out.xsi);
  auto hfrq = Kokkos::create_mirror_view(out.frq);
  for (int ifr = 1; ifr <= nfr; ++ifr) hxsi(ifr - 1) = in.sig[ifr - 1];
  for (int ifr = nfr + 1; ifr <= nfrmax; ++ifr) hxsi(ifr - 1) = hxsi(ifr - 2) * in.xfr;
  for (int ifr = 1; ifr <= nfrmax; ++ifr) hfrq(ifr - 1) = hxsi(ifr - 1) * ww::st4::kTpiInv;

  // 2.h :1177-1222. NQA here is the count of the last IKD (the same for every IKD).
  const size_t nq = static_cast<size_t>(nqa), nd = static_cast<size_t>(nkd);
  out.nqa = nqa;
  out.qst1 = IntView3D("snl3_qst1", 16, nq, nd);
  out.qst2 = RealView3D("snl3_qst2", 16, nq, nd);
  out.qst3 = RealView3D("snl3_qst3", 6, nq, nd);
  auto h1 = Kokkos::create_mirror_view(out.qst1);
  auto h2 = Kokkos::create_mirror_view(out.qst2);
  auto h3 = Kokkos::create_mirror_view(out.qst3);
  for (int ikd = 1; ikd <= nkd; ++ikd) {
    for (int iqa = 1; iqa <= nqa; ++iqa) {
      const Qst& q = ts(iqa, ikd);
      for (int j = 1; j <= 4; ++j) {
        const int b = (j - 1) * 4;
        h1(b + 0, iqa - 1, ikd - 1) = q.oth[j - 1] + q.ofr[j - 1] * nthexp;
        h1(b + 1, iqa - 1, ikd - 1) = q.oth1[j - 1] + q.ofr[j - 1] * nthexp;
        h1(b + 2, iqa - 1, ikd - 1) = q.oth[j - 1] + q.ofr1[j - 1] * nthexp;
        h1(b + 3, iqa - 1, ikd - 1) = q.oth1[j - 1] + q.ofr1[j - 1] * nthexp;
        h2(b + 0, iqa - 1, ikd - 1) = q.hfr[j - 1] * q.hth[j - 1];
        h2(b + 1, iqa - 1, ikd - 1) = q.hfr[j - 1] * q.hth1[j - 1];
        h2(b + 2, iqa - 1, ikd - 1) = q.hfr1[j - 1] * q.hth[j - 1];
        h2(b + 3, iqa - 1, ikd - 1) = q.hfr1[j - 1] * q.hth1[j - 1];
      }
      h3(0, iqa - 1, ikd - 1) = q.f1;
      h3(1, iqa - 1, ikd - 1) = q.f2;
      h3(2, iqa - 1, ikd - 1) = q.f3;
      h3(3, iqa - 1, ikd - 1) = q.f4;
      h3(4, iqa - 1, ikd - 1) = q.cqd;
      h3(5, iqa - 1, ikd - 1) = q.cqs;
    }
  }
  if (nqd > 0) {
    for (int ikd = 0; ikd < nkd; ++ikd)
      for (int iqa = 0; iqa < nqa; ++iqa) h3(4, iqa, ikd) = h3(4, iqa, ikd) / static_cast<Real>(nqd);
  }
  if (nqs > 0) {
    for (int ikd = 0; ikd < nkd; ++ikd)
      for (int iqa = 0; iqa < nqa; ++iqa) h3(5, iqa, ikd) = h3(5, iqa, ikd) / static_cast<Real>(nqs);
  }
  tstore.clear();

  // 3. :1229-1437
  int nthmx2 = 0;
  out.qst4 = IntView3D("snl3_qst4", 16, nq, nd);
  out.qst5 = RealView3D("snl3_qst5", 16, nq, nd);
  out.qst6 = RealView3D("snl3_qst6", 16, nq, nd);
  auto h4 = Kokkos::create_mirror_view(out.qst4);
  auto h5 = Kokkos::create_mirror_view(out.qst5);
  auto h6 = Kokkos::create_mirror_view(out.qst6);
  // AST1/AST2 (:1236); entries no realization writes stay 0, a defined value where the
  // Fortran's ALLOCATE leaves garbage (they are all written when SNLNQ >= 1).
  std::vector<int> ast1(16 * nq * nd, 0), ast2(16 * nq * nd, 0);
  auto at = [&](int ist, int iqa, int ikd) {  // 1-based (IST,IQA,IKD)
    return static_cast<size_t>(ist - 1) + 16 * (static_cast<size_t>(iqa - 1) + nq * static_cast<size_t>(ikd - 1));
  };
  s0 = kSitmin * kSqrtGrav / xsit;  // :1240, from the module SITMIN and XSIT
  for (int ikd = 1; ikd <= nkd; ++ikd) {
    s0 = s0 * xsit;
    Real wn0, cg0;
    ww::disp::wavnu2(s0, depth, wn0, cg0, eps, nmax, ierr);
    nqa = 0;
    for (int iq = 1; iq <= in.snlnq; ++iq) {
      const Real off12 = in.snlm[iq - 1];
      const Real off34 = in.snll[iq - 1];
      const Real snlt = in.snlt[iq - 1];
      const Real th12 = snlt * kDera;
      auxfr[0] = one + off12;  // :1267-1270
      auxfr[1] = one - off12;
      auxfr[2] = one + off34;
      auxfr[3] = one - off34;
      freq_indices(auxfr, xfrln, in.xfr, jfr, jfr1, wfr, wfr1);
      const int jj = (off12 == zero && snlt < zero) ? 2 : 4;  // :1367-1371
      for (int jiq = 1; jiq <= 4; ++jiq) {
        const Real wf = (jiq <= 2) ? -one : one;
        for (int jof = 1; jof <= 2; ++jof) {
          const int ifr = (jof == 1) ? ineg(jfr[jiq - 1]) : ineg(jfr1[jiq - 1]);
          const Real wfroff = (jof == 1) ? wfr[jiq - 1] : wfr1[jiq - 1];
          const Real sioff = s0 * powi(in.xfr, ifr);  // :1308
          Real wn1, cg1, wn2, cg2, wn3, cg3, wn4, cg4;
          ww::disp::wavnu2(sioff, depth, wn0, cg0, eps, nmax, ierr);
          ww::disp::wavnu2(sioff * (one + off12), depth, wn1, cg1, eps, nmax, ierr);
          ww::disp::wavnu2(sioff * (one - off12), depth, wn2, cg2, eps, nmax, ierr);
          ww::disp::wavnu2(sioff * (one + off34), depth, wn3, cg3, eps, nmax, ierr);
          ww::disp::wavnu2(sioff * (one - off34), depth, wn4, cg4, eps, nmax, ierr);
          offset_angles(th12, off12, wn0, wn1, wn2, wn3, wn4, delth);
          // :1352-1358
          const int k = jiq - 1;
          const Real aux1 = delth[k] / in.dth;
          jth[k] = fint(aux1);
          jth1[k] = fint(static_cast<Real>(jth[k]) + ::copysignf(one, delth[k]));
          wth1[k] = ::fabsf(aux1) - static_cast<Real>(iabs(jth[k]));
          wth[k] = one - wth1[k];
          nthmx2 = ww::st4::max_i(nthmx2, iabs(jth1[k]));
          // 3.f :1373-1407
          for (int jqr = 1; jqr <= jj; ++jqr) {
            flip(jqr, true, jth, jth1);
            int ist = (jiq - 1) * 4 + (jof - 1) * 2 + 1;
            ast1[at(ist, nqa + jqr, ikd)] = ifr;
            ast2[at(ist, nqa + jqr, ikd)] = jth[k];
            const Real w0 = wfroff * wth[k];
            h5(ist - 1, nqa + jqr - 1, ikd - 1) = wf * w0;
            h6(ist - 1, nqa + jqr - 1, ikd - 1) = wf * (w0 * w0);
            ist = ist + 1;
            ast1[at(ist, nqa + jqr, ikd)] = ifr;
            ast2[at(ist, nqa + jqr, ikd)] = jth1[k];
            const Real w1 = wfroff * wth1[k];
            h5(ist - 1, nqa + jqr - 1, ikd - 1) = wf * w1;
            h6(ist - 1, nqa + jqr - 1, ikd - 1) = wf * (w1 * w1);
          }
        }
      }
      nqa = nqa + jj;
    }
  }

  // 3.g :1441-1450
  for (int ikd = 1; ikd <= nkd; ++ikd)
    for (int iqa = 1; iqa <= static_cast<int>(nq); ++iqa)
      for (int ist = 1; ist <= 16; ++ist)
        h4(ist - 1, iqa - 1, ikd - 1) = ast1[at(ist, iqa, ikd)] * nthexp + ast2[at(ist, iqa, ikd)];
  if (nthmax < nthmx2) return kInsnl3Nthmax;
  if (nqa != static_cast<int>(nq)) return kInsnl3Nqa;

  Kokkos::deep_copy(out.xsi, hxsi);
  Kokkos::deep_copy(out.frq, hfrq);
  Kokkos::deep_copy(out.qst1, h1);
  Kokkos::deep_copy(out.qst2, h2);
  Kokkos::deep_copy(out.qst3, h3);
  Kokkos::deep_copy(out.qst4, h4);
  Kokkos::deep_copy(out.qst5, h5);
  Kokkos::deep_copy(out.qst6, h6);
  return kInsnl3Ok;
}

}  // namespace ww::snl3
