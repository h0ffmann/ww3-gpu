// kokkos/src/ww_kokkos/ic3_wn_precalc_cheng.hpp
// WN_PRECALC_CHENG: the complex wavenumber of one frequency of the IC3 (Cheng) table, found
// by Muller's method from an open-water first guess (two search directions, three forms of the
// dispersion relation) and continued from the two previous frequencies by halving the
// frequency step until the root stays close. Phase 1, translated, not wired, not regtested.
// WW3 heritage: WW3/model/src/w3sic3md.F90:2164-2417 at h0ffmann/WW3 96892c95 (the file is
// byte-identical to NOAA-EMC/WW3 03adda91 for this routine's source: the pin and the NOAA
// base compile it the same way); authors S. Cheng, E. Rogers, S. Zieger, X. Zhao. Called by
// IC3PRECALC_CHENG (ic3_ic3precalc_cheng.hpp), which takes WnPrecalcCheng below.
// Evidence: kokkos/port_report/WN_PRECALC_CHENG_KOKKOS.md.
// SPDX-FileCopyrightText: 2009 National Weather Service (NWS), National Oceanic and Atmospheric Administration
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "ic3_bsdet.hpp"
#include "ic3_cmplx_root_muller_cheng.hpp"
#include "ic3_func1_zhao.hpp"

namespace ww::ic3 {

/// WN_PRECALC_CHENG(WN, SIGMA, SIGMAM1, WN_O, WNM1, WNM2, ES_MOD, NU, DICE, HICE, DEPTH,
/// SWITCHID, IK, KU). DOUBLE PRECISION scalars and DOUBLE COMPLEX values; the Fortran
/// result WN is written to `wn`; `switchid` is INOUT (written at :2385, :2396, :2407); ik and
/// ku are the caller's 0-based IK and KU of IC3PRECALC_CHENG, only compared with each other
/// (:2364). `root` is CMPLX_ROOT_MULLER_CHENG (CmplxRootMullerCheng, ic3_cmplx_root_muller_cheng.hpp)
/// or a callable of its signature. Returns true where the Fortran PRINTs "MULLER METHOD FAILED,
/// ES_MOD,NU,HICE:" (:2412-2414, REAL(WN) < 0 on return); the caller writes it with
/// es_mod, nu, hice, a list-directed PRINT having no C++ twin.
///
/// Complex arithmetic is written out as gfortran -O3 emits it (func1_zhao_detail: the product
/// and the Smith quotient inlined, no __muldc3); a REAL or INTEGER operand is promoted to
/// (x, 0.0) and its products with 0.0 are kept. ABS of a DOUBLE COMPLEX is glibc's cabs
/// (bsdet_cabs), SQRT of one csqrt.
template <class RootMuller>
KOKKOS_INLINE_FUNCTION bool wn_precalc_cheng(Kokkos::complex<double>& wn, double sigma,
                                             double sigmam1, const Kokkos::complex<double>& wn_o,
                                             const Kokkos::complex<double>& wnm1,
                                             const Kokkos::complex<double>& wnm2, double es_mod,
                                             double nu, double dice, double hice, double depth,
                                             int& switchid, int ik, int ku,
                                             const RootMuller& root) {
  namespace f = func1_zhao_detail;
  namespace d = drfun_quad_cheng_detail;
  using f::cplx;
  // REAL literals stored in or promoted to DOUBLE PRECISION, exact (GIMPLE constants).
  constexpr double rr0 = static_cast<double>(0.2f);  // RR = 0.2 (:2274)
  constexpr double eps = 1.0e-10;        // EPS = 1.D-10 (:2276), a DOUBLE literal
  constexpr double c003 = static_cast<double>(0.03f);  // (:2288, :2296, :2337, :2394)
  constexpr double c099 = static_cast<double>(0.99f);  // (:2342)
  constexpr double c01 = static_cast<double>(0.1f);  // (:2372)
  constexpr double c02 = static_cast<double>(0.2f);  // (:2376)
  constexpr double c1e7 = 1.0e7;  // 1.e7 (:2289) is exact in REAL
  const cplx two(2.0, 0.0);
  const cplx four(4.0, 0.0);

  // :2270. CMPLX without KIND= has the default (REAL) kind: both parts are rounded to float.
  const cplx gv(static_cast<double>(static_cast<float>(es_mod)),
                static_cast<double>(static_cast<float>(-(sigma * nu * dice))));
  // :2271. KP = SIGMA / SQRT(4*GV/DICE); the root is the same value at :2334, :2359 (CSE).
  const cplx g4 = f::div(f::mul(gv, four), cplx(dice, 0.0));
  const cplx sq = d::csqrt(g4.real(), g4.imag());
  auto kp_of = [&](double s) { return f::div(cplx(s, 0.0), sq); };
  cplx kp = kp_of(sigma);
  cplx ks = f::mul(kp, two);  // :2272, KS = 2*KP

  // :2280-2282. The initial guesses from the open-water wavenumber (RR = 0.2).
  const cplx x1_0 = wn_o;
  const cplx x0_0 = f::mul(x1_0, cplx(1.0 - rr0, 0.0));
  const cplx x2_0 = f::mul(x1_0, cplx(1.0 + rr0, 0.0));

  cplx wn1;
  cplx wn2;
  // :2285-2292. A failed root (REAL < 0), the shear wave mode (KS), or the pressure wave mode
  // (KP) with ES_MOD > 1.e7*NU: the simplified form (index 2).
  root(x0_0, x1_0, x2_0, 1, sigma, es_mod, nu, dice, hice, depth, wn1);
  if (wn1.real() < 0.0 || bsdet_cabs(f::sub(wn1, ks)) / bsdet_cabs(ks) < c003 ||
      (bsdet_cabs(f::sub(wn1, kp)) / bsdet_cabs(kp) < c003 && es_mod > nu * c1e7)) {
    root(x0_0, x1_0, x2_0, 2, sigma, es_mod, nu, dice, hice, depth, wn1);
  }
  // :2295-2299. The same with the guesses in the opposite order.
  root(x2_0, x1_0, x0_0, 1, sigma, es_mod, nu, dice, hice, depth, wn2);
  if (wn2.real() < 0.0 || bsdet_cabs(f::sub(wn2, ks)) / bsdet_cabs(ks) < c003) {
    root(x2_0, x1_0, x0_0, 2, sigma, es_mod, nu, dice, hice, depth, wn2);
  }
  // :2300-2304.
  cplx wn0;
  if (Kokkos::fabs(wn1.real() - wn_o.real()) < Kokkos::fabs(wn2.real() - wn_o.real())) {
    wn0 = wn1;
  } else {
    wn0 = wn2;
  }

  // The result WN is local until the end: the Fortran writes the INTENT(OUT) dummy WN directly,
  // and no later statement reads it before the last write.
  cplx w = wn;
  if (sigmam1 == 0.0) {  // :2306
    w = wn0;
  } else {
    // :2312. maxsd of the object: R2 = (|dsigma/sigma| > |dwn/wnm2|) ? |dsigma/sigma| : |dwn/wnm2|
    // (a NaN gives the second argument of MAX, the first quotient).
    const double r2a = bsdet_cabs(f::sub(wnm1, wnm2)) / bsdet_cabs(wnm2);
    const double dsig = sigma - sigmam1;
    const double r2b = Kokkos::fabs(dsig / sigma);
    const double r2 = (r2b > r2a) ? r2b : r2a;
    int ix = 0;
    cplx x1;
    // :2313-2354. I = 1, 3, 5, 7; the two coarsest are skipped when SIGMA > 4.
    for (int i = 1; i <= 7; i += 2) {
      if (i < 3 && sigma > 4.0) continue;  // :2314
      const int num = 1 << (i - 1);        // :2317
      // :2318. MIN(MAX(R2/NUM, 0.001D0), 0.5D0): maxsd then minsd of the object, the
      // destination first (a NaN gives the constant).
      const double q = r2 / static_cast<double>(num);
      const double m1 = (q > 0.001) ? q : 0.001;
      const double rr = (m1 < 0.5) ? m1 : 0.5;
      x1 = wnm1;  // :2319
      for (ix = 1; ix <= num; ++ix) {  // :2320
        const cplx x0 = f::mul(x1, cplx(1.0 - rr, 0.0));  // :2321
        const cplx x2 = f::mul(x1, cplx(rr + 1.0, 0.0));   // :2322
        // :2323. (1.0*IX)/NUM is REAL / REAL, rounded to float, then widened to the product.
        const double sigma0 =
            sigmam1 + dsig * static_cast<double>(static_cast<float>(ix) / static_cast<float>(num));
        root(x0, x1, x2, 1, sigma0, es_mod, nu, dice, hice, depth, w);  // :2325
        if (w.real() < 0.0) {  // :2326, another searching direction
          root(x2, x1, x0, 1, sigma0, es_mod, nu, dice, hice, depth, w);
        }
        if (w.real() < 0.0) {  // :2330, another dispersion relation form
          root(x0, x1, x2, 3, sigma0, es_mod, nu, dice, hice, depth, w);
        }
        kp = kp_of(sigma0);  // :2334
        ks = f::mul(kp, two);  // :2335
        // :2337. 3 is the simple dispersion relation form, 2 the simplified one.
        if (bsdet_cabs(f::sub(w, ks)) / bsdet_cabs(ks) < c003 || w.real() < 0.0) {
          root(x0, x1, x2, 2, sigma0, es_mod, nu, dice, hice, depth, w);
        }
        x1 = w;  // :2341
        // :2342-2346. X1 = WN (:2341) was just assigned, so ABS(X1-WN) is ABS(WN-WN) (0, or NaN for a
        // non-finite WN: kept as the object keeps it).
        if (w.real() < c099 * wnm1.real() ||
            (bsdet_cabs(f::sub(w, wn0)) > eps &&
             (bsdet_cabs(f::sub(x1, w)) / bsdet_cabs(w) > static_cast<double>(0.3f) ||
              w.imag() / (x1.imag() + eps) > 10.0 || w.real() < 0.0))) {
          break;  // redo with smaller intervals
        }
        // :2347-2348. X0 = X1; X1 = WN: both dead (X0 is rebuilt, X1 is already WN).
      }
      if (ix == num + 1) break;  // :2351
      // :2354. WN = X1: X1 is WN at the exit above.
      w = x1;
    }

    // :2359-2410. Part 2: choose between the two roots.
    kp = kp_of(sigma);  // :2359
    if (wn0.real() > 0.0 && wn0.imag() >= 0.0 && bsdet_cabs(f::sub(w, wn0)) > eps) {
      if ((nu > 0.0 && ik >= ku - 1) || (nu > 0.0 && switchid != 0)) {
        // :2364-2366 one mode switch for a general viscoelastic model: keep WN.
      } else {
        const double dis =
            Kokkos::fabs(w.real() - wn_o.real()) / Kokkos::fabs(wn0.real() - wn_o.real());
        const double kappa = (w.imag() + eps) / (wn0.imag() + eps);
        if ((dis >= 1.0 && kappa >= 1.0 && wn0.imag() >= c01 * wnm1.imag() &&
             bsdet_cabs(f::sub(w, kp)) < bsdet_cabs(f::sub(wn0, kp))) ||
            (dis >= 1.0 && kappa < 1.0 &&
             ((kappa > c02 && wn0.imag() / wn0.real() < 0.5) ||
              Kokkos::fabs(w.real() - kp.real()) < Kokkos::fabs(wn0.real() - kp.real()))) ||
            (kappa > 1.0 && dis < 1.0 &&
             Kokkos::fabs(w.real() - wnm1.real()) > Kokkos::fabs(wn0.real() - wnm1.real()))) {
          w = wn0;
          switchid = ik;
        }
        // the ELSEIF (DIS < 1 .AND. KAPPA < 1) branch keeps WN (:2387).
      }
      // :2394. A dominant mode closer to the pressure wave than 3 %.
      if (bsdet_cabs(f::sub(w, kp)) / bsdet_cabs(kp) < c003) {
        w = wn0;
        switchid = ik;
      }
      // :2403.
      if (wn0.real() > wnm1.real() && wn0.real() < w.real() &&
          Kokkos::fabs(wn0.imag() - wnm1.imag()) < Kokkos::fabs(w.imag() - wnm1.imag())) {
        w = wn0;
        switchid = ik;
      }
    }
  }
  wn = w;
  return w.real() < 0.0;  // :2412
}

/// WN_PRECALC_CHENG with the Cheng root finder, as the callable `wn_precalc_cheng` that
/// IC3PRECALC_CHENG (ic3_ic3precalc_cheng.hpp) takes.
struct WnPrecalcCheng {
  KOKKOS_INLINE_FUNCTION bool operator()(Kokkos::complex<double>& wn, double sigma, double sigmam1,
                                         const Kokkos::complex<double>& wn_o,
                                         const Kokkos::complex<double>& wnm1,
                                         const Kokkos::complex<double>& wnm2, double es_mod,
                                         double nu, double dice, double hice, double depth,
                                         int& switchid, int ik, int ku) const {
    return wn_precalc_cheng(wn, sigma, sigmam1, wn_o, wnm1, wnm2, es_mod, nu, dice, hice, depth,
                            switchid, ik, ku, CmplxRootMullerCheng{});
  }
};

}  // namespace ww::ic3
