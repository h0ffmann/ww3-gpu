// kokkos/src/ww_kokkos/part_ptmean.hpp
// PTMEAN: the integrated parameters of each spectral partition (Hs, peak and mean periods,
// directions, spread, wind-sea fraction and the rest of XP(1:15)), one spectrum per call.
// Phase 1, translated, not regtested. WW3 heritage: WW3-noaa/model/src/w3partmd.F90:1175-1538
// at NOAA-EMC/WW3 03adda91; authors B. Tracy, H. L. Tolman. Evidence:
// kokkos/port_report/PTMEAN_KOKKOS.md.
//
// Arrays are 0-based C pointers holding the Fortran's values; NSPEC = NK*NTH, ISP = IK + (ITH-1)*NK.
//
//   int ptmean(g, o, npi, imo, zp, depth, uabs, udir, wn, npo, xp, dimxp, pmap, w)
//     g     PtmeanGrid: NK, NTH, DTH, XFR, FACHFE, FTE, SIG(0:NK+1), DSIP(0:NK+1), DSII(1:NK),
//           TH, ECOS, ESIN (1:NTH) of W3GDATMD, and the DISTAB table WAVNU1 reads.
//     o     PtmeanOpts: HSPMIN, WSMULT, PTMETH of W3ODATMD, UNDEF of CONSTANTS (a variable,
//           constants.F90:85, so read at call time).
//     npi   partitions found (W3PART passes NP_MAX); imo[nspec] labels 0..npi; zp[nspec]; wn[nk].
//     npo   out, XP columns written minus one (-1 .. dimxp); xp[15*(dimxp+1)], XP(J,N) =
//           xp[(J-1) + 15*N], all zeroed first; pmap[dimxp], PMAP(N) = pmap[N-1], only the
//           written entries are touched.
//     w     PtmeanScratch, carved by ptmean_carve() from ptmean_nreal() Reals and npi+1 ints.
//   Returns kPtmeanOk, or
//     kPtmeanXpFull  NPO reached DIMXP (:1415-1418): the Fortran writes format 1000 with NPO+1
//                    on NDSE when IAPROC == NAPERR and returns; the shim does that write.
//     kPtmeanNoPeak  IFPMAX(IP) = 0 (no SUMF > 0) with HS >= HSPMIN, reachable for HSPMIN <= 0 or
//                    a NaN spectrum (ww3_grid sets HSPMIN >= 1E-4, w3gridmd.F90:3141, so in WW3
//                    only the latter); the Fortran then reads SUMF(-1,IP), SUMFX(0,IP) and DSII(0)
//                    out of bounds (:1436, :1489). The shim calls the Fortran PTMEAN for that point.
//
// ⚠ On a device cosf, logf, atan2f and fmodf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "prop_params.hpp"
#include "prop_wavnu1.hpp"
#include "real.hpp"
#include "st4_params.hpp"

namespace ww::part {

inline constexpr int kDimp = 15;  // DIMP, w3odatmd.F90:332

inline constexpr int kPtmeanOk = 0;
inline constexpr int kPtmeanXpFull = 1;
inline constexpr int kPtmeanNoPeak = 2;

struct PtmeanGrid {
  int nk, nth;
  Real dth, xfr, fachfe, fte;
  const Real* sig;   // SIG(0:NK+1), sig[ik]
  const Real* dsip;  // DSIP(0:NK+1), dsip[ik]
  const Real* dsii;  // DSII(1:NK), dsii[ik-1]
  const Real* th;    // TH(1:NTH), th[ith-1]; ECOS, ESIN likewise
  const Real* ecos;
  const Real* esin;
  ww::prop::DispTable disp;
};

struct PtmeanOpts {
  Real hspmin, wsmult;
  int ptmeth;
  Real undef;
};

struct PtmeanScratch {
  Real* c;       // C(NK)
  Real* fcdir;   // FCDIR(NTH)
  Real* sumf;    // SUMF(0:NK+1,0:NPI), column-major as the Fortran's
  Real* sumfw;   // SUMFW(NK,0:NPI); SUMFX, SUMFY likewise
  Real* sumfx;
  Real* sumfy;
  Real* sume;    // SUME(0:NPI) and the other per-partition sums
  Real* sumew;
  Real* sumex;
  Real* sumey;
  Real* efpmax;
  Real* sume1;
  Real* sume2;
  Real* sumem1;
  Real* sumqp;
  int* ifpmax;   // IFPMAX(0:NPI)
};

KOKKOS_INLINE_FUNCTION int ptmean_nreal(int nk, int nth, int npi) {
  return nk + nth + (nk + 2) * (npi + 1) + 3 * nk * (npi + 1) + 9 * (npi + 1);
}

KOKKOS_INLINE_FUNCTION PtmeanScratch ptmean_carve(Real* r, int* ibuf, int nk, int nth, int npi) {
  const int np = npi + 1;
  PtmeanScratch w;
  w.c = r;
  w.fcdir = w.c + nk;
  w.sumf = w.fcdir + nth;
  w.sumfw = w.sumf + (nk + 2) * np;
  w.sumfx = w.sumfw + nk * np;
  w.sumfy = w.sumfx + nk * np;
  w.sume = w.sumfy + nk * np;
  w.sumew = w.sume + np;
  w.sumex = w.sumew + np;
  w.sumey = w.sumex + np;
  w.efpmax = w.sumey + np;
  w.sume1 = w.efpmax + np;
  w.sume2 = w.sume1 + np;
  w.sumem1 = w.sume2 + np;
  w.sumqp = w.sumem1 + np;
  w.ifpmax = ibuf;
  return w;
}

namespace ptmean_detail {
using ww::st4::max_r;  // (a > b) ? a : b, maxss with a the destination: NaN or a tie gives b
using ww::st4::min_r;

inline constexpr Real kTpi = ww::st4::kTpi;
inline constexpr Real kTpiInv = ww::st4::kTpiInv;
inline constexpr Real kRade = ww::st4::kRade;
inline constexpr Real kDera = ww::prop::kDera;
static_assert(kTpi == 0x1.921fb6p+2f && kTpiInv == 0x1.45f306p-3f && kRade == 0x1.ca5dc0p+5f);
// Folded by gfortran in w3partmd.o and written as the object's bits; for these two the exact
// power rounded once and the float product chain agree.
inline constexpr Real kTpiInv2 = 0x1.9f02f4p-6f;  // TPIINV**2 :1425, 0x3ccf817a
inline constexpr Real kTpiInv3 = 0x1.08345cp-8f;  // TPIINV**3 :1426, 0x3b841a2e
static_assert(kTpiInv2 == kTpiInv * kTpiInv);
static_assert(kTpiInv3 == kTpiInv * kTpiInv * kTpiInv);
inline constexpr Real kTail1 = 0x1.554c98p+0f;  // 0.3333/0.25 :1381, 0x3faaa64c
inline constexpr Real kTail2 = static_cast<Real>(2.);  // 0.5/0.25 :1382
inline constexpr Real kTailM1 = 0x1.99999ap-1f;  // 0.2/0.25 :1383, 0x3f4ccccd
static_assert(kTail1 == static_cast<Real>(0.3333) / static_cast<Real>(0.25));
static_assert(kTailM1 == static_cast<Real>(0.2) / static_cast<Real>(0.25));
}  // namespace ptmean_detail

/// PTMEAN (w3partmd.F90:1175-1538). Sums in the Fortran's loop order; no reduction is reordered.
KOKKOS_INLINE_FUNCTION int ptmean(const PtmeanGrid& g, const PtmeanOpts& o, int npi,
                                  const int* imo, const Real* zp, Real depth, Real uabs,
                                  Real udir, const Real* wn, int& npo, Real* xp, int dimxp,
                                  int* pmap, const PtmeanScratch& w) {
  using namespace ptmean_detail;
  const int nk = g.nk, nth = g.nth;
  const Real zero = static_cast<Real>(0.), one = static_cast<Real>(1.);
  const int ldf = nk + 2;  // leading dimension of SUMF
  auto SUMF = [&](int ik, int ip) -> Real& { return w.sumf[ik + ip * ldf]; };
  auto SUMFW = [&](int ik, int ip) -> Real& { return w.sumfw[(ik - 1) + ip * nk]; };
  auto SUMFX = [&](int ik, int ip) -> Real& { return w.sumfx[(ik - 1) + ip * nk]; };
  auto SUMFY = [&](int ik, int ip) -> Real& { return w.sumfy[(ik - 1) + ip * nk]; };
  auto XP = [&](int j, int n) -> Real& { return xp[(j - 1) + kDimp * n]; };

  npo = 0;  // :1274
  for (int i = 0; i < kDimp * (dimxp + 1); ++i) xp[i] = zero;
  if (npi == 0) return kPtmeanOk;

  for (int i = 0; i < ldf * (npi + 1); ++i) w.sumf[i] = zero;  // :1282-1297
  for (int i = 0; i < nk * (npi + 1); ++i) {
    w.sumfw[i] = zero;
    w.sumfx[i] = zero;
    w.sumfy[i] = zero;
  }
  for (int ip = 0; ip <= npi; ++ip) {
    w.sume[ip] = zero;
    w.sume1[ip] = zero;
    w.sume2[ip] = zero;
    w.sumem1[ip] = zero;
    w.sumqp[ip] = zero;
    w.sumew[ip] = zero;
    w.sumex[ip] = zero;
    w.sumey[ip] = zero;
    w.ifpmax[ip] = 0;
    w.efpmax[ip] = zero;
  }

  for (int ik = 1; ik <= nk; ++ik) w.c[ik - 1] = g.sig[ik] / wn[ik - 1];  // :1299-1301

  for (int ith = 1; ith <= nth; ++ith) {  // :1303-1318
    const Real upar =
        o.wsmult * uabs * max_r(Kokkos::cos(g.th[ith - 1] - kDera * udir), zero);
    if (upar < w.c[nk - 1]) {
      w.fcdir[ith - 1] = g.sig[nk + 1];
    } else {
      int ik = nk - 1;  // DO IK=NK-1,2,-1: IK = 1 after a full loop, NK-1 when NK < 3
      for (; ik >= 2; --ik) {
        if (upar < w.c[ik - 1]) break;
      }
      Real rd = (w.c[ik - 1] - upar) / (w.c[ik - 1] - w.c[ik]);
      if (rd < zero) {
        ik = 0;
        rd = max_r(rd + one, zero);
      }
      w.fcdir[ith - 1] = rd * g.sig[ik + 1] + (one - rd) * g.sig[ik];
    }
  }

  for (int ik = 1; ik <= nk; ++ik) {  // :1325-1341
    for (int ith = 1; ith <= nth; ++ith) {
      const int isp = ik + (ith - 1) * nk;
      const int ip = imo[isp - 1];
      const Real fact = max_r(
          min_r(one - (w.fcdir[ith - 1] - static_cast<Real>(0.5) * (g.sig[ik - 1] + g.sig[ik])) /
                          g.dsip[ik],
                one),
          zero);
      const Real z = zp[isp - 1];
      SUMF(ik, 0) = SUMF(ik, 0) + z;
      SUMFW(ik, 0) = SUMFW(ik, 0) + z * fact;
      SUMFX(ik, 0) = SUMFX(ik, 0) + z * g.ecos[ith - 1];
      SUMFY(ik, 0) = SUMFY(ik, 0) + z * g.esin[ith - 1];
      if (ip == 0) continue;
      SUMF(ik, ip) = SUMF(ik, ip) + z;
      SUMFW(ik, ip) = SUMFW(ik, ip) + z * fact;
      SUMFX(ik, ip) = SUMFX(ik, ip) + z * g.ecos[ith - 1];
      SUMFY(ik, ip) = SUMFY(ik, ip) + z * g.esin[ith - 1];
    }
  }
  for (int ip = 0; ip <= npi; ++ip) SUMF(nk + 1, ip) = SUMF(nk, ip) * g.fachfe;  // :1342

  for (int ip = 0; ip <= npi; ++ip) {  // :1344-1390
    for (int ik = 1; ik <= nk; ++ik) {
      const Real f = SUMF(ik, ip), ds = g.dsii[ik - 1], s = g.sig[ik];
      w.sume[ip] = w.sume[ip] + f * ds;
      w.sumqp[ip] = w.sumqp[ip] + (f * f) * ds * s;
      w.sume1[ip] = w.sume1[ip] + f * ds * s;
      w.sume2[ip] = w.sume2[ip] + f * ds * (s * s);
      w.sumem1[ip] = w.sumem1[ip] + f * ds / s;
      w.sumew[ip] = w.sumew[ip] + SUMFW(ik, ip) * ds;
      w.sumex[ip] = w.sumex[ip] + SUMFX(ik, ip) * ds;
      w.sumey[ip] = w.sumey[ip] + SUMFY(ik, ip) * ds;
      if (f > w.efpmax[ip]) {
        w.ifpmax[ip] = ik;
        w.efpmax[ip] = f;
      }
    }
    const Real snk = g.sig[nk], fnk = SUMF(nk, ip);
    const Real fteii = g.fte / (g.dth * snk);  // :1379
    w.sume[ip] = w.sume[ip] + fnk * fteii;
    w.sume1[ip] = w.sume1[ip] + fnk * snk * fteii * kTail1;
    w.sume2[ip] = w.sume2[ip] + fnk * (snk * snk) * fteii * kTail2;
    w.sumem1[ip] = w.sumem1[ip] + fnk / snk * fteii * kTailM1;
    w.sumqp[ip] = w.sumqp[ip] + fnk * fteii;
    w.sumew[ip] = w.sumew[ip] + SUMFW(nk, ip) * fteii;
    w.sumex[ip] = w.sumex[ip] + SUMFX(nk, ip) * fteii;
    w.sumey[ip] = w.sumey[ip] + SUMFY(nk, ip) * fteii;
  }

  npo = -1;  // :1394
  for (int ip = 0; ip <= npi; ++ip) {
    const Real dth = g.dth;
    const Real m0 = w.sume[ip] * dth * kTpiInv;
    const Real hs = static_cast<Real>(4.) * Kokkos::sqrt(max_r(zero, m0));  // maxss 0,M0: NaN stays
    if (hs < o.hspmin) {
      if (o.ptmeth == 4 || o.ptmeth == 5) {
        npo = npo + 1;
        for (int j = 1; j <= kDimp; ++j) XP(j, npo) = o.undef;
        XP(6, npo) = zero;
      }
      continue;
    }
    if (npo >= dimxp) return kPtmeanXpFull;  // :1415
    npo = npo + 1;
    if (ip > 0) {
      if (npo < 1) continue;
      pmap[npo - 1] = ip;
    }

    const Real m1 = w.sume1[ip] * dth * kTpiInv2;
    const Real m2 = w.sume2[ip] * dth * kTpiInv3;
    const Real mm1 = w.sumem1[ip] * dth;
    const Real dt = dth * kTpiInv;
    const Real qp = w.sumqp[ip] * (dt * dt);

    const int ifp = w.ifpmax[ip];
    if (ifp == 0) return kPtmeanNoPeak;
    const Real xl = one / g.xfr - one;
    const Real xh = g.xfr - one;
    const Real xl2 = xl * xl;
    const Real xh2 = xh * xh;
    Real el = SUMF(ifp - 1, ip) - SUMF(ifp, ip);
    Real eh = SUMF(ifp + 1, ip) - SUMF(ifp, ip);
    Real denom = xl * eh - xh * el;
    Real sigp = g.sig[ifp];
    if (denom != zero) {  // SIGN(ABS(DENOM),DENOM) is DENOM, as the object folds it
      sigp = sigp * (one + static_cast<Real>(0.5) * (xl2 * eh - xh2 * el) / denom);
    }
    Real wnp, cgp;
    ww::prop::wavnu1(g.disp, sigp, depth, wnp, cgp);

    Real efp = SUMF(ifp, ip) * dth;  // :1447-1457, parabolic fit at the peak
    if (ifp > 1 && ifp < nk) {
      el = SUMF(ifp - 1, ip) * dth;
      eh = SUMF(ifp + 1, ip) * dth;
      const Real d = el - eh;
      const Real numer = static_cast<Real>(0.125) * (d * d);
      denom = el - static_cast<Real>(2.) * efp + eh;
      if (denom != zero) efp = efp - numer / denom;
    }
    w.efpmax[ip] = efp;

    Real fsprd = o.undef;  // :1464-1487
    Real sumy = zero, sumxy = zero, sumxxy = zero, sumylogy = zero, sumxylogy = zero;
    for (int ik = 1; ik <= nk; ++ik) {
      const Real y = SUMF(ik, ip) * dth;
      if (y >= static_cast<Real>(1.E-15)) {
        const Real yhat = Kokkos::log(y);
        const Real t = (g.sig[ik] - sigp) * kTpiInv;
        const Real xhat = -(static_cast<Real>(0.5) * (t * t));
        sumy = sumy + y;
        sumxy = sumxy + xhat * yhat;
        sumxxy = sumxxy + xhat * xhat * y;
        sumylogy = sumylogy + y * yhat;
        sumxylogy = sumxylogy + sumxy * yhat;
      }
    }
    const Real numer = sumy * sumxxy - sumxy * sumxy;
    denom = sumy * sumxylogy - sumxy * sumylogy;
    if (denom != zero) fsprd = Kokkos::sqrt(numer / Kokkos::copysign(denom, numer));

    const Real sumexp = SUMFX(ifp, ip) * g.dsii[ifp - 1];
    const Real sumeyp = SUMFY(ifp, ip) * g.dsii[ifp - 1];

    const Real sume = w.sume[ip], sumex = w.sumex[ip], sumey = w.sumey[ip];
    const Real k630 = static_cast<Real>(630.), k360 = static_cast<Real>(360.);
    XP(1, npo) = hs;
    XP(2, npo) = kTpi / sigp;
    XP(3, npo) = kTpi / wnp;
    XP(4, npo) = Kokkos::fmod(k630 - Kokkos::atan2(sumey, sumex) * kRade, k360);
    XP(5, npo) = kRade * Kokkos::sqrt(max_r(
                             static_cast<Real>(2.) *
                                 (one - Kokkos::sqrt(max_r(
                                            (sumex * sumex + sumey * sumey) / (sume * sume), zero))),
                             zero));
    XP(6, npo) = w.sumew[ip] / sume;
    XP(7, npo) = Kokkos::fmod(k630 - Kokkos::atan2(sumeyp, sumexp) * kRade, k360);
    XP(8, npo) = Kokkos::sqrt(max_r(m2 * m0 / (m1 * m1), one) - one);
    const Real s2 = sigp * sigp;
    const Real alp_pm = static_cast<Real>(0.3125) * (hs * hs) * (s2 * s2);
    const Real sigm5 = one / (s2 * sigp * s2);  // SIGP**(-5) as the object's power tree
    const Real epm_fp = alp_pm * kTpi * sigm5 * static_cast<Real>(2.865048E-1);
    XP(9, npo) = max_r(efp / epm_fp, one);
    XP(10, npo) = static_cast<Real>(2.) * qp / (m0 * m0);
    XP(11, npo) = fsprd;
    XP(12, npo) = mm1 / m0;
    XP(13, npo) = m0 / m1;
    XP(14, npo) = Kokkos::sqrt(m0 / m2);
    XP(15, npo) = efp;
  }
  return kPtmeanOk;
}

}  // namespace ww::part
