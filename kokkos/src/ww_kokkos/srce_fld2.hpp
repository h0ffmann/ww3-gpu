// kokkos/src/ww_kokkos/srce_fld2.hpp
// W3FLD2, the wind stress from the wave spectrum (Donelan et al. 2012), with the APPENDTAIL of
// W3FLD1MD it calls, for one sea point inside the W3SRCE team body. Phase 1, translated, not
// regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3fld2md.F90:65-590 (W3FLD2) and
// WW3-noaa/model/src/w3fld1md.F90:878-1170 (APPENDTAIL) at NOAA-EMC/WW3 03adda91; author
// B. G. Reichl. SIG2WN, WND2Z0M, WND2SAT and INFLD are the units fld1_sig2wn.hpp,
// fld1_wnd2z0m.hpp, fld2_wnd2sat.hpp and fld1_infld.hpp.
//
// Arithmetic: the -O3 GIMPLE and assembly of w3fld2md.F90 and w3fld1md.F90 under the GFS-Wave
// regtests' switch (gfortran 16.2, WW3's Release flags) is the reference, statement by statement:
//   - **2.0 is x*x; **3.0 is the libm call powf(x, 3.), kept, never x*x*x; XFR**2 is XFR*XFR.
//   - /2.0 is *0.5 (DWN); the ((...)) of the GIMPLE (PAREN_EXPR) only stops reassociation, which
//     this build does not do either.
//   - COS(TH(T)) and SIN(TH(T)) are one sincosf, COS(WND_10_DIR-TH(T)) a lone cosf; the sum
//     TAUINTX(K) + SPC2*SCIN*COS*SIG2*DTH keeps its left-to-right product order.
//   - TAUXW and TAUYW, SUM(...) and the AVG sums are in-order reductions (the vector loops of the
//     object add lane 0, 1, 2, 3 in turn), so they are plain loops here.
//   - APPENDTAIL divides by WN2(K)**3.0 through libmvec's _ZGVbN4vv_powf for the first
//     4*(NTH/4) directions when NTH >= 4 and through scalar powf for the rest (the loops at
//     :1112-1117, :1134-1139 and :1151-1157 of w3fld1md.F90 are vectorised by gfortran); the
//     argument is the same for every lane, so one lane of the vector function gives its bits.
//   - The first pass of the smooth-roughness iteration (:446-456) is peeled by gfortran and its
//     LOG(10./0.001) is the constant 9.2103404998779296875, which is used here.
// MAX/MIN keep the operand order of the object where it differs from the GIMPLE (see the sites).
//
// Not reproduced: the PRINT* lines of the Fortran (:488, :575-576) are reported to the caller
// as status bits, the shim prints them. FIRST/INFLD is a threadprivate SAVE in the Fortran: the
// tail parameters are those of the grid that thread called W3FLD2 for first; here they are those
// of the grid the point belongs to. They are the same unless two grids of one run have different
// FLD namelists, which W3KOKKOS_SRCE_SETUP refuses (PATCH.md). ⚠ SAT stays 0 where the Fortran
// leaves it undefined (TAIL_ID not 0 or 1); ANGLE1(1) is 0 where the Fortran leaves it undefined
// (a NaN spectrum matches no direction).
// Every libm call, asinf and acosf of APPENDTAIL included, goes through ww::dm.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "fld1_infld.hpp"
#include "fld1_sig2wn.hpp"
#include "fld1_wnd2z0m.hpp"
#include "fld2_wnd2sat.hpp"
#include "libm_device.hpp"
#include "real.hpp"
#include "st4_libmvec.hpp"

namespace ww::fld2 {

/// Bits of the status w3fld2 returns.
enum Fld2Status : int {
  kStatusBulk = 1,        ///< :573-580, "W3FLD2 failed, using bulk stress"
  kStatusNoConverge = 2,  ///< :484-490, "Stress not converged for windspeed"
  kStatusCapacity = 4     ///< the extended spectrum does not fit the work array: nothing computed
};

namespace detail {
inline constexpr Real kTpi = static_cast<Real>(2.0) * ww::kPi;   // TPI, constants.F90:73
inline constexpr Real kKappa = 0.4F;                             // KAPPA, constants.F90:70
inline constexpr Real kDwat = 1000.F;                            // DWAT
inline constexpr Real kGrav = 9.806F;                            // GRAV
// 0.132 * NU, NU = 0.105/10000.0, folded by gfortran in REAL (:450).
inline constexpr Real kNu = 0.105F / 10000.0F;
inline constexpr Real kZ02Num = 0.132F * kNu;
static_assert(kZ02Num == 1.3860000080967438407242298126220703125e-6F,
              "0.132*NU must fold to the constant the object holds");
// LOG(10./Z01) of the peeled first pass, Z01 = 0.001 (:449).
inline constexpr Real kLogTenOverMilli = 9.2103404998779296875F;

KOKKOS_INLINE_FUNCTION Real mx(Real a, Real b) { return a > b ? a : b; }
KOKKOS_INLINE_FUNCTION Real mn(Real a, Real b) { return a < b ? a : b; }

/// x**3.0 for a direction computed by a lane of _ZGVbN4vv_powf (the same argument in every lane).
KOKKOS_INLINE_FUNCTION Real pow3_mvec(Real x) {
  Real r = 0;
  ww::st4::mvec_powf(
      4, [&](int) { return x; }, [&](int) { return static_cast<Real>(3.); },
      [&](int i, Real v) {
        if (i == 0) r = v;
      });
  return r;
}
}  // namespace detail

/// The most spectral bins W3FLD2 extends the spectrum to: the Fortran appends bins until KMAX >=
/// 366 starting from SIG2WN(SIG(NK)), which is at least the deep-water value SIG**2/GRAV; half of
/// it is taken, so the bound is never short. Host only, once per grid.
inline int nkt_max(int nk, Real signk, Real xfr) {
  Real k = 0.5F * ((signk * signk) / detail::kGrav);
  const Real x2 = xfr * xfr;
  int n = nk;
  while (k < 366.F && n < 100000) {
    ++n;
    k = k * x2;
  }
  return n + 1;
}

/// Reals of work w3fld2 needs for `nktmax` bins: WN, DWN, CP, SIG2, TAUINTX, TAUINTY (6*NKT),
/// SPC2 (NKT*NTH), BT, ANG (2*NKT), NORMSPC and ANGLE1 (2*NTH).
inline int work_reals(int nth, int nktmax) { return nktmax * (8 + nth) + 2 * nth; }

/// APPENDTAIL(INSPC, WN2, NKT, KA1, KA2, KA3, WNDDIR, SAT): attach the saturation tail to INSPC
/// (NKT, NTH), column-major, in place. wn2 = WN2(1:NKT) 0-based; th(i) = TH(i+1); bt, ang (NKT),
/// normspc, angle1 (NTH) are work (the Fortran's BT, ANG, NORMSPC and ANGLE1).
template <class TH>
KOKKOS_INLINE_FUNCTION void appendtail(Real* inspc, const Real* wn2, int nkt, int ka1, int ka2,
                                       int ka3, Real wnddir, Real sat, int nth, Real dth,
                                       const TH& th, Real* bt, Real* ang, Real* normspc,
                                       Real* angle1) {
  using detail::kTpi;
  const Real kPiR = ww::kPi;
  const auto I = [&](int k, int t) -> Real& { return inspc[(k - 1) + (t - 1) * nkt]; };

  // :974-975
  bt[ka1 - 1] = 0;
  for (int k = 0; k < nkt; ++k) ang[k] = 0;
  // :976-978. WN2(KA1)**3.0 is powf; BT(KA1) accumulates in order.
  {
    const Real p3 = dm::powf(wn2[ka1 - 1], static_cast<Real>(3.));
    Real acc = 0;
    for (int t = 1; t <= nth; ++t) acc = acc + (I(ka1, t) * p3) * dth;
    bt[ka1 - 1] = acc;
  }
  // :982-998
  bt[ka2 - 1] = sat;
  Real m = ((bt[ka2 - 1] - bt[ka1 - 1])) / ((wn2[ka2 - 1] - wn2[ka1 - 1]));
  Real ic = bt[ka1 - 1] - m * wn2[ka1 - 1];
  for (int k = ka1; k <= ka2; ++k) bt[k - 1] = m * wn2[k - 1] + ic;

  // :1006-1024. MAXANG, MAI and the directions of the maximum, ANGLE1(1:MAI).
  Real maxang = 0;
  for (int t = 1; t <= nth; ++t) {
    const Real v = I(ka2, t);
    maxang = v > maxang ? v : maxang;
  }
  int mai = 0;
  for (int t = 1; t <= nth; ++t)
    if (maxang == I(ka2, t)) ++mai;
  const int nm = mai > 1 ? mai : 1;
  {
    int kk = 0;
    for (int t = 1; t <= nth; ++t)
      if (maxang == I(ka2, t)) angle1[kk++] = th(t - 1);
    if (kk == 0) angle1[0] = 0;  // ⚠ undefined in the Fortran
  }
  // :1035-1042
  for (int k = 1; k <= nm; ++k) {
    while (angle1[k - 1] < static_cast<Real>(0.)) angle1[k - 1] = angle1[k - 1] + kTpi;
    while (angle1[k - 1] >= kTpi) angle1[k - 1] = angle1[k - 1] - kTpi;
  }
  // :1043-1068
  Real angle2;
  if (mai > 1) {
    Real maxan = angle1[0];
    Real minan = angle1[0];
    for (int i = 2; i <= mai; ++i) {
      if (maxan < angle1[i - 1]) maxan = angle1[i - 1];
      if (minan > angle1[i - 1]) minan = angle1[i - 1];
    }
    if (maxan - minan > kPiR) {
      for (int i = 1; i <= mai; ++i)
        if (maxan - angle1[i - 1] > kPiR) angle1[i - 1] = angle1[i - 1] + kTpi;
    }
    Real val = 0;
    for (int i = 1; i <= mai; ++i) val = val + angle1[i - 1];
    angle2 = val / static_cast<Real>(nm);  // SUM(ANGLE1)/MAX(REAL(MAI),1.)
  } else {
    angle2 = angle1[0];
  }
  // :1070-1075
  while (angle2 < static_cast<Real>(0.)) angle2 = angle2 + kTpi;
  while (angle2 >= kTpi) angle2 = angle2 - kTpi;

  // :1080-1109. COS(ANGLE2-WNDDIR) is a lone cosf; SIN and COS of (WNDDIR-ANGLE2) are one sincosf.
  {
    const Real c0 = dm::cosf(angle2 - wnddir);
    Real s1 = 0, c1 = 0;
    dm::sincosf(wnddir - angle2, s1, c1);
    const Real dwn = ((wn2[ka3 - 1] - wn2[ka2 - 1]));
    if (c0 >= static_cast<Real>(0.)) {
      m = dm::asinf(s1) / dwn;
      ic = angle2;
      for (int k = ka2; k <= ka3; ++k) ang[k - 1] = ic + m * ((wn2[k - 1] - wn2[ka2 - 1]));
    } else if (s1 >= static_cast<Real>(0.)) {
      m = dm::acosf(c1) / dwn;
      ic = angle2;
      for (int k = ka2; k <= ka3; ++k) ang[k - 1] = ic + m * ((wn2[k - 1] - wn2[ka2 - 1]));
    } else {
      m = dm::acosf(c1) / dwn;
      ic = angle2;
      for (int k = ka2; k <= ka3; ++k) ang[k - 1] = ic - m * ((wn2[k - 1] - wn2[ka2 - 1]));
    }
  }

  // Directions computed by libmvec lanes in the loops below (gfortran vectorises them for NTH >= 4).
  const int nvec = nth > 3 ? (nth / 4) * 4 : 0;
  const Real nthr = static_cast<Real>(nth > 1 ? nth : 1);  // MAX(REAL(NTH),1.)
  const auto pow3_of = [&](Real w, Real& pv, Real& ps) {
    pv = nvec > 0 ? detail::pow3_mvec(w) : static_cast<Real>(0.);
    ps = nvec < nth ? dm::powf(w, static_cast<Real>(3.)) : static_cast<Real>(0.);
  };

  // :1111-1118. The spectrum between KA1 and KA2 is rescaled to the new level BT.
  for (int k = ka1; k <= ka2 - 1; ++k) {
    Real val = 0;
    for (int t = 1; t <= nth; ++t) val = val + I(k, t);
    const Real avg = val / nthr;
    if (avg != static_cast<Real>(0.)) {
      Real pv, ps;
      pow3_of(wn2[k - 1], pv, ps);
      for (int t = 1; t <= nth; ++t)
        I(k, t) = (((bt[k - 1] * I(k, t)) / kTpi) / (t <= nvec ? pv : ps)) / avg;
    } else {
      for (int t = 1; t <= nth; ++t) I(k, t) = 0;
    }
  }
  // :1125-1140. KA2 to KA3: the level SAT, a cos**2 distribution about the direction ANG(K).
  for (int k = ka2; k <= ka3; ++k) {
    for (int t = 1; t <= nth; ++t) {
      const Real c = dm::cosf(th(t - 1) - ang[k - 1]);
      normspc[t - 1] = c > static_cast<Real>(0.) ? c * c : static_cast<Real>(0.);
    }
    Real val = 0;
    for (int t = 1; t <= nth; ++t) val = val + normspc[t - 1];
    const Real avg = val / nthr;
    if (avg != static_cast<Real>(0.)) {
      Real pv, ps;
      pow3_of(wn2[k - 1], pv, ps);
      for (int t = 1; t <= nth; ++t)
        I(k, t) = (((sat * normspc[t - 1]) / kTpi) / (t <= nvec ? pv : ps)) / avg;
    } else {
      for (int t = 1; t <= nth; ++t) I(k, t) = 0;
    }
  }
  // :1143-1157. Beyond KA3: the same distribution about the wind direction.
  for (int t = 1; t <= nth; ++t) {
    const Real c = dm::cosf(th(t - 1) - wnddir);
    normspc[t - 1] = c > static_cast<Real>(0.) ? c * c : static_cast<Real>(0.);
  }
  {
    Real val = 0;
    for (int t = 1; t <= nth; ++t) val = val + normspc[t - 1];
    const Real avg = val / nthr;
    for (int k = ka3 + 1; k <= nkt; ++k) {
      if (avg != static_cast<Real>(0.)) {
        Real pv, ps;
        pow3_of(wn2[k - 1], pv, ps);
        for (int t = 1; t <= nth; ++t)
          I(k, t) = (((normspc[t - 1] * sat) / kTpi) / (t <= nvec ? pv : ps)) / avg;
      } else {
        for (int t = 1; t <= nth; ++t) I(k, t) = 0;
      }
    }
  }
}

/// W3FLD2(ASPC, FPI, WNDX, WNDY, ZWND, DEPTH, RIB, DAIR, UST, USTD, Z0, TAUNUX, TAUNUY, CHARN)
/// for one point. aspc = ASPC(1:NSPEC); fpi, wndx, wndy, zwnd, depth, dair by value (RIB is 0.
/// at the only call, W3SRCE :2295, and is not read); sig = SIG(1:NK) 0-based; th(i) = TH(i+1);
/// the tail parameters are what INFLD copies. ust, ustd and charn are written (charn always, as
/// W3SRCE passes it). work has work_reals(nth, nktmax) reals. Returns the Fld2Status bits.
template <class TH>
KOKKOS_INLINE_FUNCTION int w3fld2(const Real* aspc, Real fpi, Real wndx, Real wndy, Real zwnd,
                                  Real depth, Real dair, int nk, int nth, Real xfr, Real dth,
                                  const Real* sig, const TH& th, int tail_id, Real tail_lev,
                                  Real tail_tran1, Real tail_tran2, Real& ust, Real& ustd,
                                  Real& charn, Real* work, int nktmax) {
  using detail::kDwat;
  using detail::kKappa;
  using detail::kTpi;
  using detail::mn;
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.);
  int status = 0;

  // :231-234 FIRST/INFLD
  ww::fld1::TailState tail;
  ww::fld1::infld(tail_id, tail_lev, tail_tran1, tail_tran2, tail);

  // :239-240
  const Real uref = Kokkos::sqrt(wndx * wndx + wndy * wndy);
  const Real urefd = dm::atan2f(wndy, wndx);
  // :250
  Real z01;
  ww::fld1::wnd2z0m(uref, z01);
  const Real wnd_10_mag = uref;
  // :276-280. SAT stays 0 for another TAIL_ID, which the Fortran leaves undefined.
  Real sat = zero;
  if (tail.tail_choice == 0) {
    sat = tail.tail_level;
  } else if (tail.tail_choice == 1) {
    ww::fld2::wnd2sat(wnd_10_mag, sat);
  }
  // :286-290. UREF*KAPPA is one product in the object.
  const Real ukap = uref * kKappa;
  Real ustra = ukap / dm::logf(zwnd / z01);
  ustd = urefd;
  Real cu = 0, su = 0;
  dm::sincosf(urefd, su, cu);  // COS and SIN of WND_10_DIR (= UREFD): one sincosf

  // :294-299
  Real kmax;
  ww::fld1::sig2wn(sig[nk - 1], depth, kmax);
  int nkt = nk;
  const Real xfr2 = xfr * xfr;
  while (kmax < 366.F) {
    nkt = nkt + 1;
    kmax = ((kmax * xfr2));
  }
  if (nkt > nktmax || nkt < 6) {
    ust = zero;
    ustd = urefd;
    charn = zero;
    return kStatusCapacity;
  }
  Real* const wn = work;
  Real* const dwn = wn + nkt;
  Real* const cp = dwn + nkt;
  Real* const sig2 = cp + nkt;
  Real* const tauintx = sig2 + nkt;
  Real* const tauinty = tauintx + nkt;
  Real* const spc2 = tauinty + nkt;  // SPC2(NKT,NTH)
  Real* const bt = spc2 + nkt * nth;
  Real* const ang = bt + nkt;
  Real* const normspc = ang + nkt;
  Real* const angle1 = normspc + nth;
  const auto S = [&](int k, int t) -> Real& { return spc2[(k - 1) + (t - 1) * nkt]; };

  // :305-313
  for (int k = 1; k <= nk; ++k) {
    ww::fld1::sig2wn(sig[k - 1], depth, wn[k - 1]);
    cp[k - 1] = sig[k - 1] / wn[k - 1];
    sig2[k - 1] = sig[k - 1];
  }
  for (int k = nk + 1; k <= nkt; ++k) {
    sig2[k - 1] = sig2[k - 2] * xfr;
    ww::fld1::sig2wn(sig2[k - 1], depth, wn[k - 1]);
    cp[k - 1] = sig2[k - 1] / wn[k - 1];
  }
  // :315-319. /2.0 is *0.5; XFR**2.0 is XFR*XFR.
  for (int k = 2; k <= nkt - 1; ++k) dwn[k - 1] = ((wn[k] - wn[k - 2])) * 0.5F;
  dwn[0] = ((wn[1] - ((wn[0] / xfr2)))) * 0.5F;
  dwn[nkt - 1] = ((xfr2 * wn[nkt - 1] - wn[nkt - 2])) * 0.5F;
  // :323-334. WN**3.0 is powf, one call per bin.
  {
    int count = 0;
    for (int k = 1; k <= nk; ++k)
      for (int t = 1; t <= nth; ++t) {
        ++count;
        S(k, t) = aspc[count - 1] * sig[k - 1];
      }
    for (int k = nk + 1; k <= nkt; ++k) {
      const Real pnk = dm::powf(wn[nk - 1], static_cast<Real>(3.));
      const Real pk = dm::powf(wn[k - 1], static_cast<Real>(3.));
      for (int t = 1; t <= nth; ++t) S(k, t) = (S(nk, t) * pnk) / pk;
    }
  }
  // :339-343. FPI*TPI is one product.
  Real ktaila, ktailb;
  const Real fpitpi = fpi * kTpi;
  ww::fld1::sig2wn(fpitpi * tail.tail_transition_ratio1, depth, ktaila);
  ww::fld1::sig2wn(fpitpi * tail.tail_transition_ratio2, depth, ktailb);
  const Real ktailc = ktailb * 2.0F;
  // :344-354
  int ka1 = 2;
  while (ktaila >= wn[ka1 - 1] && ka1 < nkt - 6) ka1 = ka1 + 1;
  int ka2 = ka1 + 2;
  while (ktailb >= wn[ka2 - 1] && ka2 < nkt - 4) ka2 = ka2 + 1;
  int ka3 = ka2 + 2;
  while (ktailc >= wn[ka3 - 1] && ka3 < nkt - 2) ka3 = ka3 + 1;
  // :356, WNDDIR = ATAN2(WNDY,WNDX), the value of UREFD
  appendtail(spc2, wn, nkt, ka1, ka2, ka3, urefd, sat, nth, dth, th, bt, ang, normspc, angle1);

  // :368-565 with HEIGHTFLG false: ITTOT = 1, one pass of the wind iteration.
  bool no_err = true;
  bool flg1 = true, flg2 = true;
  int its = 1;
  Real a1 = zero;
  const Real uref2 = wnd_10_mag * wnd_10_mag;  // WND_10_MAG**2.0
  Real taunux = zero, taunuy = zero, taux = zero, tauy = zero, ustrb = zero;
  while ((flg1 && flg2) && no_err) {
    // :390. Z0 is also an output of W3FLD2, which W3SRCE does not read after :2295.
    const Real z0 = 10.F / ((dm::expf(ukap / ustra)));
    for (int k = 0; k < nkt; ++k) {
      tauintx[k] = zero;
      tauinty[k] = zero;
    }
    // :393-424
    for (int k = 1; k <= nkt; ++k) {
      const Real wnd_z = mn(ww::kPi / wn[k - 1], 20.F);
      const Real wnd_z_mag = ((ustra / kKappa)) * ((dm::logf(wnd_z / z0)));
      Real acc_x = tauintx[k - 1];
      Real acc_y = tauinty[k - 1];
      for (int t = 1; t <= nth; ++t) {
        const Real wnd_z_proj = wnd_z_mag * dm::cosf(urefd - th(t - 1));
        if (wnd_z_proj > cp[k - 1]) {
          a1 = 0.11F;
        } else if (wnd_z_proj >= zero && wnd_z_proj <= cp[k - 1]) {
          a1 = 0.01F;
        } else if (wnd_z_proj < zero) {
          a1 = 0.1F;
        }
        const Real wnd_effect = wnd_z_proj - cp[k - 1];
        const Real scin =
            (((((a1 * wnd_effect) * Kokkos::fabs(wnd_effect)) * dair) / kDwat) * wn[k - 1]) /
            cp[k - 1];
        Real sn = 0, cs = 0;
        dm::sincosf(th(t - 1), sn, cs);
        acc_x = acc_x + (((((S(k, t) * scin) * cs) * sig2[k - 1]) * dth));
        acc_y = acc_y + (((((S(k, t) * scin) * sn) * sig2[k - 1]) * dth));
      }
      tauintx[k - 1] = acc_x;
      tauinty[k - 1] = acc_y;
    }
    // :426-434
    Real tauyw = zero, tauxw = zero;
    for (int k = 1; k <= nkt; ++k) {
      tauxw = tauxw + (kDwat * dwn[k - 1]) * tauintx[k - 1];
      tauyw = tauyw + (kDwat * dwn[k - 1]) * tauinty[k - 1];
    }
    const Real cdf = ((Kokkos::sqrt(tauyw * tauyw + tauxw * tauxw) / dair)) / uref2;
    // :438-458
    Real ustsm;
    bool iterflag;
    Real z02 = zero;
    if (uref < 0.01F) {
      ustsm = zero;
      iterflag = false;
    } else {
      z02 = 0.001F;
      iterflag = true;
    }
    int count = 1;
    bool first = true;
    while (iterflag && count < 10) {
      const Real zz1 = z02;
      const Real lg = first ? detail::kLogTenOverMilli : ((dm::logf(10.F / zz1)));
      first = false;
      ustsm = ukap / lg;
      z02 = detail::kZ02Num / ustsm;
      iterflag = !(Kokkos::fabs(z02 / zz1 - one) < 9.99999974737875163555145263671875e-5F);
      count = count + 1;
    }
    Real cds = (ustsm * ustsm) / uref2;
    // :460
    cds = (cds / 3.F) * (((cds * 2.F) / ((cdf + cds))) + one);
    // :462-466
    taunux = ((dair * cds) * uref2) * cu;
    taunuy = ((dair * cds) * uref2) * su;
    taux = taunux + tauxw;
    tauy = taunuy + tauyw;
    ustrb = Kokkos::sqrt(Kokkos::sqrt(tauy * tauy + taux * taux) / dair);
    ustd = dm::atan2f(tauy, taux);
    const Real b1 = ((ustra - ustrb));
    const Real b2 = ((ustrb + ustra)) * 0.5F;
    its = its + 1;
    const Real ratio = Kokkos::fabs((b1 * 100.F) / b2);
    flg1 = ratio >= 9.99999977648258209228515625e-3F;
    flg2 = its < 20;
    if (flg1 && flg2) {
      ustra = (ustrb * 0.5F) * 2.F;  // USTRB*.5 + USTRB*.5 as the object computes it
    } else if (ratio >= 5.F) {
      flg1 = false;
      flg2 = false;
      no_err = false;
      status |= kStatusNoConverge;
    }
  }
  // :567-571
  ust = ustrb;
  ustd = dm::atan2f(tauy, taux);
  const Real ust2 = ust * ust;
  const Real cd = ust2 / uref2;
  charn = 0.01F / Kokkos::sqrt(Kokkos::sqrt(taunux * taunux + taunuy * taunuy) / ((ust2)));
  // :573-581, the fail-safe to the bulk stress
  if (!(cd < 0.01F && cd > 5.00000023748725652694702148437500e-4F) || !no_err) {
    status |= kStatusBulk;
    Real z0b;
    ww::fld1::wnd2z0m(uref, z0b);
    ust = ukap / dm::logf(zwnd / z0b);
    ustd = urefd;
  }
  return status;
}

}  // namespace ww::fld2
