// kokkos/src/ww_kokkos/bt8_w3sbt8.hpp
// W3SBT8: dissipation by viscous fluid mud after Dalrymple and Liu (1978), "thin model",
// one spectral point (S and D from the action density AC). Phase 1, translated, not wired,
// not regtested.
// WW3 heritage: WW3/model/src/w3sbt8md.F90:111-492 at h0ffmann/WW3 96892c95 (the same -O3
// object was compiled from it and checked against the CMake build's, see the port report);
// authors M. Orzech, W. E. Rogers.
// Evidence: kokkos/port_report/W3SBT8_KOKKOS.md.
// SPDX-FileCopyrightText: 2009 National Weather Service (NWS), National Oceanic and Atmospheric Administration
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Host side, not translated (the Fortran does them before the arithmetic, :290-307):
//  - the three INFLAGS1(-2), (-1), (0) tests and the loads RHOM = MUDD(IX,IY),
//    THICKM = MUDT(IX,IY), KINVISM = MUDV(IX,IY); the caller passes the values and, when a
//    flag is off, does the WRITE(NDSE,*) and EXTCDE(1..3) itself.
// KMIMAG (:255, :473) is stored and never read; gfortran drops it, so does this file.
//
// Complex arithmetic. W3SBT8 is built with gfortran's default -fcx-fortran-rules, and every
// REAL or INTEGER operand of a COMPLEX operation is first converted to (x, 0.). Each operator
// below is the sequence that gfortran emits, no shortcut:
//  - a product is (ar*br - ai*bi, ar*bi + ai*br), always the four products, so a real factor
//    keeps its "- ai*0." and "+ ar*0." (they carry a NaN, an infinity and the sign of a zero);
//  - a quotient is Smith's algorithm (range reduction, no NaN recovery, no __divsc3),
//    branching on fabs(br) < fabs(bi);
//  - SQRT and EXP of a COMPLEX are glibc's csqrtf and cexpf.
// A product of two REAL operands (RHOM*KINVISM, 2*RHOW*KINVISW, ...) stays REAL until it meets
// a COMPLEX operand, as in the Fortran. gfortran folds what is constant; the folded value has
// the bits of the run-time operation (IEEE), so the C++ may leave it to g++.
#pragma once

#include <cmath>
#include <complex>

#include <Kokkos_Complex.hpp>
#include <Kokkos_Core.hpp>

#include "bt8_ccosh.hpp"
#include "bt8_csinh.hpp"
#include "prop_wavnu1.hpp"
#include "real.hpp"

namespace ww::bt8 {

namespace w3sbt8_detail {

/// COMPLEX(4) as two REALs.
struct Cf {
  Real re;
  Real im;
};

/// REAL or INTEGER operand converted to COMPLEX: (x, 0.).
KOKKOS_INLINE_FUNCTION Cf cr(Real x) { return Cf{x, static_cast<Real>(0.)}; }

KOKKOS_INLINE_FUNCTION Cf operator-(const Cf& a) { return Cf{-a.re, -a.im}; }
KOKKOS_INLINE_FUNCTION Cf operator+(const Cf& a, const Cf& b) { return Cf{a.re + b.re, a.im + b.im}; }
KOKKOS_INLINE_FUNCTION Cf operator-(const Cf& a, const Cf& b) { return Cf{a.re - b.re, a.im - b.im}; }
KOKKOS_INLINE_FUNCTION Cf operator*(const Cf& a, const Cf& b) {
  return Cf{a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re};
}
/// Smith's algorithm as gfortran emits it for a / b (a numerator, b denominator).
KOKKOS_INLINE_FUNCTION Cf operator/(const Cf& a, const Cf& b) {
  if (Kokkos::fabs(b.re) < Kokkos::fabs(b.im)) {
    const Real r = b.re / b.im;
    const Real den = b.im + b.re * r;
    return Cf{(a.im + a.re * r) / den, (a.im * r - a.re) / den};
  }
  const Real r = b.im / b.re;
  const Real den = b.re + b.im * r;
  return Cf{(a.re + a.im * r) / den, (a.im - a.re * r) / den};
}

/// SQRT(COMPLEX): glibc's csqrtf on the host (libstdc++ lowers std::sqrt to __builtin_csqrtf).
/// ⚠ On a device it is Kokkos', not shown to return glibc's bits.
KOKKOS_INLINE_FUNCTION Cf csqrt_(const Cf& a) {
  Cf out{};
  KOKKOS_IF_ON_HOST((
    const std::complex<Real> z = std::sqrt(std::complex<Real>(a.re, a.im));
    out = Cf{z.real(), z.imag()};
  ))
  KOKKOS_IF_ON_DEVICE((
    const Kokkos::complex<Real> z = Kokkos::sqrt(Kokkos::complex<Real>(a.re, a.im));
    out = Cf{z.real(), z.imag()};
  ))
  return out;
}

/// REAL(EXP(COMPLEX)): glibc's cexpf on the host. ⚠ Not glibc's on a device.
KOKKOS_INLINE_FUNCTION Cf cexp_(const Cf& a) {
  Cf out{};
  KOKKOS_IF_ON_HOST((
    const std::complex<Real> z = std::exp(std::complex<Real>(a.re, a.im));
    out = Cf{z.real(), z.imag()};
  ))
  KOKKOS_IF_ON_DEVICE((
    const Kokkos::complex<Real> z = Kokkos::exp(Kokkos::complex<Real>(a.re, a.im));
    out = Cf{z.real(), z.imag()};
  ))
  return out;
}

KOKKOS_INLINE_FUNCTION Cf csinh_(const Cf& c) {
  Kokkos::complex<Real> r;
  csinh(Kokkos::complex<Real>(c.re, c.im), r);
  return Cf{r.real(), r.imag()};
}
KOKKOS_INLINE_FUNCTION Cf ccosh_(const Cf& c) {
  Kokkos::complex<Real> r;
  ccosh(Kokkos::complex<Real>(c.re, c.im), r);
  return Cf{r.real(), r.imag()};
}

// The object calls glibc's scalar sinhf (:463).
KOKKOS_INLINE_FUNCTION Real sinh_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::sinhf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::sinh(x);))
#else
  return Kokkos::sinh(x);
#endif
}

// GRAV, DWAT, NU_WATER of constants.F90:62-68 as REAL.
inline constexpr Real kGrav = static_cast<Real>(9.806);
inline constexpr Real kDwat = static_cast<Real>(1000.);
inline constexpr Real kNuWater = static_cast<Real>(1.31E-6);

}  // namespace w3sbt8_detail

/// W3SBT8(AC, H_WDEPTH, S, D, IX, IY) for one point, arrays 0-based.
///   nk, nspec  W3GDATMD NK, NSPEC
///   sig        SIG(1:NK) (the Fortran's SIG is a pointer with bounds 0:NK+1: pass SIG(1))
///   mapwn      MAPWN(1:NSPEC), Fortran 1-based wavenumber index of each spectral point
///   disp       the DISTAB table WAVNU1 reads (prop_wavnu1.hpp)
///   rhom, thickm, kinvism  MUDD, MUDT, MUDV at (IX,IY), already loaded
///   ac         AC(NSPEC); s, d  out, S(NSPEC), D(NSPEC)
///   smudwd     work, NK reals (the Fortran's SMUDWD(NK), zeroed at :322 when it is used)
KOKKOS_INLINE_FUNCTION void w3sbt8_point(int nk, int nspec, const Real* sig, const int* mapwn,
                                         const ww::prop::DispTable& disp, Real rhom,
                                         Real thickm, Real kinvism, const Real* ac,
                                         Real h_wdepth, Real* s, Real* d, Real* smudwd) {
  using namespace w3sbt8_detail;
  const Real zero = static_cast<Real>(0.);
  const Real rhow = kDwat;          // :309
  const Real kinvisw = kNuWater;    // :310
  const Real kdcutoff = static_cast<Real>(10.0);  // :311
  const Real kthreshold = static_cast<Real>(1.0E-9);  // :312
  const Real a = static_cast<Real>(1.0);  // :314
  const Cf ci{zero, static_cast<Real>(1.)};  // I = (0.,1.), :273
  const Cf c1 = cr(static_cast<Real>(1.));
  const Cf c2 = cr(static_cast<Real>(2.));
  const Cf cm1 = cr(static_cast<Real>(-1.));

  for (int is = 0; is < nspec; ++is) {  // :317-318
    d[is] = zero;
    s[is] = zero;
  }

  if (thickm > zero && rhom > zero && kinvism > zero) {  // :320
    for (int ik = 0; ik < nk; ++ik) smudwd[ik] = zero;   // :322

    for (int ik = 1; ik <= nk; ++ik) {  // :325
      const Real sigma = sig[ik - 1];   // :327
      Real k_unmud = 0, cg_unmud = 0;
      ww::prop::wavnu1(disp, sigma, h_wdepth, k_unmud, cg_unmud);  // :330
      Cf k = cr(k_unmud);                                          // :331
      Cf fm1{zero, zero};
      Cf km1{zero, zero};
      Cf kmud{zero, zero};

      for (int icount = 1; icount <= 20; ++icount) {  // :335
        const Cf kh = k * cr(h_wdepth);
        const Cf kt = k * cr(thickm);
        const Cf shh = csinh_(kh);  // :337
        const Cf chh = ccosh_(kh);  // :338
        const Cf shd = csinh_(kt);  // :339
        const Cf chd = ccosh_(kt);  // :340

        const Cf kk = k * k;
        // :343-344, I*SIGMA/KINVISW is (I*SIGMA)/KINVISW
        const Cf isigma = ci * cr(sigma);
        const Cf lam1 = csqrt_(kk - isigma / cr(kinvisw));
        const Cf lam2 = csqrt_(kk - isigma / cr(kinvism));

        const Cf lt = lam2 * cr(thickm);
        const Cf chlam2 = ccosh_(lt);  // :347
        const Cf shlam2 = csinh_(lt);  // :348

        // :351, EXP(-LAM1*H_WDEPTH) is EXP(-(LAM1*H_WDEPTH)); the REAL part is kept.
        const Real exph = cexp_(-(lam1 * cr(h_wdepth))).re;
        const Cf cexph = cr(exph);

        // :354-356
        const Cf a1 = -(lam2 * shd / k) + shlam2;
        const Cf a2 = -chd + chlam2;
        const Cf a3 = shd - lam2 * shlam2 / k;

        // :359-362
        const Cf b1 = lam1 * shh / k - chh;
        const Cf b2 = lam2 * a2 * shh / k + a1 * chh;
        const Cf b3 = -(a3 * shh) + a2 * chh;
        const Cf b4 = lam1 * shh / k + chh;

        const Cf l1l1kk = lam1 * lam1 + kk;  // (LAM1*LAM1+K*K)
        const Cf l2l2kk = lam2 * lam2 + kk;  // (LAM2*LAM2+K*K)

        // :365
        const Cf c0 = b4 * cexph - l1l1kk / (c2 * k * k);

        // :368, BET0 is REAL: the real part of -(EXPH/C0), the minus binds after the quotient
        const Real bet0 = (-(cexph / c0)).re;
        const Cf cbet0 = cr(bet0);

        // :371-374. RHOM*KINVISM, 2*RHOM*KINVISM, (RHOM-RHOW)*GRAV are REAL products.
        const Real rk = rhom * kinvism;
        const Real r2k = static_cast<Real>(2.) * rhom * kinvism;
        const Real drg = (rhom - rhow) * kGrav;
        const Cf testalf1 = -(cr(rk) * l2l2kk * (-(lam2 / k)) * chd / k) -
                            cr(r2k) * lam2 * chlam2 - cr(drg) * (ci * a1 / cr(sigma));
        const Cf testalf2 = -(cr(rk) * l2l2kk * cm1 * shd / k) - cr(r2k) * lam2 * shlam2 -
                            cr(drg) * (ci * a2 / cr(sigma));
        const Cf alf1 = -testalf1;  // :375
        const Cf alf2 = -testalf2;  // :376

        // :379-380
        const Cf psi1 = c2 * k * (-(lam2 / k)) * shd + l2l2kk * shlam2 / k;
        const Cf psi2 = c2 * k * cm1 * chd + l2l2kk * chlam2 / k;

        // :383-384. 2*RHOW*KINVISW is REAL.
        const Real r2w = static_cast<Real>(2.) * rhow * kinvisw;
        const Cf m1 = ci * cr(rhow) * cr(sigma) / k - cr(r2w) * k;
        const Cf m0 = b1 + l1l1kk * cexph / (k * k);

        const Cf bm0p = cbet0 * m0 + c1;  // BET0*M0+1
        const Cf bm0m = cbet0 * m0 - c1;  // BET0*M0-1
        const Cf bb2 = cbet0 * b2 + a1;   // BET0*B2+A1
        const Cf bb3 = cbet0 * b3 + a2;   // BET0*B3+A2
        const Cf l1b3 = lam1 * cbet0 * b3 / k - a3;  // (LAM1*BET0*B3/K-A3)
        const Cf l1b2 = lam1 * cbet0 * b2 + lam2 * a2;  // (LAM1*BET0*B2+LAM2*A2)

        Cf c11, c12, c13, c21, c22, c23, c31, c32, c33, c42, c43, c44;
        // :387-389
        c11 = lam1 * bm0p * shh / k + bm0m * chh + m0 / c0 + cexph;
        c12 = l1b2 * shh / k + bb2 * chh + b2 / c0;
        c13 = l1b3 * shh + bb3 * chh + b3 / c0;

        // :392-397
        const Cf r2wl1 = cr(r2w) * lam1;  // 2*RHOW*KINVISW*LAM1
        c21 = lam1 * bm0p * m1 * chh / k + bm0m * m1 * shh - r2wl1 * m0 / c0 + r2wl1 * cexph;
        c22 = l1b2 * m1 * chh / k + bb2 * m1 * shh - r2wl1 * b2 / c0;
        c23 = l1b3 * m1 * chh + bb3 * m1 * shh - r2wl1 * b3 / c0;

        // :400-405. RHOW*KINVISW is REAL.
        const Real rwk = rhow * kinvisw;
        const Cf k2rk = c2 * k * cr(rhow) * cr(kinvisw);  // 2*K*RHOW*KINVISW
        c31 = k2rk * bm0m + cr(rwk) * l1l1kk * (c1 - cbet0 * m0) / k;
        c32 = k2rk * bb2 - cr(rwk) * l1l1kk * cbet0 * b2 / k - ci * cr(rhom) * cr(kinvism) * psi1;
        c33 = k2rk * bb3 - cr(rwk) * l1l1kk * cbet0 * b3 / k - ci * cr(rhom) * cr(kinvism) * psi2;

        // :408-412
        const Cf c41a = lam1 * m1 * bm0p / k + r2wl1 + r2wl1 * cbet0 * m0;
        const Cf c42a = m1 * l1b2 / k + r2wl1 * cbet0 * b2 + alf1;
        const Cf c43a = m1 * l1b3 + r2wl1 * cbet0 * b3 + alf2;

        // :415-417 (C(4,1) is never read; gfortran drops it)
        c44 = c41a * c31 / c41a - c31;
        (void)c44;
        c42 = c42a * c31 / c41a - c32;
        c43 = c43a * c31 / c41a - c33;

        // :420-423 (B(3), B(4) are never read)
        const Cf bv1 = -(ci * cr(sigma) * cr(a));
        const Cf bv2 = cr(rhow * kGrav * a);

        // :425-428
        const Cf cd = -((c33 - c32 * c43 / c42) / c31);
        const Cf hh = bv2 / (c21 * cd - c22 * (c43 / c42) + c23);
        const Cf dd = cd * hh;
        const Cf gg = -(c43 * hh / c42);

        // :431
        const Cf f = c11 * dd + c12 * gg + c13 * hh - bv1;

        if (icount == 1) {  // :433
          fm1 = f;
          km1 = k;
          k = k * cr(static_cast<Real>(.995)) + Cf{zero, static_cast<Real>(.001)};  // :436
        } else {
          const Real kcheck = Kokkos::fabs(k.im - km1.im);  // :439
          if ((f.re == fm1.re && f.im == fm1.im) || (k.re == km1.re && k.im == km1.im) ||
              (kcheck < kthreshold)) {  // :440
            kmud = k;                   // :442
            break;                      // :443
          }
          const Cf fp = (f - fm1) / (k - km1);  // :445
          km1 = k;
          fm1 = f;
          k = k - cr(static_cast<Real>(0.8)) * f / fp;  // :448
        }
      }

      kmud = k;  // :452

      const Real kd = kmud.re * h_wdepth;  // :455
      if (kd < kdcutoff) {                 // :457
        const Real cwave = sigma / kmud.re;  // :460
        const Real ztmp0 = static_cast<Real>(2.0) * kmud.re * h_wdepth;  // :461
        const Real ztmp = (ztmp0 < static_cast<Real>(70)) ? sinh_(ztmp0)
                                                          : static_cast<Real>(1.0E+30);  // :462-466
        const Real nwave_mud =
            static_cast<Real>(0.5) * (static_cast<Real>(1.0) + ztmp0 / ztmp);  // :467
        const Real cg_mud = nwave_mud * cwave;                                  // :468
        smudwd[ik - 1] = static_cast<Real>(2.0) * kmud.im * cg_mud;             // :470
      }
    }

    for (int is = 1; is <= nspec; ++is) {  // :480-482
      d[is - 1] = -smudwd[mapwn[is - 1] - 1];
    }
  }

  for (int is = 0; is < nspec; ++is) s[is] = d[is] * ac[is];  // :486
}

}  // namespace ww::bt8
