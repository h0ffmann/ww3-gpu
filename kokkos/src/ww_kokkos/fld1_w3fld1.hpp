// kokkos/src/ww_kokkos/fld1_w3fld1.hpp
// W3FLD1, the wind stress vector from the wave spectrum and the wind (Reichl, Hara and Ginis 2014),
// for one sea point: the spectrum is extended with a saturation tail, the wave-supported stress is
// integrated, and the viscous stress is found by a two-component Newton iteration on the wind
// profile. Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3fld1md.F90:93-779 at NOAA-EMC/WW3 03adda91 (the file is
// byte-identical at the pin WW3@96892c95); author B. G. Reichl. SIG2WN, WND2Z0M, WND2SAT and INFLD
// are the units fld1_sig2wn.hpp, fld1_wnd2z0m.hpp, fld1_wnd2sat.hpp and fld1_infld.hpp; APPENDTAIL
// is ww::fld2::appendtail of srce_fld2.hpp (the same routine of the same module, which W3FLD2 calls too).
// Evidence: kokkos/port_report/W3FLD1_KOKKOS.md.
//
// Arithmetic: the -O3 GIMPLE of w3fld1md.F90 under ww3_ts3's switch_ST4_PR1_FLD1 (gfortran 16.2,
// WW3's Release flags) is the reference, statement by statement:
//   - **2.0, **2 and **2.0 inside SQRT are x*x; XFR**2 is XFR*XFR; **(1.5) is the libm call powf(x, 1.5).
//   - SIN(TH(T)) and COS(TH(T)) are one sincosf; COS(ANGDIF) is a lone cosf.
//   - the products of the stress integrals keep the source order, left to right, and the sums add
//     in index order; the sums over the T loop are accumulators started at 0.0.
//   - the loop that searches the top of the wave boundary layer (KB, :564-569) has no effect on any
//     output and is deleted by the compiler; it is not translated.
//   - the first-guess loop, the convergence loop and the wind profile of the object are the
//     statements of the source, with the parenthesised groups of the source kept (PAREN_EXPR).
// Where the object puts the two operands of a commutative + or * the other way round (for instance
// UITV**2 + VITV**2), the source order is used here: only the payload of a NaN can differ.
//
// Not reproduced: the WRITE(*,*) lines of the Fortran (:733-734, :766) are reported to the caller as
// status bits, the shim prints them. FIRST/INFLD is a threadprivate SAVE in the Fortran: the tail
// parameters are those of the grid that thread called W3FLD1 for first; here they are those of the
// grid the point belongs to (see fld2 for the same caveat). The ALLOCATE of the twenty work arrays
// becomes a slice of preallocated work. RIB is not read by W3FLD1 and is not a parameter.
// ⚠ SAT stays 0 where the Fortran leaves it undefined (TAIL_ID not 0 or 1); CBETA is 0 where the
// Fortran leaves it undefined (the first WAGE a NaN); the ALLOCATE is refused as a capacity status
// when NKT < 6 (the Fortran reads WN beyond its bounds in the KA loops) or NKT > NKTMAX.
// Every libm call, APPENDTAIL's asinf and acosf included, goes through ww::dm.
// SPDX-FileCopyrightText: 2009 National Weather Service (NWS), National Oceanic and Atmospheric Administration
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "fld1_infld.hpp"
#include "fld1_sig2wn.hpp"
#include "fld1_wnd2sat.hpp"
#include "fld1_wnd2z0m.hpp"
#include "libm_device.hpp"
#include "real.hpp"
#include "srce_fld2.hpp"

namespace ww::fld1 {

/// Bits of the status w3fld1 returns.
enum W3fld1Status : int {
  kStatusBulk = 1,        ///< :764-771, "W3FLD1 failed, will output bulk..."
  kStatusNoConverge = 2,  ///< :732-738, "W3FLD1 not converged."
  kStatusCapacity = 4     ///< the extended spectrum does not fit the work array: nothing computed
};

namespace w3fld1_detail {
inline constexpr Real kTpi = static_cast<Real>(2.0) * ww::kPi;  // TPI, constants.F90:73
inline constexpr Real kKappa = 0.4F;                            // KAPPA, constants.F90:70
inline constexpr Real kDwat = 1000.F;                           // DWAT
inline constexpr Real kGrav = 9.806F;                           // GRAV
inline constexpr Real kDelta = 0.03F;                           // DELTA, :196
// 0.132 * NU, NU = 0.105/10000.0, folded by gfortran in REAL (:383).
inline constexpr Real kNu = 0.105F / 10000.0F;
inline constexpr Real kZ0smNum = 0.132F * kNu;
static_assert(kZ0smNum == 1.3860000080967438407242298126220703125e-6F,
              "0.132*NU must fold to the constant the object holds");
// 0.1 * 1.45E-5 (:576), folded in REAL.
inline constexpr Real kZnuNum = 0.1F * 1.45E-5F;
static_assert(kZnuNum == 1.4500000133921275846660137176513671875e-6F,
              "0.1*1.45E-5 must fold to the constant the object holds");
inline constexpr Real kDtx = 0.00005F;  // :369-370
static_assert(kDtx == 4.999999873689375817775726318359375e-5F, "DTX");
inline constexpr Real kTenPowM6 = 1.0e-6F;  // 10.0**(-6), :384
static_assert(kTenPowM6 == 9.999999974752427078783512115478515625e-7F, "10**-6");

/// CBETA of :432-449 and :497-510 (the same block twice, and once more in the Fortran text of the
/// second pass). `cbeta` keeps its value when WAGE is a NaN, as the object does.
KOKKOS_INLINE_FUNCTION void beta_coef(Real cosv, Real cp_z2, Real ustl, Real& cbeta) {
  if (cosv >= static_cast<Real>(0.0)) {
    const Real wage = cp_z2 / ustl;
    if (wage < static_cast<Real>(10.)) {
      cbeta = 25.0F;
    } else if (wage >= static_cast<Real>(10.0) && wage <= static_cast<Real>(25.0)) {
      cbeta = 10.0F + 15.0F * dm::cosf(ww::kPi * (wage - 10.0F) / 15.0F);
    } else if (wage > static_cast<Real>(25.0)) {
      cbeta = -5.0F;
    }
  } else {
    cbeta = -25.0F;
  }
}
}  // namespace w3fld1_detail

/// TH(1:NTH) held as a plain array, for a caller that has no TH functor of its own (and for the
/// instantiation in wave_support.cpp). th(i) = TH(i+1).
struct ThArray {
  const Real* p;
  KOKKOS_INLINE_FUNCTION Real operator()(int i) const { return p[i]; }
};

/// Reals of work w3fld1 needs for `nktmax` bins: fifteen arrays of NKT (the Fortran's WN, DWN, CP,
/// SIG2, TLTN, TLTE, TAUD, TLTND, TLTED, ZOFK, FTILDE, TLTNA, TLTEA, and APPENDTAIL's BT and ANG),
/// six of NKT+1 (UPROF, VPROF, UP1, VP1, UP, VP), SPC2 (NKT*NTH), NORMSPC and ANGLE1 (2*NTH).
inline int work_reals(int nth, int nktmax) { return nktmax * (21 + nth) + 6 + 2 * nth; }

/// W3FLD1(ASPC, FPI, WNDX, WNDY, ZWND, DEPTH, RIB, DAIR, UST, USTD, Z0, TAUNUX, TAUNUY, CHARN)
/// for one point. aspc = ASPC(1:NSPEC); fpi, wndx, wndy, zwnd, depth, dair by value (RIB is not
/// read); sig = SIG(1:NK) 0-based; th(i) = TH(i+1); the tail parameters are what INFLD copies.
/// taunux and taunuy are INTENT(INOUT) (the first guess is replaced); ust, ustd and z0 are written;
/// charn is written when it is not null (OPTIONAL in the Fortran). work has work_reals(nth, nktmax)
/// reals. Returns the W3fld1Status bits.
template <class TH>
KOKKOS_INLINE_FUNCTION int w3fld1(const Real* aspc, Real fpi, Real wndx, Real wndy, Real zwnd,
                                  Real depth, Real dair, int nk, int nth, Real xfr, Real dth,
                                  const Real* sig, const TH& th, int tail_id, Real tail_lev,
                                  Real tail_tran1, Real tail_tran2, Real& ust, Real& ustd, Real& z0,
                                  Real& taunux, Real& taunuy, Real* charn, Real* work,
                                  int nktmax) {
  using w3fld1_detail::beta_coef;
  using w3fld1_detail::kDelta;
  using w3fld1_detail::kDtx;
  using w3fld1_detail::kDwat;
  using w3fld1_detail::kGrav;
  using w3fld1_detail::kKappa;
  using w3fld1_detail::kTpi;
  const Real zero = static_cast<Real>(0.);
  int status = 0;

  // :258-261 FIRST/INFLD
  TailState tail;
  infld(tail_id, tail_lev, tail_tran1, tail_tran2, tail);

  // :262-263
  const Real wnd_in_mag = Kokkos::sqrt(wndx * wndx + wndy * wndy);
  const Real wnd_in_dir = dm::atan2f(wndy, wndx);
  // :269-275. SAT stays 0 for another TAIL_ID, which the Fortran leaves undefined.
  const Real u10 = wnd_in_mag;
  Real sat = zero;
  if (tail.tail_choice == 0) {
    sat = tail.tail_level;
  } else if (tail.tail_choice == 1) {
    wnd2sat(u10, sat);
  }

  // :286-292. NKT counts the bins until KMAX reaches 366 rad/m. XFR**2 is XFR*XFR.
  Real kmax;
  sig2wn(sig[nk - 1], depth, kmax);
  int nkt = nk;
  const Real xfr2 = xfr * xfr;
  while (kmax < 366.F) {
    nkt = nkt + 1;
    kmax = ((kmax * xfr2));
  }
  if (nkt > nktmax || nkt < 6) {
    ust = zero;
    ustd = wnd_in_dir;
    z0 = zero;
    if (charn != nullptr) *charn = zero;
    return kStatusCapacity;
  }
  // :294-298, the ALLOCATEs, as slices of `work`.
  Real* const wn = work;
  Real* const dwn = wn + nkt;
  Real* const cp = dwn + nkt;
  Real* const sig2 = cp + nkt;
  Real* const tltn = sig2 + nkt;
  Real* const tlte = tltn + nkt;
  Real* const taud = tlte + nkt;
  Real* const tltnd = taud + nkt;
  Real* const tlted = tltnd + nkt;
  Real* const zofk = tlted + nkt;
  Real* const ftilde = zofk + nkt;
  Real* const tltna = ftilde + nkt;
  Real* const tltea = tltna + nkt;
  Real* const bt = tltea + nkt;
  Real* const ang = bt + nkt;
  Real* const uprof = ang + nkt;  // NKT+1
  Real* const vprof = uprof + nkt + 1;
  Real* const up1 = vprof + nkt + 1;
  Real* const vp1 = up1 + nkt + 1;
  Real* const up = vp1 + nkt + 1;
  Real* const vp = up + nkt + 1;
  Real* const spc2 = vp + nkt + 1;  // SPC2(NKT,NTH)
  Real* const normspc = spc2 + nkt * nth;
  Real* const angle1 = normspc + nth;
  const auto S = [&](int k, int t) -> Real& { return spc2[(k - 1) + (t - 1) * nkt]; };

  // :303-313
  for (int k = 1; k <= nk; ++k) {
    sig2wn(sig[k - 1], depth, wn[k - 1]);
    cp[k - 1] = sig[k - 1] / wn[k - 1];
    sig2[k - 1] = sig[k - 1];
  }
  for (int k = nk + 1; k <= nkt; ++k) {
    sig2[k - 1] = sig2[k - 2] * xfr;
    sig2wn(sig2[k - 1], depth, wn[k - 1]);
    cp[k - 1] = sig2[k - 1] / wn[k - 1];
  }
  // :315-318 and :318, DWN(NKT) = WN(NKT)*XFR**2 - WN(NKT)
  for (int k = 1; k <= nkt - 1; ++k) dwn[k - 1] = wn[k] - wn[k - 1];
  dwn[nkt - 1] = wn[nkt - 1] * xfr2 - wn[nkt - 1];
  // :325-337. WN**3.0 is powf, one call per bin.
  {
    int i = 0;
    for (int k = 1; k <= nk; ++k)
      for (int t = 1; t <= nth; ++t) {
        ++i;
        S(k, t) = aspc[i - 1] * sig[k - 1];
      }
    for (int k = nk + 1; k <= nkt; ++k) {
      const Real pnk = dm::powf(wn[nk - 1], static_cast<Real>(3.));
      const Real pk = dm::powf(wn[k - 1], static_cast<Real>(3.));
      for (int t = 1; t <= nth; ++t) S(k, t) = (S(nk, t) * pnk) / pk;
    }
  }
  // :343-347. FPI*TPI is one product.
  Real ktaila, ktailb;
  const Real fpitpi = fpi * kTpi;
  sig2wn(fpitpi * tail.tail_transition_ratio1, depth, ktaila);
  sig2wn(fpitpi * tail.tail_transition_ratio2, depth, ktailb);
  const Real ktailc = ktailb * 2.0F;
  // :349-360
  int ka1 = 2;
  while (ktaila >= wn[ka1 - 1] && ka1 < nkt - 6) ka1 = ka1 + 1;
  int ka2 = ka1 + 2;
  while (ktailb >= wn[ka2 - 1] && ka2 < nkt - 4) ka2 = ka2 + 1;
  int ka3 = ka2 + 2;
  while (ktailc >= wn[ka3 - 1] && ka3 < nkt - 2) ka3 = ka3 + 1;
  // :363
  ww::fld2::appendtail(spc2, wn, nkt, ka1, ka2, ka3, wnd_in_dir, sat, nth, dth, th, bt, ang, normspc,
                       angle1);

  // :369-388, the smooth-law first guess. KAPPA*WND_IN_MAG is one product; the first pass
  // is peeled by gfortran but computes the same statements.
  const Real ukap = wnd_in_mag * kKappa;
  Real z0sm = 0.001F;
  Real ustsm = zero;
  {
    bool it_flag1 = true;
    int iteration = 0;
    while (it_flag1) {
      iteration = iteration + 1;
      const Real z1 = z0sm;
      ustsm = ukap / dm::logf(zwnd / z1);
      z0sm = w3fld1_detail::kZ0smNum / ustsm;
      if (Kokkos::fabs(z0sm - z1) < w3fld1_detail::kTenPowM6 || iteration > 5) it_flag1 = false;
    }
  }
  // :390-393. USTSM**2 * DAIR * WNDX / WND_IN_MAG, left to right.
  const Real ustsm2 = ustsm * ustsm;
  taunux = ((ustsm2 * dair) * wndx) / wnd_in_mag;
  taunuy = ((ustsm2 * dair) * wndy) / wnd_in_mag;

  // :397-749, the iteration on TAUNU.
  Real iter_thresh = 0.001F;
  int iteration = 1;
  bool it_flag1 = true;
  Real cbeta = zero;  // ⚠ undefined in the Fortran until the first WAGE
  Real wnd_1x = zero, wnd_1y = zero, wnd_2x = zero, wnd_2y = zero, wnd_3x = zero, wnd_3y = zero;
  Real taux = zero, tauy = zero;
  Real ustar = zero, taudir = zero;
  while (it_flag1) {
    for (int iter = 1; iter <= 3; ++iter) {
      int z2 = nkt;
      // :402-411
      if (iter == 1) {
        taunux = taunux + kDtx;
      } else if (iter == 2) {
        taunux = taunux - kDtx;
        taunuy = taunuy + kDtx;
      } else if (iter == 3) {
        taunuy = taunuy - kDtx;
      }
      // :413-417
      tltn[0] = taunuy;
      tlte[0] = taunux;
      ww::fld2::appendtail(spc2, wn, nkt, ka1, ka2, ka3, dm::atan2f(taunuy, taunux), sat, nth, dth,
                           th, bt, ang, normspc, angle1);
      // :422-473, first guess of the growth rate and the local turbulent stress.
      for (int zi = 2; zi <= nkt; ++zi) {
        tltnd[zi - 1] = zero;
        tlted[zi - 1] = zero;
        z2 = z2 - 1;
        taud[zi - 1] = dm::atan2f(tltn[zi - 2], tlte[zi - 2]);
        const Real sq = Kokkos::sqrt(tltn[zi - 2] * tltn[zi - 2] + tlte[zi - 2] * tlte[zi - 2]);
        const Real ustl = Kokkos::sqrt(sq / dair);
        const Real wn2 = wn[z2 - 1] * wn[z2 - 1];
        Real acc_n = tltnd[zi - 1];
        Real acc_e = tlted[zi - 1];
        for (int t = 1; t <= nth; ++t) {
          const Real angdif = taud[zi - 1] - th(t - 1);
          const Real c = dm::cosf(angdif);
          beta_coef(c, cp[z2 - 1], ustl, cbeta);
          Real sn = zero, cs = zero;
          dm::sincosf(th(t - 1), sn, cs);
          const Real c2 = c * c;
          acc_n = acc_n + (((((((sn * c2) * cbeta) * S(z2, t)) * sq) * wn2) * dth));
          acc_e = acc_e + (((((((cs * c2) * cbeta) * S(z2, t)) * sq) * wn2) * dth));
        }
        tltnd[zi - 1] = acc_n;
        tlted[zi - 1] = acc_e;
        // :463-472
        if (zi == 2) {
          tltna[zi - 1] = (tltnd[zi - 1] * dwn[z2 - 1]) * 0.5F;
          tltea[zi - 1] = (tlted[zi - 1] * dwn[z2 - 1]) * 0.5F;
        } else {
          tltna[zi - 1] = ((tltnd[zi - 1] + tltnd[zi - 2]) * 0.5F) * dwn[z2 - 1];
          tltea[zi - 1] = ((tlted[zi - 1] + tlted[zi - 2]) * 0.5F) * dwn[z2 - 1];
        }
        tltn[zi - 1] = tltn[zi - 2] + tltna[zi - 1];
        tlte[zi - 1] = tlte[zi - 2] + tltea[zi - 1];
      }
      tauy = tltn[nkt - 1];
      taux = tlte[nkt - 1];
      // :480-481
      Real ustrb = Kokkos::sqrt(Kokkos::sqrt(tauy * tauy + taux * taux) / dair);
      Real taudirb = dm::atan2f(tauy, taux);
      // :482-561, iterate to convergence.
      bool it_flag2 = true;
      int ctr = 1;
      while (it_flag2 && ctr < 10) {
        z2 = nkt + 1;
        for (int zi = 1; zi <= nkt; ++zi) {
          z2 = z2 - 1;
          tlted[zi - 1] = zero;
          tltnd[zi - 1] = zero;
          ftilde[z2 - 1] = zero;
          taud[zi - 1] = dm::atan2f(tltn[zi - 1], tlte[zi - 1]);
          const Real sq = Kokkos::sqrt(tltn[zi - 1] * tltn[zi - 1] + tlte[zi - 1] * tlte[zi - 1]);
          const Real ustl = Kokkos::sqrt(sq / dair);
          const Real wn2 = wn[z2 - 1] * wn[z2 - 1];
          const Real cp2 = cp[z2 - 1] * cp[z2 - 1];
          Real acc_n = tltnd[zi - 1];
          Real acc_e = tlted[zi - 1];
          Real acc_f = ftilde[z2 - 1];
          for (int t = 1; t <= nth; ++t) {
            const Real angdif = taud[zi - 1] - th(t - 1);
            const Real c = dm::cosf(angdif);
            beta_coef(c, cp[z2 - 1], ustl, cbeta);
            Real sn = zero, cs = zero;
            dm::sincosf(th(t - 1), sn, cs);
            const Real c2 = c * c;
            // :511-514. BP = SQRT((COS*COS**2)**2 + (SIN*COS**2)**2).
            const Real bpa = cs * c2;
            const Real bpb = sn * c2;
            const Real bp = Kokkos::sqrt(bpa * bpa + bpb * bpb);
            const Real betag = ((((bp * cbeta) * sq) / kDwat) * sig2[z2 - 1]) / cp2;
            acc_f = acc_f + ((((betag * kDwat) * kGrav) * S(z2, t)) * dth);
            acc_n = acc_n + (((((sn * c2) * cbeta) * S(z2, t)) * sq) * wn2) * dth;
            acc_e = acc_e + (((((cs * c2) * cbeta) * S(z2, t)) * sq) * wn2) * dth;
          }
          ftilde[z2 - 1] = acc_f;
          tltnd[zi - 1] = acc_n;
          tlted[zi - 1] = acc_e;
          // :526-532
          if (zi == 1) {
            tltna[zi - 1] = (tltnd[zi - 1] * dwn[z2 - 1]) * 0.5F;
            tltea[zi - 1] = (tlted[zi - 1] * dwn[z2 - 1]) * 0.5F;
          } else {
            tltna[zi - 1] = ((tltnd[zi - 1] + tltnd[zi - 2]) * 0.5F) * dwn[z2 - 1];
            tltea[zi - 1] = ((tlted[zi - 1] + tlted[zi - 2]) * 0.5F) * dwn[z2 - 1];
          }
          // :533-539
          if (zi > 1) {
            tltn[zi - 1] = tltn[zi - 2] + tltna[zi - 1];
            tlte[zi - 1] = tlte[zi - 2] + tltea[zi - 1];
          } else {
            tltn[zi - 1] = taunuy + tltna[zi - 1];
            tlte[zi - 1] = taunux + tltea[zi - 1];
          }
        }
        tauy = tltn[nkt - 1];
        taux = tlte[nkt - 1];
        ustar = Kokkos::sqrt(Kokkos::sqrt(tauy * tauy + taux * taux) / dair);
        taudir = dm::atan2f(tauy, taux);
        // :547-552
        const bool crit1 =
            (Kokkos::fabs(ustar - ustrb) * 100.0F) / ((ustar + ustrb) * 0.5F) > 0.1F;
        bool crit2;
        if ((taudir + taudirb) != zero) {
          crit2 = (Kokkos::fabs(taudir - taudirb) * 100.0F / (taudir + taudirb) * 0.5F) > 0.1F;
        } else {
          crit2 = true;
        }
        if (crit1 || crit2) {
          ustrb = ustar;
          taudirb = taudir;
          ctr = ctr + 1;
        } else {
          it_flag2 = false;
        }
      }
      // :564-569, the search of KB has no effect on an output (see the heritage note).

      // :573-602, the wind profile.
      for (int i = 1; i <= nkt; ++i) zofk[i - 1] = kDelta / wn[i - 1];
      const Real znu =
          w3fld1_detail::kZnuNum / Kokkos::sqrt(Kokkos::sqrt(taunux * taunux + taunuy * taunuy) / dair);
      for (int i = 0; i <= nkt; ++i) {
        uprof[i] = zero;
        vprof[i] = zero;
      }
      {
        int zi = 1;
        z2 = nkt;
        const Real a = ((wn[z2 - 1] * wn[z2 - 1]) / kDelta) * ftilde[z2 - 1];
        const Real r = Kokkos::sqrt(tltn[zi - 1] * tltn[zi - 1] + tlte[zi - 1] * tlte[zi - 1]) / dair;
        const Real b = (dair / (zofk[z2 - 1] * kKappa)) * dm::powf(r, 1.5F);
        const Real den = tlte[zi - 1] * taux + tltn[zi - 1] * tauy;
        up1[zi - 1] = ((a + b) * tlte[zi - 1]) / den;
        vp1[zi - 1] = ((a + b) * tltn[zi - 1]) / den;
        up[zi - 1] = up1[zi - 1];
        vp[zi - 1] = vp1[zi - 1];
        const Real pw = dm::powf(Kokkos::sqrt(taunux * taunux + taunuy * taunuy) / dair, 1.5F);
        const Real den0 = taux * taunux + tauy * taunuy;
        const Real lg = dm::logf(zofk[z2 - 1] / znu);
        uprof[zi - 1] = dair / kKappa * pw * (taunux / den0) * lg;
        vprof[zi - 1] = dair / kKappa * pw * (taunuy / den0) * lg;
      }
      // :605-621
      for (int zi = 2; zi <= nkt; ++zi) {
        z2 = z2 - 1;
        const Real a = ((wn[z2 - 1] * wn[z2 - 1]) / kDelta) * ftilde[z2 - 1];
        const Real r = Kokkos::sqrt(tltn[zi - 1] * tltn[zi - 1] + tlte[zi - 1] * tlte[zi - 1]) / dair;
        const Real b = (dair / (zofk[z2 - 1] * kKappa)) * dm::powf(r, 1.5F);
        const Real den = tlte[zi - 1] * taux + tltn[zi - 1] * tauy;
        up1[zi - 1] = ((a + b) * tlte[zi - 1]) / den;
        vp1[zi - 1] = ((a + b) * tltn[zi - 1]) / den;
        up[zi - 1] = up1[zi - 1] * 0.5F + up1[zi - 2] * 0.5F;
        vp[zi - 1] = vp1[zi - 1] * 0.5F + vp1[zi - 2] * 0.5F;
        uprof[zi - 1] = uprof[zi - 2] + up[zi - 1] * (zofk[z2 - 1] - zofk[z2]);
        vprof[zi - 1] = vprof[zi - 2] + vp[zi - 1] * (zofk[z2 - 1] - zofk[z2]);
      }
      // :627-634, the wind at ZWND. SQRT(SQRT(TAUY**2+TAUX**2)/DAIR) is the USTAR of the loop.
      {
        const Real us = Kokkos::sqrt(Kokkos::sqrt(tauy * tauy + taux * taux) / dair);
        const Real sqt = Kokkos::sqrt(tauy * tauy + taux * taux);
        const Real lg = dm::logf(zwnd / zofk[z2 - 1]);
        uprof[nkt] = uprof[nkt - 1] + us / kKappa * taux / sqt * lg;
        vprof[nkt] = vprof[nkt - 1] + us / kKappa * tauy / sqt * lg;
      }
      // :635-644
      if (iter == 3) {
        wnd_1x = uprof[nkt];
        wnd_1y = vprof[nkt];
      } else if (iter == 2) {
        wnd_2x = uprof[nkt];
        wnd_2y = vprof[nkt];
      } else if (iter == 1) {
        wnd_3x = uprof[nkt];
        wnd_3y = vprof[nkt];
      }
    }
    // :689-697
    iteration = iteration + 1;
    const Real difu10xx = wnd_3x - wnd_1x;
    const Real difu10yx = wnd_3y - wnd_1y;
    const Real difu10xy = wnd_2x - wnd_1x;
    const Real difu10yy = wnd_2y - wnd_1y;
    const Real fd_a = difu10xx / kDtx;
    const Real fd_b = difu10xy / kDtx;
    const Real fd_c = difu10yx / kDtx;
    const Real fd_d = difu10yy / kDtx;
    const Real dwndx = -wndx + wnd_1x;
    const Real dwndy = -wndy + wnd_1y;
    const Real uitv = Kokkos::fabs(dwndx);
    const Real vitv = Kokkos::fabs(dwndy);
    const Real ch = Kokkos::sqrt(uitv * uitv + vitv * vitv);
    Real apar;
    if (ch > 15.F) {
      apar = 0.5F / (fd_a * fd_d - fd_b * fd_c);
    } else {
      apar = 1.0F / (fd_a * fd_d - fd_b * fd_c);
    }
    const Real ck = 4.F;
    // MAX(ABS(WNDY),CK): maxss with ABS(WNDY) first, so a NaN gives CK.
    const Real my = Kokkos::fabs(wndy) > ck ? Kokkos::fabs(wndy) : ck;
    const Real mx = Kokkos::fabs(wndx) > ck ? Kokkos::fabs(wndx) : ck;
    const bool over = (vitv / my > iter_thresh) || (uitv / mx > iter_thresh);
    // :709-743. The first branch (ITERATION < 2) cannot be taken: ITERATION is at least 2 here.
    if (over && iteration < 2) {
      taunux = taunux - apar * (fd_d * dwndx - fd_b * dwndy);
      taunuy = taunuy - apar * (-fd_c * dwndx + fd_a * dwndy);
    } else if (over && iteration < 24) {
      iter_thresh = 0.001F;
      taunux = taunux - apar * (fd_d * dwndx - fd_b * dwndy);
      taunuy = taunuy - apar * (-fd_c * dwndx + fd_a * dwndy);
    } else if (over && iteration < 26) {
      iter_thresh = 0.01F;
      taunux = taunux - apar * (fd_d * dwndx - fd_b * dwndy);
      taunuy = taunuy - apar * (-fd_c * dwndx + fd_a * dwndy);
    } else if (over && iteration < 30) {
      iter_thresh = 0.05F;
      taunux = taunux - apar * (fd_d * dwndx - fd_b * dwndy);
      taunuy = taunuy - apar * (-fd_c * dwndx + fd_a * dwndy);
    } else if (iteration >= 30) {
      status |= kStatusNoConverge;
      it_flag1 = false;
      taunux = zero;
      taunuy = zero;
    } else if ((vitv / my < iter_thresh) && (uitv / mx < iter_thresh) && iteration >= 2) {
      it_flag1 = false;
    }
    // :745-748
    if (!(dm::cosf(wnd_in_dir - dm::atan2f(taunuy, taunux)) >= static_cast<Real>(0.0))) {
      taunux = ((ustsm2 * dair) * wndx) / wnd_in_mag * 0.95F;
      taunuy = ((ustsm2 * dair) * wndy) / wnd_in_mag * 0.95F;
    }
  }

  // :753-758
  ustd = dm::atan2f(tauy, taux);
  ust = Kokkos::sqrt(Kokkos::sqrt(taux * taux + tauy * tauy) / dair);
  const Real wnd_pa = wnd_in_mag * dm::cosf(wnd_in_dir - ustd);
  z0 = zwnd / dm::expf(wnd_pa * kKappa / ust);
  const Real ust2 = ust * ust;
  const Real cd = ust2 / (wnd_in_mag * wnd_in_mag);
  // :759-761
  if (charn != nullptr)
    *charn = 0.01F / Kokkos::sqrt(Kokkos::sqrt(taunux * taunux + taunuy * taunuy) / ust2);
  // :762-771, the fail-safe to the bulk stress
  const bool fsfl1 = !(cd < 0.01F && cd > 0.0001F);
  const bool fsfl2 = !(dm::cosf(wnd_in_dir - ustd) > 0.9F);
  if (fsfl1 || fsfl2) {
    status |= kStatusBulk;
    wnd2z0m(wnd_in_mag, z0);
    ust = ukap / dm::logf(zwnd / z0);
    ustd = wnd_in_dir;
  }
  return status;
}

}  // namespace ww::fld1
