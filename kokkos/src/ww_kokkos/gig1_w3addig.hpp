// kokkos/src/ww_kokkos/gig1_w3addig.hpp
// W3ADDIG: the second-order (difference-interaction) spectrum, in particular the
// infragravity part of the surface elevation, from a frequency-direction spectrum by the
// bound-wave coupling coefficient of Okihiro et al. (1992), Df1f2theta, summed by the
// rectangle rule over the spectral domain. Phase 1, translated, not regtested.
// WW3 heritage: model/src/w3gig1md.F90:146-375 at h0ffmann/WW3 96892c95 (byte-identical to
// NOAA-EMC/WW3 03adda91 in this file); authors A. Rawat, F. Ardhuin. Evidence:
// kokkos/port_report/W3ADDIG_KOKKOS.md.
// WAVNU1 (prop_wavnu1.hpp) and Df1f2theta (gig1_df1f2theta.hpp) are the callees already
// translated.
// ⚠ On a device cosf and sinf of the double lanes, tanhf and atan2f are CUDA's, not glibc's.
// SPDX-FileCopyrightText: 2009 National Weather Service (NWS), National Oceanic and Atmospheric Administration
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "gig1_df1f2theta.hpp"
#include "libm_device.hpp"
#include "prop_wavnu1.hpp"
#include "real.hpp"
#include "srce_params.hpp"
#include "st4_params.hpp"

namespace ww::gig1 {

/// What W3ADDIG reads of W3GDATMD (USE list, :210-212). Pointers are to the first element used:
/// sig = SIG(1:NK) (SIG is (0:NK+1)), dsip = DSIP(1:NK) (DSIP is (0:NK+1)), th = TH(1:NTH).
struct AddigGrid {
  int nk;            ///< NK
  int nth;           ///< NTH
  int nspec;         ///< NSPEC
  Real dth;          ///< DTH
  Real igpars5;      ///< IGPARS(5), NKIG = IGPARS(5) truncated to INTEGER (:253)
  const Real* sig;   ///< SIG(1:NK)
  const Real* dsip;  ///< DSIP(1:NK)
  const Real* th;    ///< TH(1:NTH)
};

/// The ALLOCATABLE arrays of W3ADDIG, which a kernel may not allocate; the caller sizes them
/// from nkig = INT(IGPARS(5)): dd (nkig*nk*nth*nth), wfr1 and ifr2c (nkig*nk), efthig
/// (nkig*nth), espec (nspec). The Fortran's ESPEC is automatic (NSPEC).
struct AddigWork {
  Real* dd;      ///< DD(NKIG,NK,NTH,NTH), (iig,ifr,ith1,ith2) at ((ith2*nth+ith1)*nk+ifr)*nkig+iig, 0-based
  Real* wfr1;    ///< WFR1(NKIG,NK)
  int* ifr2c;    ///< IFR2C(NKIG,NK)
  Real* efthig;  ///< EFTHIG(NSPECIG)
  Real* espec;   ///< ESPEC(NSPEC)
};

namespace addig_detail {
/// MINLOC(f, 1) as gfortran -O3 inlines it for a REAL array without mask (:301, :341): the first
/// element that is not NaN (compare <= +Inf) starts the scan, later elements win only by a
/// strict <, all NaN gives 1, n <= 0 gives 0. Position is 1-based.
template <class F>
KOKKOS_INLINE_FUNCTION int minloc1(int n, const F& f) {
  int i = 1;
  for (; i <= n; ++i) {
    if (f(i) <= static_cast<Real>(INFINITY)) break;
  }
  if (i > n) return n > 0 ? 1 : 0;
  int pos = i;
  Real limit = f(i);
  for (++i; i <= n; ++i) {
    const Real v = f(i);
    if (limit > v) {
      limit = v;
      pos = i;
    }
  }
  return pos;
}

KOKKOS_INLINE_FUNCTION Real fabsr(Real x) { return Kokkos::fabs(x); }
}  // namespace addig_detail

/// W3ADDIG(E, DEPTH, WN, CG, IACTION) (:146-375), one spectrum, arrays 0-based. e (NSPEC) is
/// replaced: the first NKIG*NTH elements by the second-order spectrum, the others kept. wn, cg
/// are WN(1:NK), CG(1:NK). tab is the DISTAB table WAVNU1 reads. IACTION = 0 means e is E(f,theta),
/// otherwise A(k,theta).
KOKKOS_INLINE_FUNCTION void w3addig(const AddigGrid& g, const ww::prop::DispTable& tab,
                                    const AddigWork& w, Real* e, Real depth, const Real* wn,
                                    const Real* cg, int iaction) {
  using addig_detail::minloc1;
  const int nk = g.nk;
  const int nth = g.nth;
  const int nspec = g.nspec;
  const Real* sig = g.sig;
  const Real* th = g.th;
  const Real zero = static_cast<Real>(0.);
  const int nkig = ww::st4::fint(g.igpars5);  // :253
  const int nspecig = nth * nkig;             // :254
  for (int i = 0; i < nspecig; ++i) w.efthig[i] = zero;  // :264

  // :264-273.
  if (iaction == 0) {
    for (int i = 0; i < nspec; ++i) w.espec[i] = e[i];
  } else {
    for (int ik = 1; ik <= nk; ++ik) {
      for (int ith = 1; ith <= nth; ++ith) {
        const int isp1 = ith + (ik - 1) * nth - 1;
        w.espec[isp1] = ((e[isp1] * sig[ik - 1]) * ww::srce::kTpi) / cg[ik - 1];
      }
    }
  }

  // :275-310.
  for (int iig = 1; iig <= nkig; ++iig) {
    for (int ifr = 1; ifr <= nk; ++ifr) {
      Real wn1;
      Real cg2;
      ww::prop::wavnu1(tab, sig[ifr - 1] + sig[iig - 1], depth, wn1, cg2);
      for (int ith1 = 1; ith1 <= nth; ++ith1) {
        for (int ith2 = 1; ith2 <= nth; ++ith2) {
          const Real c = df1f2theta(sig[ifr - 1] + sig[iig - 1], -sig[ifr - 1], wn1, wn[ifr - 1],
                                    addig_detail::fabsr(th[ith1 - 1] - th[ith2 - 1]) + ww::kPi,
                                    depth);
          w.dd[((static_cast<std::size_t>(ith2 - 1) * static_cast<std::size_t>(nth) +
                 static_cast<std::size_t>(ith1 - 1)) * static_cast<std::size_t>(nk) +
                static_cast<std::size_t>(ifr - 1)) * static_cast<std::size_t>(nkig) +
               static_cast<std::size_t>(iig - 1)] = c * c;
        }
      }
      // :291. DBLE(DSIP(IFR))*DTH rounded to REAL is the REAL product.
      w.wfr1[(ifr - 1) * nkig + (iig - 1)] = g.dsip[ifr - 1] * g.dth;

      // :298-310.
      int ifr2c;
      if (sig[iig - 1] < static_cast<Real>(0.5) * g.dsip[ifr - 1]) {
        ifr2c = ifr;
      } else {
        const Real tgt = (sig[iig - 1] + sig[ifr - 1]);
        const int iloc = minloc1(nk, [&](int i) {
          return addig_detail::fabsr((sig[i - 1] - g.dsip[i - 1]) - tgt);
        });
        ifr2c = (iloc != 0) ? iloc : nk;
      }
      w.ifr2c[(ifr - 1) * nkig + (iig - 1)] = ifr2c;
    }
  }

  // :315-351.
  for (int iig = 1; iig <= nkig; ++iig) {
    for (int ifr = 1; ifr <= nk - 1; ++ifr) {
      const int f2 = w.ifr2c[(ifr - 1) * nkig + (iig - 1)];
      const Real k1 = wn[ifr - 1];
      const Real k2 = wn[f2 - 1];
      for (int ith1 = 1; ith1 <= nth; ++ith1) {
        for (int ith2 = 1; ith2 <= nth; ++ith2) {
          const int isp1 = ith1 + (f2 - 1) * nth - 1;
          const int isp2 = ith2 + (ifr - 1) * nth - 1;
          const Real eadd = ((w.dd[((static_cast<std::size_t>(ith2 - 1) * static_cast<std::size_t>(nth) +
                                     static_cast<std::size_t>(ith1 - 1)) * static_cast<std::size_t>(nk) +
                                    static_cast<std::size_t>(ifr - 1)) * static_cast<std::size_t>(nkig) +
                                   static_cast<std::size_t>(iig - 1)] *
                              w.wfr1[(ifr - 1) * nkig + (iig - 1)]) *
                             w.espec[isp1]) *
                            w.espec[isp2];
          // :332-333. DBLE(DTH*ITH) is the REAL product widened; k2*cos is DOUBLE, the
          // difference is rounded to REAL by the assignment.
          const double a2 = static_cast<double>(g.dth * static_cast<Real>(ith2));
          const double a1 = static_cast<double>(g.dth * static_cast<Real>(ith1));
          const double k2d = static_cast<double>(k2);
          const double k1d = static_cast<double>(k1);
          const Real dkx = static_cast<Real>(k2d * Kokkos::cos(a2) - k1d * Kokkos::cos(a1));
          const Real dky = static_cast<Real>(k2d * Kokkos::sin(a2) - k1d * Kokkos::sin(a1));
          Real thetaig = ww::dm::atan2f(dky, dkx);
          if (thetaig < zero) thetaig = static_cast<Real>(2.) * ww::kPi + thetaig;
          int i = minloc1(nth, [&](int t) { return addig_detail::fabsr(thetaig - th[t - 1]); }) - 1;
          if (i == 0) i = nth;
          const int isp3 = i + (iig - 1) * nth - 1;
          w.efthig[isp3] = w.efthig[isp3] + eadd;
        }
      }
    }
  }

  // :354-367.
  for (int i = 0; i < nspecig; ++i) w.espec[i] = w.efthig[i];
  if (iaction == 0) {
    for (int i = 0; i < nspecig; ++i) e[i] = w.espec[i];
  } else {
    for (int ik = 1; ik <= nkig; ++ik) {
      for (int ith = 1; ith <= nth; ++ith) {
        const int isp1 = ith + (ik - 1) * nth - 1;
        e[isp1] = (w.espec[isp1] * cg[ik - 1]) / (sig[ik - 1] * ww::srce::kTpi);
      }
    }
  }
}

}  // namespace ww::gig1
