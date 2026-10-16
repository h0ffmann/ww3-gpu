// kokkos/src/ww_kokkos/srce_st6_lfactor.hpp
// LFACTOR: the reduction factor of the ST6 wind input that keeps the wave-supported
// stress within the total stress (Rogers et al. 2012, doi:10.1175/JTECH-D-11-00092.1),
// its stresses and the Charnock parameter, one spectral point; lfactor_tables(), the host
// build of its SAVE arrays IK10Hz, SIG10Hz, DSII10Hz and MK10Hz.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src6md.F90:805-1135 at NOAA-EMC/WW3 03adda91 (SAVE
// arrays :103-108); authors S. Zieger, Q. Liu.
// Evidence: kokkos/port_report/LFACTOR_KOKKOS.md.
// The ALLOCATEs of :972-977 become `work`; the SAVE arrays allocated and filled on the
// first call (:955-969, :995-1001, :1010-1013) become lfactor_tables(), which the caller
// runs once with the first call's SIG and DSII. STME21 (:1047) and every use of its
// IDTIME and of TAU_NND (:1036, :1067) only feed WRITEs to NDST, and are dropped.
// ⚠ On a device expf, logf, tanhf, sincosf and sqrtf are CUDA's, and libmvec does not
// exist: the EXP loop (:1065) cannot be bit for bit there.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"
#include "srce_st6_util.hpp"
#include "st4_libmvec.hpp"
#include "st4_params.hpp"

namespace ww::srce::st6 {

/// Return codes of lfactor() and lfactor_tables().
inline constexpr int kLfactorItermax = 1;  ///< IK >= ITERMAX: WRITE(NDST,280) (:1093-1094)
inline constexpr int kLfactorExtcde2 = 2;  ///< EXTCDE(2), SIZE(IK10Hz) /= NK10Hz (:958-961)
inline constexpr int kLfactorExtcde3 = 3;  ///< EXTCDE(3), MK10Hz /= NK10Hz (:983-986)

namespace lfactor_detail {
// minss/maxss of w3src6md.o, d the destination: a NaN in either operand or a tie gives s.
// Every MIN/MAX of LFACTOR puts the variable operand in the destination and the
// constant in s (:993, :994, :1016, :1017, :1026, :1065, :1086, :1108); :1028 puts
// TAU_VIS in d and 0.95*TAU_TOT in s. Vector bodies and scalar tails agree.
KOKKOS_INLINE_FUNCTION Real minss(Real d, Real s) { return (d < s) ? d : s; }
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }

// TPI (constants.F90:72) as the object divides by it at :952, 0x40c90fdb.
inline constexpr Real kTpi = 0x1.921fb6p+2f;
static_assert(kTpi == ww::srce::kTpi);

inline constexpr Real kFrqMax = static_cast<Real>(10.);  // FRQMAX :927
inline constexpr int kItermax = 80;                      // ITERMAX :928

// REAL**INTEGER with a run-time exponent (:1088): the object calls libgcc's __powisf2.
// The device branch is __powisf2's algorithm (libgcc2.c), operation for operation.
KOKKOS_INLINE_FUNCTION Real powi(Real x, int m) {
  KOKKOS_IF_ON_HOST((return __builtin_powif(x, m);))
  KOKKOS_IF_ON_DEVICE((
      unsigned n = m < 0 ? 0u - static_cast<unsigned>(m) : static_cast<unsigned>(m);
      Real y = (n % 2u) ? x : static_cast<Real>(1);
      while (n >>= 1) {
        x = x * x;
        if (n % 2u) y = y * x;
      }
      return m < 0 ? static_cast<Real>(1) / y : y;))
}

// TAUWINDS(SD*LF,CINV,DSII) (:1067-1069, inlined from :1467): the product SD*LF is
// rounded per element as the Fortran temporary holds it, then ((sd*lf)*c)*d is added in
// index order from 0.0, then one multiplication by GRAV*DWAT. Same sum as tauwinds().
KOKKOS_INLINE_FUNCTION Real tauwinds_lf(const Real* sd, const Real* lf, const Real* cinv,
                                        const Real* dsii, int n) {
  Real sum = static_cast<Real>(0.);
  for (int i = 0; i < n; ++i) {
    const Real sdlf = sd[i] * lf[i];
    sum = sum + (sdlf * cinv[i]) * dsii[i];
  }
  return sum * util_detail::kGravDwat;
}
}  // namespace lfactor_detail

/// NK10Hz (:952-953) = MAX(NK, CEILING(ALOG(FRQMAX/(SIG(1)/TPI))/ALOG(XFR))+1). CEILING
/// is gfortran's: i = INT(q) by cvttss2si, then i if REAL(i) >= q, else i+1. sig1 is
/// SIG(1). The size of every 10 Hz array.
KOKKOS_INLINE_FUNCTION int lfactor_nk10hz(int nk, Real sig1, Real xfr) {
  const Real q = Kokkos::log(lfactor_detail::kFrqMax / (sig1 / lfactor_detail::kTpi)) /
                 Kokkos::log(xfr);
  const int i = ww::st4::fint(q);
  const int nk10hz = (static_cast<Real>(i) >= q) ? i + 1 : i + 2;
  return nk10hz > nk ? nk10hz : nk;
}

/// The first call of LFACTOR on the host: IK10Hz = RRANGE(1,NK10Hz,1), MK10Hz = NK10Hz
/// (:955-962), and SIG10Hz, DSII10Hz (:996-1000 when NK < NK10Hz, else :1011-1012).
/// sig, dsii are LFACTOR's SIG(1:NK), DSII(1:NK) of that call; xfr is W3GDATMD XFR.
/// ik10hz holds range_size(1,nk10hz,1) elements, sig10hz and dsii10hz nk10hz, where
/// nk10hz = lfactor_nk10hz(nk, sig[0], xfr). Returns 0 or kLfactorExtcde2.
inline int lfactor_tables(int nk, Real xfr, const Real* sig, const Real* dsii, int& mk10hz,
                          Real* ik10hz, Real* sig10hz, Real* dsii10hz) {
  const int nk10hz = lfactor_nk10hz(nk, sig[0], xfr);
  const int nik = rrange(1, nk10hz, 1, ik10hz);
  mk10hz = nk10hz;
  if (nik != nk10hz) return kLfactorExtcde2;
  if (nk < nk10hz) {
    // :996, groups of four through _ZGVbN4vv_powf from the first element, then powf.
    const Real sig1 = sig[0];
    ww::st4::mvec_powf(
        nk10hz, KOKKOS_LAMBDA(int) { return xfr; },
        KOKKOS_LAMBDA(int i) { return ik10hz[i] - static_cast<Real>(1.0); },
        KOKKOS_LAMBDA(int i, Real v) { sig10hz[i] = sig1 * v; });
    const Real c = xfr - static_cast<Real>(1.0) / xfr;  // :997
    for (int i = 0; i < nk10hz; ++i) dsii10hz[i] = (static_cast<Real>(0.5) * sig10hz[i]) * c;
    dsii10hz[0] = (static_cast<Real>(0.5) * sig10hz[0]) * (xfr - static_cast<Real>(1.0));  // :999
    dsii10hz[nk10hz - 1] = (static_cast<Real>(0.5) * sig10hz[nk10hz - 1]) *
                           (xfr - static_cast<Real>(1.0)) / xfr;  // :1000
  } else {
    for (int i = 0; i < nk; ++i) {  // :1011-1012, NK10Hz = NK here
      sig10hz[i] = sig[i];
      dsii10hz[i] = dsii[i];
    }
  }
  return 0;
}

/// LFACTOR (w3src6md.F90:805-1135) for one point, arrays 0-based. s is S(NTH,NK) in
/// column-major order (the SDENSIG of W3SIN6, :505); cinv, sig, dsii hold NK elements
/// (dsii is read only by the first call, through lfactor_tables()); lfact receives NK.
/// ecos, esin start at ECOS(1), ESIN(1) and are read over NSPEC (:979-980). sin6* are
/// W3GDATMD SIN6WS, SIN6CHKMIN, SIN6FLCAP, SIN6CHKCAP, SIN6CHKINF, SIN6CHKSIG.
/// mk10hz, sig10hz, dsii10hz come from lfactor_tables(). work holds 6*NK10Hz elements
/// (LF10Hz, CINV10Hz, SDENS10Hz, SDENSX10Hz, SDENSY10Hz, UCINV10Hz of :972-977).
/// Returns 0, kLfactorItermax (outputs valid; the Fortran writes a warning and goes on),
/// or kLfactorExtcde3 (the Fortran stops; outputs not written).
KOKKOS_INLINE_FUNCTION int lfactor(const Real* s, const Real* cinv, Real u10, Real ustar,
                                   Real usdir, Real dair, const Real* sig, const Real* dsii,
                                   Real* lfact, Real& tauwx, Real& tauwy, Real& charn,
                                   const Params& p, Real dth, Real xfr, const Real* ecos,
                                   const Real* esin, Real sin6ws, Real sin6chkmin,
                                   bool sin6flcap, Real sin6chkcap, Real sin6chkinf,
                                   Real sin6chksig, int mk10hz, const Real* sig10hz,
                                   const Real* dsii10hz, Real* work) {
  using lfactor_detail::maxss;
  using lfactor_detail::minss;
  const int nk = p.nk;
  const int nth = p.nth;
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.0);

  const int nk10hz = lfactor_nk10hz(nk, sig[0], xfr);  // :952-953
  if (mk10hz != nk10hz) return kLfactorExtcde3;        // :983-986

  Real* lf10hz = work;
  Real* cinv10hz = work + nk10hz;
  Real* sdens10hz = work + 2 * nk10hz;
  Real* sdensx10hz = work + 3 * nk10hz;
  Real* sdensy10hz = work + 4 * nk10hz;
  Real* ucinv10hz = work + 5 * nk10hz;

  // :992-994 and :1015-1017: SUM(...,1) adds over ITH in order from 0.0, then * DTH.
  // RESHAPE(ECOS2,(/NTH,NK/))(ITH,IK) is ECOS(ITH+(IK-1)*NTH).
  for (int ik = 0; ik < nk; ++ik) {
    Real v = zero;
    for (int ith = 0; ith < nth; ++ith) v = v + s[ith + ik * nth];
    sdens10hz[ik] = v * dth;
  }
  for (int ik = 0; ik < nk; ++ik) {
    Real v = zero;
    for (int ith = 0; ith < nth; ++ith) {
      const int is = ith + ik * nth;
      v = v + maxss(s[is], zero) * ecos[is];
    }
    sdensx10hz[ik] = v * dth;
  }
  for (int ik = 0; ik < nk; ++ik) {
    Real v = zero;
    for (int ith = 0; ith < nth; ++ith) {
      const int is = ith + ik * nth;
      v = v + maxss(s[is], zero) * esin[is];
    }
    sdensy10hz[ik] = v * dth;
  }
  for (int ik = 0; ik < nk; ++ik) cinv10hz[ik] = cinv[ik];  // :1003, :1014
  if (nk < nk10hz) {
    const Real sig_nk = sig10hz[nk - 1];
    for (int ik = nk; ik < nk10hz; ++ik)
      cinv10hz[ik] = sig10hz[ik] * static_cast<Real>(0.101978);  // :1004, 1/c = sigma/grav
    // :1006-1008, spectral slope F**(-2): SD(NK) * (SIG10Hz(NK)/SIG10Hz(IK))**2
    const Real sd = sdens10hz[nk - 1];
    for (int ik = nk; ik < nk10hz; ++ik) {
      const Real r = sig_nk / sig10hz[ik];
      sdens10hz[ik] = sd * (r * r);
    }
    const Real sdx = sdensx10hz[nk - 1];
    for (int ik = nk; ik < nk10hz; ++ik) {
      const Real r = sig_nk / sig10hz[ik];
      sdensx10hz[ik] = sdx * (r * r);
    }
    const Real sdy = sdensy10hz[nk - 1];
    for (int ik = nk; ik < nk10hz; ++ik) {
      const Real r = sig_nk / sig10hz[ik];
      sdensy10hz[ik] = sdy * (r * r);
    }
  }

  // :1022-1031
  const Real tau_tot = (ustar * ustar) * dair;
  Real tau_vis = maxss(-static_cast<Real>(5.0E-5) * u10 + static_cast<Real>(1.1E-3), zero) *
                 (u10 * u10) * dair;
  tau_vis = minss(tau_vis, static_cast<Real>(0.95) * tau_tot);
  const Real tauvx = tau_vis * Kokkos::cos(usdir);  // one sincosf in the object
  const Real tauvy = tau_vis * Kokkos::sin(usdir);

  // :1034-1044; TAU_NND (:1036) and TAU_INIT (:1038) feed only WRITEs.
  tauwx = tauwinds(sdensx10hz, cinv10hz, dsii10hz, nk10hz);
  tauwy = tauwinds(sdensy10hz, cinv10hz, dsii10hz, nk10hz);
  Real tau_wav = Kokkos::sqrt(tauwx * tauwx + tauwy * tauwy);
  Real taux = tauvx + tauwx;
  Real tauy = tauvy + tauwy;
  Real tau = Kokkos::sqrt(taux * taux + tauy * tauy);

  for (int ik = 0; ik < nk10hz; ++ik) lf10hz[ik] = one;  // :1048
  int status = 0;

  if (tau > tau_tot) {  // :1051
    Real err = (tau - tau_tot) / tau_tot;  // :1043
    bool overshot = false;
    Real rtau = err / static_cast<Real>(90.);
    Real drtau = static_cast<Real>(2.0);
    int sign_new = static_cast<int>(Kokkos::copysign(one, err));  // INT(SIGN(1.0,ERR)), +-1
    const Real uproxy = sin6ws * ustar;
    for (int ik = 0; ik < nk10hz; ++ik) ucinv10hz[ik] = one - (uproxy * cinv10hz[ik]);

    int ik = 1;
    for (; ik <= lfactor_detail::kItermax; ++ik) {
      // :1065, groups of four through _ZGVbN4v_expf from the first element, then expf.
      const Real rt = rtau;
      ww::st4::mvec_expf(
          nk10hz, [=](int i) { return ucinv10hz[i] * rt; },
          [=](int i, Real v) { lf10hz[i] = minss(v, one); });
      tauwx = lfactor_detail::tauwinds_lf(sdensx10hz, lf10hz, cinv10hz, dsii10hz, nk10hz);
      tauwy = lfactor_detail::tauwinds_lf(sdensy10hz, lf10hz, cinv10hz, dsii10hz, nk10hz);
      tau_wav = Kokkos::sqrt(tauwx * tauwx + tauwy * tauwy);
      taux = tauvx + tauwx;
      tauy = tauvy + tauwy;
      tau = Kokkos::sqrt(taux * taux + tauy * tauy);
      err = (tau - tau_tot) / tau_tot;
      const int sign_old = sign_new;
      sign_new = static_cast<int>(Kokkos::copysign(one, err));
      if (sign_new != sign_old) overshot = true;  // :1085-1086
      if (overshot)
        drtau = maxss(static_cast<Real>(0.5) * (one + drtau), static_cast<Real>(1.00010));
      rtau = rtau * lfactor_detail::powi(drtau, sign_new);  // :1088
      if (Kokkos::fabs(err) < static_cast<Real>(1.54E-4)) break;
    }
    if (ik >= lfactor_detail::kItermax) status = kLfactorItermax;  // :1093
  }

  for (int i = 0; i < nk; ++i) lfact[i] = lf10hz[i];  // :1097

  // :1101-1108
  Real chkmin = sin6chkmin;
  if (sin6flcap) {
    chkmin = sin6chkinf + static_cast<Real>(0.5) * (sin6chkmin - sin6chkinf) *
                              (one - Kokkos::tanh((u10 - sin6chkcap) / sin6chksig));
  }
  charn = chkmin / Kokkos::sqrt(one - minss(tau_wav / tau_tot, static_cast<Real>(0.99)));
  return status;
}

}  // namespace ww::srce::st6
