// kokkos/src/ww_kokkos/snl5_calc_wbtv2.hpp
// CALC_WBTv2 of W3SNL5MD: the dominant wave breaking probability b_T of the wind-sea
// part of a spectrum, after Babanin et al. (2001), which sets NL5's phase-mixing interval.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3snl5md.F90:560-711 at NOAA-EMC/WW3 03adda91;
// author Q. Liu. Evidence: kokkos/port_report/CALC_WBTV2_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "prop_wavnu1.hpp"
#include "real.hpp"
#include "st4_libmvec.hpp"

namespace ww::snl5 {

/// CALC_WBTv2(A, CG, WN, DPT, U10, UDIR), :560-711. Called by W3SNL5 (:283) only when
/// QI5PMX < 0. Array bases, all 0-based here:
///   a      A(NTH,NK), column-major: A(ith,ik) at a[(ik-1)*max(nth,0) + ith-1]
///   cg, wn CG(1:NK), WN(1:NK)
///   sig    SIG(1:NK) of W3GDATMD (allocated SIG(0:NK+1)); dsii DSII(1:NK)
///   ecos, esin  ECOS(1:NTH), ESIN(1:NTH); dth DTH
///   disp   W3DISPMD's DISTAB table, read by WAVNU1 (:682)
///   esig   caller-provided work of NK floats, the automatic ESIG(NK) of :623
/// W3_S (STRACE) is not translated; no other #ifdef in the routine.
KOKKOS_INLINE_FUNCTION Real calc_wbtv2(const Real* a, const Real* cg, const Real* wn, Real dpt,
                                       Real u10, Real udir, int nk, int nth, const Real* sig,
                                       const Real* esin, const Real* ecos, Real dth,
                                       const Real* dsii, const ww::prop::DispTable& disp,
                                       Real* esig) {
  const Real beta = static_cast<Real>(1.2);  // :619, the REAL literal 1.2 (0x3F99999A)
  const long stride = nth > 0 ? static_cast<long>(nth) : 0L;

  // :646-647; gfortran fuses the pair into one sincosf.
  const Real sinu = Kokkos::sin(udir);
  const Real cosu = Kokkos::cos(udir);

  for (int ik = 0; ik < nk; ++ik) esig[ik] = static_cast<Real>(0.);  // :649
  Real etp = static_cast<Real>(0.);                                   // :651

  for (int ik = 0; ik < nk; ++ik) {                         // :653
    const Real tc = sig[ik] / wn[ik];                       // :654
    const Real factor = dth * (sig[ik] / cg[ik]);           // :655-656
    for (int ith = 0; ith < nth; ++ith) {                   // :658
      // :659-660, ((u10*(cosu*ecos+sinu*esin))*beta): the parenthesised sum first.
      const Real tforce = tc - u10 * (cosu * ecos[ith] + sinu * esin[ith]) * beta;
      if (tforce < static_cast<Real>(0.)) {                  // :662
        esig[ik] = esig[ik] + a[ik * stride + ith] * factor;  // :663
      }
    }
  }

  // :672, SUM(ESIG*DSII): the object adds in index order (vectorised products, an
  // in-order fold), so a scalar loop in the same order gives the same bits.
  Real et = static_cast<Real>(0.);
  for (int ik = 0; ik < nk; ++ik) et = et + dsii[ik] * esig[ik];
  // :673, MAX(0.,ET) is maxss with ET as destination: a NaN ET gives 0.
  const Real hs = static_cast<Real>(4.) *
                  Kokkos::sqrt(et > static_cast<Real>(0.) ? et : static_cast<Real>(0.));

  // :677-678. ESIG**4. with a REAL exponent is powf; the object computes it by fours
  // through _ZGVbN4vv_powf and powf for the last NK mod 4, in each of the two SUMs.
  const auto x = [=](int i) { return esig[i]; };
  const auto four = [](int) { return static_cast<Real>(4.); };
  Real num = static_cast<Real>(0.);
  ww::st4::mvec_powf(nk, x, four, [&](int i, Real p) { num = num + dsii[i] * (sig[i] * p); });
  Real den = static_cast<Real>(0.);
  ww::st4::mvec_powf(nk, x, four, [&](int i, Real p) { den = den + dsii[i] * p; });
  // MAX(1E-10, SUM) is maxss with the SUM as destination: a NaN sum gives 1E-10.
  const Real eps = static_cast<Real>(1E-10);
  Real sigp = num / (den > eps ? den : eps);
  // :679. With NK <= 0 the Fortran reads SIG(NK), here sig[nk-1], i.e. SIG(0) for NK = 0.
  if (Kokkos::fabs(sigp) < static_cast<Real>(1E-7)) sigp = sig[nk - 1];

  Real kp = 0;
  Real cgp = 0;
  ww::prop::wavnu1(disp, sigp, dpt, kp, cgp);  // :682, out of line in the object

  const Real lo = sigp * static_cast<Real>(0.7);  // :689
  for (int ik = 0; ik < nk; ++ik) {               // :688
    if (sig[ik] >= lo && sig[ik] <= sigp * static_cast<Real>(1.3)) {
      etp = etp + esig[ik] * dsii[ik];  // :691
    }
  }
  const Real hsp = static_cast<Real>(4.) *
                   Kokkos::sqrt(etp > static_cast<Real>(0.) ? etp : static_cast<Real>(0.));  // :694

  const Real wstp = static_cast<Real>(0.5) * kp * hsp;  // :698
  // :702, (MAX(0.,WSTP-0.055)*(1+HS/DPT))**2.33 with maxss destination WSTP-0.055.
  const Real d = wstp - static_cast<Real>(0.055);
  const Real m = d > static_cast<Real>(0.) ? d : static_cast<Real>(0.);
  Real twbt = static_cast<Real>(85.1) *
              Kokkos::pow((static_cast<Real>(1.) + hs / dpt) * m, static_cast<Real>(2.33));
  // :703, MIN(1.0,TWBT) is minss with TWBT as destination: a NaN TWBT gives 1.0.
  twbt = twbt < static_cast<Real>(1.) ? twbt : static_cast<Real>(1.);
  return twbt;
}

}  // namespace ww::snl5
