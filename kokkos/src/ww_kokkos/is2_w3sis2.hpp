// kokkos/src/ww_kokkos/is2_w3sis2.hpp
// W3SIS2: floe-size dependent scattering of waves in the marginal ice zone (tabulated
// scattering coefficients for a semi-infinite ice sheet, Dumont et al. 2011; Williams et al.
// 2013), flexural break-up of the floes and creep or anelastic dissipation, one spectral
// point. Phase 1, translated, not regtested.
// WW3 heritage: model/src/w3sis2md.F90:652-1086 at h0ffmann/WW3 96892c95 (byte-identical to
// NOAA-EMC/WW3 03adda91 in this file); authors F. Ardhuin, P. Nicot, C. Sevigny, G. Boutin.
// Evidence: kokkos/port_report/W3SIS2_KOKKOS.md.
// The tables it reads (SIS2ALPHAS, ICEDAVETAB) are filled by INSIS2 (is2_insis2.hpp).
// ⚠ On a device expf, powf, logf and tanhf are CUDA's, not glibc's.
// SPDX-FileCopyrightText: 2009 National Weather Service (NWS), National Oceanic and Atmospheric Administration
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "is2_w3fsd_dave.hpp"
#include "is2_w3rpwnice.hpp"
#include "libm_device.hpp"
#include "real.hpp"
#include "srce_params.hpp"
#include "st4_params.hpp"

#if defined(__x86_64__) && defined(__SSE2__)
#include <xmmintrin.h>
extern "C" __m128 _ZGVbN4v_logf(__m128);
#define WW_IS2_HAVE_LIBMVEC 1
#endif

namespace ww::is2 {

/// NTHICK, NICED, FRAGILITY-free sizes of the module (w3sis2md.F90:78).
inline constexpr int kNthick = 20;
inline constexpr int kNiced = 500;

/// Grid and module state W3SIS2 reads. Pointers are to the first element used.
struct Sis2Grid {
  int nk;          ///< NK
  int nth;         ///< NTH
  int nspec;       ///< NSPEC
  Real xfr;        ///< XFR
  Real iicehmin;   ///< IICEHMIN
  bool iicesmooth; ///< IICESMOOTH
  const Real* is2pars;     ///< IS2PARS(1:24), IS2PARS(n) is is2pars[n-1]
  const Real* sig;         ///< SIG(1:NK) (SIG is (0:NK+1), so &SIG(1))
  const Real* dden;        ///< DDEN(1:NK)
  const Real* sis2alphas;  ///< SIS2ALPHAS(NTHICK,NK), (j,i) at (j-1) + NTHICK*(i-1)
  const Real* icedavetab;  ///< ICEDAVETAB(NICED)
};

/// The automatic arrays of W3SIS2 that are not dead, all (NK). The Fortran also holds DUMMY,
/// WLG, WSQ, CG_EFF, ALPHA_DISP, WAMP, ESPEC, whose elements nothing reads back (WAMP, CG_EFF
/// are read at the same IK that writes them, so they are scalars here); BRK_CRIT_W is read
/// at IK-1 and IK+1 after the loop that writes it, so it is an array.
struct Sis2Work {
  Real* icedave;
  Real* wn_i;
  Real* wn_rp;
  Real* cg_i;
  Real* wlg_i;
  Real* cgratio;
  Real* curv;
  Real* curvspec;
  Real* s_d;
  bool* brk_crit_w;
};

namespace w3sis2_detail {
/// out[i] = LOG(sig[i]*tau) for i < n, as the object computes S_D (:817): groups of four from
/// the first element through libmvec's _ZGVbN4v_logf when n >= 4, then scalar logf for the last
/// n mod 4. libmvec and scalar logf differ in the last bits, so the grouping is arithmetic. On a
/// device the lanes are scalar dm::logf ⚠ (no device copy of libmvec's logf).
KOKKOS_INLINE_FUNCTION void log_scaled(int n, const Real* sig, Real tau, Real* out) {
  int i = 0;
#if defined(WW_IS2_HAVE_LIBMVEC)
  KOKKOS_IF_ON_HOST((for (; i + 4 <= n; i += 4) {
    alignas(16) float a[4], r[4];
    for (int l = 0; l < 4; ++l) a[l] = tau * sig[i + l];
    _mm_store_ps(r, _ZGVbN4v_logf(_mm_load_ps(a)));
    for (int l = 0; l < 4; ++l) out[i + l] = r[l];
  }))
#endif
  for (; i < n; ++i) out[i] = ww::dm::logf(sig[i] * tau);
}

/// maxss of w3sis2md.o with d the destination register: (d > s) ? d : s.
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }

KOKKOS_INLINE_FUNCTION int imax(int a, int b) { return (a > b) ? a : b; }
KOKKOS_INLINE_FUNCTION int imin(int a, int b) { return (a < b) ? a : b; }

/// FLOOR of a REAL as the object computes it (:887): i = INT(x), then REAL(i) <= x ? i : i-1.
KOKKOS_INLINE_FUNCTION int ffloor(Real x) {
  const int i = ww::st4::fint(x);
  return static_cast<Real>(i) <= x ? i : static_cast<int>(static_cast<unsigned>(i) - 1u);
}

using ww::srce::kDwat;
using ww::srce::kGrav;
using ww::srce::kTpi;
using ww::srce::kTpiInv;
inline constexpr Real kPi = ww::kPi;

inline constexpr Real kThick1 = 0.1f;   // THICK1, a module variable never assigned, :80
inline constexpr Real kDthick = 0.25f;  // DTHICK, :80

inline constexpr Real kYoung = 5.49E+9f;  // YOUNG, :783
inline constexpr Real kPoisson = 0.3f;    // POISSON, :784
inline constexpr Real kDens = 1025.0f;    // DENS, :786, named "ice density" in the source
// MP2 = (1-POISSON**2), folded (:811).
inline constexpr Real kMp2 = 1.0f - kPoisson * kPoisson;
static_assert(kMp2 == 9.0999996662139892578125e-1f);
inline constexpr Real kAlphaD = 0.54f;  // ALPHA_D, :814
// 1.60218E-19, 1.38064852e-23*268.15 as folded in :815.
inline constexpr Real kElectron = 1.60218E-19f;
inline constexpr Real kBoltzT = 1.38064852e-23f * 268.15f;
static_assert(kBoltzT == 3.70220919798803156842082666497933951532672836037818342447e-21f);
inline constexpr Real kColeB = 1.205E-9f;
inline constexpr Real kTauDiv = 0.07f;  // TAU_D = B_COLE/0.07, :816
// DWAT*GRAV, 3*DWAT*GRAV*MP2, the constants of :826, :1055.
inline constexpr Real kDwatGrav = kDwat * kGrav;
static_assert(kDwatGrav == 9.806e+3f);
// (PI**4*YOUNG*ICEH**3)/(48*DENS*GRAV*MP2) of :840: PI**4*YOUNG and 48*DENS*GRAV*MP2 folded.
inline constexpr Real kPi4 = (kPi * kPi) * (kPi * kPi);
inline constexpr Real kPi4Young = kPi4 * kYoung;
static_assert(kPi4Young == 5.34775955456e+11f);
inline constexpr Real kFlexDen = ((48.0f * kDens) * kGrav) * kMp2;
static_assert(kFlexDen == 4.3903421875e+5f);
// 3*DWAT*GRAV*MP2 of the R(IK) quotient (:953).
inline constexpr Real kRDen = ((3.0f * kDwat) * kGrav) * kMp2;
static_assert(kRDen == 2.677037890625e+4f);
// (YOUNG/MP2)**2 of :1053.
inline constexpr Real kYoungMp2 = kYoung / kMp2;
inline constexpr Real kYoungMp2Sq = kYoungMp2 * kYoungMp2;
static_assert(kYoungMp2Sq == 3.63966936096178176e+19f);
}  // namespace w3sis2_detail

/// W3SIS2(A, DEPTH, CICE, ICEH, ICEF, ICEDMAX, IX, IY, S, D, DISSIP, WN, CG, WN_R, CG_ICE, R)
/// (:652-1086), one point, arrays 0-based. DEPTH, IX and IY are not read by the Fortran and are
/// not arguments. icedmin is the module variable ICEDMIN (:81) that W3SIS2 assigns (:819,
/// :996) and reads: the caller owns one per thread, as it owns the arrays of w. w3rpwnice
/// returns an error code that W3SIS2 does not test, nor does the port.
/// a, s, d, dissip (NSPEC); wn, cg, wn_r, cg_ice, r (NK). r is not written when cice <= 0.
KOKKOS_INLINE_FUNCTION void w3sis2_point(const Sis2Grid& g, const Sis2Work& w, const Real* a,
                                         Real cice, Real iceh, Real& icef, Real icedmax, Real* s,
                                         Real* d, Real* dissip, const Real* wn, const Real* cg,
                                         const Real* wn_r, const Real* cg_ice, Real* r,
                                         Real& icedmin) {
  using namespace w3sis2_detail;
  const int nk = g.nk;
  const int nth = g.nth;
  const int nspec = g.nspec;
  const Real* p = g.is2pars;
  const Real* sig = g.sig;
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.);

  // :802-809.
  for (int is = 0; is < nspec; ++is) {
    s[is] = zero;
    d[is] = zero;
    dissip[is] = zero;
  }
  for (int ik = 0; ik < nk; ++ik) {
    w.wn_i[ik] = zero;
    w.wn_rp[ik] = zero;
    w.cg_i[ik] = zero;
  }
  const Real sigma_c = p[18];  // IS2PARS(19)
  // :815-817. EXP(x*1.60218E-19/(...)): the product first, the folded divisor second.
  const Real b_cole = ww::dm::expf(p[23] * kElectron / kBoltzT) * kColeB;
  const Real tau_d = b_cole / kTauDiv;
  log_scaled(nk, sig, tau_d, w.s_d);
  const Real delta_d = p[20];  // IS2PARS(21)
  if (p[8] > zero) icedmin = p[8];
  Real creepfac;
  if (p[11] > zero) {
    const Real b = p[11];
    // :825-826. -2*(...) is the negation of the whole product; powf calls, exponent REAL.
    const Real e2 = p[14] + 2.0f;
    const Real e1 = p[14] + 1.0f;
    const Real q = ((0.25f / e2) * 2.0f) * 0.375f;
    creepfac = -((((b * q) * ww::dm::powf(iceh, e2)) *
                  ww::dm::powf(kYoung / ((b * 2.0f) * kMp2), e1)) /
                 kDwatGrav);
  } else {
    creepfac = one;
  }

  Real dmax = icef;  // :833, ICEF_CREEP = ICEF (:832) is dead
  bool brk_crit = false;
  const int nsum = ww::st4::nint(0.3f / (g.xfr - one));
  const Real strain_c = (sigma_c * kMp2) / kYoung;

  // :840. ICEH**3 as ICEH*(ICEH*ICEH).
  const Real iceh2 = iceh * iceh;
  const Real iceh3 = iceh * iceh2;
  const Real d_flex_fail = ww::dm::powf((kPi4Young * iceh3) / kFlexDen, 0.25f) * 0.5f;

  // :842-856.
  if (dmax > static_cast<Real>(kNiced)) {
    for (int ik = 0; ik < nk; ++ik) w.icedave[ik] = dmax;
  } else if (p[8] > zero) {
    const Real v = g.icedavetab[imax(1, ww::st4::nint(dmax)) - 1];
    for (int ik = 0; ik < nk; ++ik) w.icedave[ik] = v;
  } else {
    for (int ik = 0; ik < nk; ++ik) {
      const Real lo = p[13] * (kTpi / wn_r[ik]);  // IS2PARS(14)*(TPI/WN_R(IK))
      if (dmax > lo) {
        w.icedave[ik] = w3fsd_dave<false>(lo, dmax, p[7]);
      } else {
        w.icedave[ik] = dmax;
      }
    }
  }

  if (cice > zero) {
    // :868-883.
    if (p[5] > 0.5f) {
      w3rpwnice(iceh, w.wn_i, w.wn_rp, w.cg_i, nk, sig);
    } else {
      for (int ik = 0; ik < nk; ++ik) {
        w.wn_i[ik] = wn_r[ik];
        w.cg_i[ik] = cg_ice[ik];
        w.wn_rp[ik] = wn[ik] * zero;
      }
    }
    for (int ik = 0; ik < nk; ++ik) w.wlg_i[ik] = kTpi / w.wn_i[ik];
    if (p[15] > 0.5f) {
      for (int ik = 0; ik < nk; ++ik) w.cgratio[ik] = w.cg_i[ik] / cg[ik];
    } else {
      for (int ik = 0; ik < nk; ++ik) w.cgratio[ik] = one;
    }

    // :887-898. IND1 = 1+FLOOR(X), W = X-REAL(FLOOR(X)).
    const Real x = (iceh - kThick1);
    const Real xq = x / kDthick;
    const int fl = ffloor(xq);
    int ind1 = static_cast<int>(1u + static_cast<unsigned>(fl));
    int ind2 = static_cast<int>(static_cast<unsigned>(ind1) + 1u);
    Real wt = xq - static_cast<Real>(fl);
    if (ind1 < 1) {
      ind1 = 1;
      wt = zero;
    } else if (ind2 > kNthick) {
      ind2 = kNthick;
      ind1 = kNthick;
      wt = zero;
    }

    Real curvtostrain = zero;
    for (int ik = 1; ik <= nk; ++ik) {
      const int k0 = ik - 1;
      // :905.
      const Real cg_eff = cice * w.cg_i[k0] + (one - cice) * cg[k0];
      // :909-910.
      const Real* tab = g.sis2alphas + kNthick * k0;
      Real alpha = -(cice * ((((tab[ind1 - 1] * (one - wt)) + (tab[ind2 - 1] * wt)) / w.icedave[k0]) +
                             ((p[6] * 2.0f) * w.wn_rp[k0])));
      // :914.
      alpha = p[0] * alpha - ((p[3] * 2.0f) * ww::dm::expf(-(p[4] / sig[k0])));
      if (p[10] > zero) {
        if (cice < 0.2f) {
          alpha = zero;
        } else if (cice > 0.8f) {
          alpha = zero;
        } else {
          alpha = ((cice - 0.2f) * alpha) * (0.8f - cice);
        }
      }
      const Real beta = (alpha * cice) * cg_eff;  // :923
      Real sumalldir = zero;
      Real suma = zero;
      // :930.
      curvtostrain = 0.25f * (maxss(g.iicehmin, iceh) * maxss(g.iicehmin, iceh));
      for (int ith = 1; ith <= nth; ++ith) {
        const int is = ith + k0 * nth - 1;
        d[is] = beta;
        s[is] = beta * a[is];
        sumalldir = sumalldir + s[is];
        suma = suma + a[is];
      }
      // :943-951.
      Real smoothd;
      if (g.iicesmooth) {
        const Real lim = p[13] * w.wlg_i[k0];
        if (lim < dmax) {
          smoothd = ww::dm::tanhf((dmax - lim) / (dmax * p[12]));
        } else {
          smoothd = zero;
        }
      } else {
        smoothd = one;
      }
      // :953, (PI/WLG_I)**4 as (x*x)*(x*x), ICEH**3 already formed.
      const Real pw = kPi / w.wlg_i[k0];
      const Real pw2 = pw * pw;
      r[k0] = 1.0f + ((((((p[15] * smoothd) * 4.0f) * kYoung) * iceh3) * (pw2 * pw2)) / kRDen);
      // :957-961.
      const Real sume = ((g.dden[k0] * suma) / cg[k0]) / (r[k0] * w.cgratio[k0]);
      const Real tw = kTpi / w.wlg_i[k0];
      const Real tw2 = tw * tw;
      w.curvspec[k0] = (tw2 * tw2) * sume;
      sumalldir = sumalldir / static_cast<Real>(nth);
      // :969.
      for (int ith = 0; ith < nth; ++ith) {
        s[k0 * nth + ith] = s[k0 * nth + ith] - p[1] * sumalldir;
      }
    }

    // :974-977.
    if (p[9] < 0.5f) {
      dmax = icedmax;
      icef = icedmax;
    }
    // :979-983.
    for (int ik = 1; ik <= nk; ++ik) {
      const int lo = imax(1, ik - nsum);
      const int hi = imin(nk, ik + nsum);
      Real val = zero;
      for (int j = lo; j <= hi; ++j) val = w.curvspec[j - 1] + val;
      w.curv[ik - 1] = val;
    }

    // :987-1034.
    if (p[2] > 0.5f) {
      int ikbreak = 0;
      for (int ik = 1; ik <= nk; ++ik) {
        const int k0 = ik - 1;
        const Real strain = w.curv[k0] * curvtostrain;
        if (d_flex_fail < dmax) {
          const Real wamp = p[16] * Kokkos::sqrt(strain);
          if (p[8] == zero) icedmin = (kTpi / wn_r[k0]) * p[13];
          const Real half = w.wlg_i[k0] * 0.5f;
          w.brk_crit_w[k0] = wamp > strain_c && half > icedmin && half < dmax && half > d_flex_fail;
          if (w.brk_crit_w[k0]) {
            ikbreak = ik;
            brk_crit = true;
          }
        }
      }
      // :1011-1032.
      if (brk_crit) {
        const int ikhi = ikbreak;  // the DO bounds are evaluated once; IKBREAK moves inside
        for (int ik = imax(ikbreak - nsum, 1); ik <= ikhi; ++ik) {
          if (w.curvspec[ik - 1] >= w.curvspec[ikbreak - 1] &&
              dmax >= (w.wlg_i[ik - 1] * 0.5f)) {
            ikbreak = ik;
          }
        }
        dmax = w.wlg_i[ikbreak - 1] * 0.5f;
        const int ikp1 = imin(ikbreak + 1, nk);
        const int ikm1 = imax(ikbreak - 1, 1);
        if (w.brk_crit_w[ikp1 - 1] && w.brk_crit_w[ikm1 - 1]) {
          dmax = ((w.wlg_i[ikbreak - 1] * w.curvspec[ikbreak - 1] +
                   w.wlg_i[ikp1 - 1] * w.curvspec[ikp1 - 1]) +
                  w.wlg_i[ikm1 - 1] * w.curvspec[ikm1 - 1]) /
                 (2.0f * ((w.curvspec[ikbreak - 1] + w.curvspec[ikm1 - 1]) + w.curvspec[ikp1 - 1]));
        }
        icef = dmax;
      }
    }

    // :1038-1060. S=S+DISSIP*CICE*A is inside the IK loop in the source, over the whole NSPEC.
    if (p[11] > zero) {
      for (int ik = 1; ik <= nk; ++ik) {
        const int k0 = ik - 1;
        const Real lim = p[13] * w.wlg_i[k0];
        if (lim < dmax) {
          const Real smoothd = ww::dm::tanhf((dmax - lim) / (dmax * p[12]));
          Real v;
          const Real rr = r[k0] * r[k0];
          const Real den = w.cgratio[k0] * rr;
          if (p[22] <= 0.5f) {
            const Real tw = kTpi / w.wlg_i[k0];
            v = ((((creepfac * 4.0f) * w.curv[k0]) * ww::dm::powf(tw, p[14] + 1.0f)) / den) * smoothd;
          } else {
            const Real wn4a = w.wn_i[k0] * w.wn_i[k0];
            const Real hh = iceh * 0.5f;
            const Real t = w.s_d[k0] * kAlphaD;
            v = ((((((((((5.0f * sig[k0]) * delta_d) * kAlphaD) * (wn4a * wn4a)) * kYoungMp2Sq) *
                     (hh * (hh * hh))) / 3.0f) /
                   (ww::dm::expf(t) + ww::dm::expf(-t))) *
                  smoothd) / den) / kDwatGrav;
            v = -(v * kTpiInv);
          }
          for (int ith = 0; ith < nth; ++ith) dissip[k0 * nth + ith] = v;
        }
        for (int is = 0; is < nspec; ++is) s[is] = ((cice * dissip[is]) * a[is]) + s[is];
      }
    }
  } else {
    // :1064-1066.
    dmax = zero;
    icef = zero;
  }
}

}  // namespace ww::is2
