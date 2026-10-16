// kokkos/src/ww_kokkos/srce_st6_sin6.hpp
// W3SIN6: observation-based wind input of ST6 (Donelan et al. 2006, doi:10.1175/JPO2933.1,
// as implemented by Rogers et al. 2012, doi:10.1175/JTECH-D-11-00092.1), with the stress
// reduction of LFACTOR and the negative stress of TAU_WAVE_ATMOS, one spectral point.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src6md.F90:298-539 at NOAA-EMC/WW3 03adda91 (SAVE
// array IKN :104); authors S. Zieger, Q. Liu. Evidence: kokkos/port_report/W3SIN6_KOKKOS.md.
// The module SAVE index vector IKN, built by the first call of W3SIN6 (:449-456) or
// W3SDS6 (:696-704), is the host table of w3sds6_ikn(); the automatic arrays of :412-417
// and the callees' ALLOCATEs are `work`. EXTCDE becomes a return code.
// ⚠ On a device tanhf, sincosf and sqrtf are CUDA's, and libmvec does not exist: the
// TANH loops (:497-498, :524-525) cannot be bit for bit there.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"
#include "srce_st6_lfactor.hpp"
#include "srce_st6_sds6.hpp"
#include "srce_st6_tau_wave_atmos.hpp"

#if defined(__x86_64__) && defined(__SSE2__)
#include <xmmintrin.h>
extern "C" __m128 _ZGVbN4v_tanhf(__m128);
#define WW_ST6_HAVE_LIBMVEC_TANHF 1
#endif

namespace ww::srce::st6 {

/// Return code of w3sin6_point(): (LFACTOR's code) | (TAU_WAVE_ATMOS's code << 4).
/// LFACTOR's is 0, kLfactorItermax (the Fortran writes a warning and goes on) or
/// kLfactorExtcde3 (the Fortran stops in LFACTOR; tauwx, tauwy, charn, taunwx, taunwy
/// and the reduction not applied). TAU_WAVE_ATMOS's is 0 or 3 (EXTCDE(3) there, :1274-1277;
/// taunwx, taunwy stay 0).
inline constexpr int kSin6TauShift = 4;

namespace sin6_detail {
// maxss/minss of w3src6md.o, d the destination: a NaN in either operand or a tie gives s.
// :495 and :523 put the variable in d and 0.0 in s, in the vector bodies and the tail.
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }
KOKKOS_INLINE_FUNCTION Real minss(Real d, Real s) { return (d < s) ? d : s; }

// 2.8 (:498, :525) as w3src6md.o loads it, 0x40333333.
inline constexpr Real k28 = 0x1.666666p+1f;
static_assert(k28 == 2.8f);

// libgfortran 16.2's _gfortran_maxval_r4 (:474): -HUGE for an empty column, a quiet NaN
// when every element is NaN.
KOKKOS_INLINE_FUNCTION Real neg_huge() { return Kokkos::bit_cast<Real>(std::uint32_t{0xff7fffffu}); }
KOKKOS_INLINE_FUNCTION Real neg_inf() { return Kokkos::bit_cast<Real>(std::uint32_t{0xff800000u}); }
KOKKOS_INLINE_FUNCTION Real qnan() { return Kokkos::bit_cast<Real>(std::uint32_t{0x7fc00000u}); }

// out(i, TANH(x(i))) for i < n as the object computes :498 and :525: _ZGVbN4v_tanhf by
// four from the first element, then tanhf for the last n mod 4 (all of them when n < 4).
template <class X, class Out>
KOKKOS_INLINE_FUNCTION void mvec_tanhf(int n, const X& x, const Out& out) {
  int i = 0;
#if defined(WW_ST6_HAVE_LIBMVEC_TANHF)
  KOKKOS_IF_ON_HOST((for (; i + 4 <= n; i += 4) {
    alignas(16) float v[4], r[4];
    for (int l = 0; l < 4; ++l) v[l] = x(i + l);
    _mm_store_ps(r, _ZGVbN4v_tanhf(_mm_load_ps(v)));
    for (int l = 0; l < 4; ++l) out(i + l, r[l]);
  }))
#endif
  for (; i < n; ++i) out(i, Kokkos::tanh(x(i)));
}
}  // namespace sin6_detail

/// SIG = SIG2(IKN), DSII = DSII2(IKN) with DSII2 = DDEN2/DTH/SIG2 (:457-459), NK values.
/// The kernel uses it, and the shim on the host for the SIG, DSII it passes to
/// lfactor_tables(): the first LFACTOR call, inside the first W3SIN6 call, builds the 10 Hz
/// tables from these. sig2, dden2 = &SIG2(1), &DDEN2(1); ikn from w3sds6_ikn().
KOKKOS_INLINE_FUNCTION void w3sin6_sig_dsii(int nk, const int* ikn, const Real* sig2,
                                            const Real* dden2, Real dth, Real* sig, Real* dsii) {
  for (int ik = 0; ik < nk; ++ik) {
    const int j = ikn[ik] - 1;
    dsii[ik] = (dden2[j] / dth) / sig2[j];
    sig[ik] = sig2[j];
  }
}

/// Reals of work w3sin6_point() needs: 5*NSPEC + 9*NK of its own (K, SDENSIG, CG2,
/// SQRTBN2, W1 then W2; DSII, SIG, WN, ADENSIG, KMAX, ANAR, SQRTBN, LFACT, CINV), then
/// 6*NK10Hz shared by LFACTOR (6*NK10Hz) and, after it, TAU_WAVE_ATMOS (4*NK10Hz).
/// nk10hz = lfactor_nk10hz(NK, SIG2(IKN(1)), XFR).
KOKKOS_INLINE_FUNCTION int w3sin6_work_size(const Params& p, int nk10hz) {
  return 5 * p.nspec + 9 * p.nk + 6 * nk10hz;
}

/// W3SIN6 (w3src6md.F90:298-539) for one point, arrays 0-based. a, wn2 = A, WN2(NSPEC);
/// cg = CG(NK); s, d = S, D(NSPEC) out. cd is unused by the Fortran.
/// Module state: p.nk, p.nth, p.nspec; dth, xfr; sig2, dden2, ecos, esin = &SIG2(1),
/// &DDEN2(1), &ECOS(1), &ESIN(1), read over NSPEC; the SIN6* scalars of W3GDATMD; ikn the
/// 1-based table of w3sds6_ikn() (one per process, as the Fortran SAVE); mk10hz, sig10hz,
/// dsii10hz from lfactor_tables() run once with w3sin6_sig_dsii() of the first call.
/// work holds w3sin6_work_size() reals. Returns the code described at kSin6TauShift.
KOKKOS_INLINE_FUNCTION int w3sin6_point(
    const Real* a, const Real* cg, const Real* wn2, Real uabs, Real ustar, Real usdir, Real cd,
    Real dair, Real& tauwx, Real& tauwy, Real& taunwx, Real& taunwy, Real& charn, Real* s,
    Real* d, const Params& p, Real dth, Real xfr, const Real* sig2, const Real* dden2,
    const Real* ecos, const Real* esin, Real sin6a0, Real sin6ws, Real sin6chkmin,
    bool sin6flcap, Real sin6chkcap, Real sin6chkinf, Real sin6chksig, const int* ikn,
    int mk10hz, const Real* sig10hz, const Real* dsii10hz, Real* work) {
  using sin6_detail::maxss;
  using sin6_detail::minss;
  (void)cd;
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.);
  const Real ten = static_cast<Real>(10.);
  const Real eleven = static_cast<Real>(11.);

  Real* k = work;
  Real* sdensig = work + nspec;
  Real* cg2 = work + 2 * nspec;
  Real* sqrtbn2 = work + 3 * nspec;
  Real* w = work + 4 * nspec;  // W1 (:495), then W2 (:523)
  Real* dsii = work + 5 * nspec;
  Real* sig = dsii + nk;
  Real* wn = dsii + 2 * nk;
  Real* adensig = dsii + 3 * nk;
  Real* kmax = dsii + 4 * nk;
  Real* anar = dsii + 5 * nk;
  Real* sqrtbn = dsii + 6 * nk;
  Real* lfact = dsii + 7 * nk;
  Real* cinv = dsii + 8 * nk;
  Real* cwork = dsii + 9 * nk;

  // :424-431, one sincosf in the object.
  const Real cosu = Kokkos::cos(usdir);
  const Real sinu = Kokkos::sin(usdir);
  taunwx = zero;
  taunwy = zero;
  tauwx = zero;
  tauwy = zero;
  charn = sin6chkmin;

  const Real uproxy = sin6ws * ustar;  // :444
  // ECOS2, ESIN2 (:446-447) are read in place; CINV2 = WN2/SIG2 (:461) per element.

  w3sin6_sig_dsii(nk, ikn, sig2, dden2, dth, sig, dsii);    // :457-459
  for (int ik = 0; ik < nk; ++ik) wn[ik] = wn2[ikn[ik] - 1];  // :460
  for (int ith = 1; ith <= nth; ++ith) {                       // :463-465
    for (int ik = 0; ik < nk; ++ik) cg2[ikn[ik] + ith - 2] = cg[ik];
  }

  // :470-471. SUM(K,1) adds over ITH in order from 0.0 (the vector loop adds the lanes one
  // by one), then (sum*SIG)*DTH.
  for (int is = 0; is < nspec; ++is) k[is] = a[is];
  for (int ik = 0; ik < nk; ++ik) {
    Real v = zero;
    for (int ith = 0; ith < nth; ++ith) v = v + k[ith + ik * nth];
    adensig[ik] = (v * sig[ik]) * dth;
  }

  // :474, _gfortran_maxval_r4: skip to the first element >= -Inf (not NaN), then keep the
  // larger (a tie or a NaN keeps the running value).
  for (int ik = 0; ik < nk; ++ik) {
    const Real* col = k + ik * nth;
    Real r;
    if (nth <= 0) {
      r = sin6_detail::neg_huge();
    } else {
      r = sin6_detail::neg_inf();
      int n = 0;
      for (; n < nth; ++n) {
        if (col[n] >= r) break;
      }
      if (n >= nth) {
        r = sin6_detail::qnan();
      } else {
        for (; n < nth; ++n) r = (col[n] > r) ? col[n] : r;
      }
    }
    kmax[ik] = r;
  }
  // :475-481. A NaN KMAX takes the division branch, as the object's comiss does.
  for (int ik = 0; ik < nk; ++ik) {
    Real* col = k + ik * nth;
    if (kmax[ik] < static_cast<Real>(1.0E-34)) {
      for (int ith = 0; ith < nth; ++ith) col[ith] = one;
    } else {
      for (int ith = 0; ith < nth; ++ith) col[ith] = col[ith] / kmax[ik];
    }
  }

  // :484
  for (int ik = 0; ik < nk; ++ik) {
    Real v = zero;
    for (int ith = 0; ith < nth; ++ith) v = v + k[ith + ik * nth];
    anar[ik] = one / (v * dth);
  }
  // :486, WN**3 as (WN*WN)*WN; :487-489 scatter through IKN.
  for (int ik = 0; ik < nk; ++ik) {
    sqrtbn[ik] = Kokkos::sqrt((anar[ik] * adensig[ik]) * ((wn[ik] * wn[ik]) * wn[ik]));
  }
  for (int ith = 1; ith <= nth; ++ith) {
    for (int ik = 0; ik < nk; ++ik) sqrtbn2[ikn[ik] + ith - 2] = sqrtbn[ik];
  }

  // :495
  for (int is = 0; is < nspec; ++is) {
    const Real m = maxss(((uproxy * (wn2[is] / sig2[is])) * (ecos[is] * cosu + esin[is] * sinu)) - one,
                         zero);
    w[is] = m * m;
  }
  // :497-498. DAIR/DWAT is one division.
  const Real dairdw = dair / ww::srce::kDwat;
  sin6_detail::mvec_tanhf(
      nspec, [=](int is) { return ((ten * sqrtbn2[is]) * w[is]) - eleven; },
      [=](int is, Real t) {
        d[is] = (((dairdw * sig2[is]) * (sin6_detail::k28 - (one + t))) * sqrtbn2[is]) * w[is];
      });
  for (int is = 0; is < nspec; ++is) s[is] = d[is] * a[is];  // :500

  // :504-507
  for (int ik = 0; ik < nk; ++ik) {
    const int j = ikn[ik] - 1;
    cinv[ik] = wn2[j] / sig2[j];
  }
  for (int is = 0; is < nspec; ++is) sdensig[is] = (s[is] * sig2[is]) / cg2[is];
  const int rc_lf = lfactor(sdensig, cinv, uabs, ustar, usdir, dair, sig, dsii, lfact, tauwx,
                            tauwy, charn, p, dth, xfr, ecos, esin, sin6ws, sin6chkmin, sin6flcap,
                            sin6chkcap, sin6chkinf, sin6chksig, mk10hz, sig10hz, dsii10hz, cwork);
  if (rc_lf == kLfactorExtcde3) return rc_lf;

  // :510-515. SUM(LFACT) from 0.0 in index order; the test is REAL(NK) > sum.
  Real sumlf = zero;
  for (int ik = 0; ik < nk; ++ik) sumlf = sumlf + lfact[ik];
  if (sumlf < static_cast<Real>(nk)) {
    for (int ith = 1; ith <= nth; ++ith) {
      for (int ik = 0; ik < nk; ++ik) {
        const int is = ikn[ik] + ith - 2;
        d[is] = d[is] * lfact[ik];
      }
    }
    for (int is = 0; is < nspec; ++is) s[is] = d[is] * a[is];
  }

  // :522-531
  int rc_tw = 0;
  if (sin6a0 > zero) {
    for (int is = 0; is < nspec; ++is) {
      const Real m = minss(
          ((uproxy * (wn2[is] / sig2[is])) * (ecos[is] * cosu + esin[is] * sinu)) - one, zero);
      w[is] = m * m;
    }
    sin6_detail::mvec_tanhf(
        nspec, [=](int is) { return ((ten * sqrtbn2[is]) * w[is]) - eleven; },
        [=](int is, Real t) {
          d[is] = d[is] - (((((dairdw * sig2[is]) * sin6a0) * (sin6_detail::k28 - (one + t))) *
                            sqrtbn2[is]) *
                           w[is]);
        });
    for (int is = 0; is < nspec; ++is) s[is] = d[is] * a[is];
    for (int is = 0; is < nspec; ++is) sdensig[is] = (s[is] * sig2[is]) / cg2[is];
    const int nk10hz = tau_wave_atmos_nk10hz(sig[0], xfr, nk);
    rc_tw = tau_wave_atmos(sdensig, cinv, sig, dsii, taunwx, taunwy, p, dth, xfr, ecos, esin,
                           mk10hz, sig10hz, dsii10hz, cwork, cwork + nk10hz, cwork + 2 * nk10hz,
                           cwork + 3 * nk10hz);
  }
  return rc_lf | (rc_tw << kSin6TauShift);
}

}  // namespace ww::srce::st6
