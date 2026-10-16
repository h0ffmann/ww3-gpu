// kokkos/src/ww_kokkos/ic3_func1_zhao.hpp
// FUNC1_ZHAO: residual of the Zhao and Shen viscoelastic ice-layer dispersion relation at a
// complex wavenumber (IC3 "V1"), the JUDGE /= 0 branch of F_ZHAO_V1: the 4 x 4 determinant of
// BSDET when ES > 1E7, the closed form otherwise.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic3md.F90:1465-1608 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, S. Zieger (from Zhao and Shen's Clarkson code).
// Evidence: kokkos/port_report/FUNC1_ZHAO_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "ic3_bsdet.hpp"
#include "ic3_drfun_quad_cheng.hpp"

namespace ww::ic3 {

namespace func1_zhao_detail {

using cplx = Kokkos::complex<double>;

/// Product as gfortran -O3 expands it (Fortran complex rules, no __muldc3), operands in the
/// Fortran's order. A REAL operand is passed promoted, cplx(x, 0.0): its products with 0.0 stay
/// in both objects (NaN and the sign of zero); products with 1.0 fold in both.
KOKKOS_INLINE_FUNCTION cplx mul(const cplx& a, const cplx& b) {
  return cplx(a.real() * b.real() - a.imag() * b.imag(),
              a.real() * b.imag() + a.imag() * b.real());
}

/// Quotient, Smith's algorithm as GCC inlines it under Fortran rules.
KOKKOS_INLINE_FUNCTION cplx div(const cplx& a, const cplx& b) {
  const double ar = a.real();
  const double ai = a.imag();
  const double br = b.real();
  const double bi = b.imag();
  double tr;
  double ti;
  double den;
  if (Kokkos::fabs(br) < Kokkos::fabs(bi)) {
    const double ratio = br / bi;
    den = br * ratio + bi;
    tr = ar * ratio + ai;
    ti = ai * ratio - ar;
  } else {
    const double ratio = bi / br;
    den = bi * ratio + br;
    tr = ai * ratio + ar;
    ti = ai - ar * ratio;
  }
  return cplx(tr / den, ti / den);
}

KOKKOS_INLINE_FUNCTION cplx add(const cplx& a, const cplx& b) {
  return cplx(a.real() + b.real(), a.imag() + b.imag());
}

KOKKOS_INLINE_FUNCTION cplx sub(const cplx& a, const cplx& b) {
  return cplx(a.real() - b.real(), a.imag() - b.imag());
}

KOKKOS_INLINE_FUNCTION cplx neg(const cplx& a) { return cplx(-a.real(), -a.imag()); }

KOKKOS_INLINE_FUNCTION cplx cexp(const cplx& z) {
  return drfun_quad_cheng_detail::cexp(z.real(), z.imag());
}

}  // namespace func1_zhao_detail

/// FUNC1_ZHAO(WN, SIGMA, ES, NU, DICE, HICE, DEPTH): DOUBLE COMPLEX WN, DOUBLE PRECISION the rest,
/// returns DOUBLE COMPLEX. Its signature is the `func1` that f_zhao_v1 (ic3_f_zhao_v1.hpp) calls;
/// pass `Func1Zhao{}` there (a function name works on the host only). The Fortran's AA(4,4) and
/// BSDET's automatic MAT(4,4) are fixed-size local arrays here, column major as in bsdet().
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> func1_zhao(const Kokkos::complex<double>& wn,
                                                           double sigma, double es, double nu,
                                                           double dice, double hice,
                                                           double depth) {
  namespace f = func1_zhao_detail;
  namespace d = drfun_quad_cheng_detail;
  using f::cplx;
  // DBLE(GRAV), GRAV = 9.806 (REAL), constants.F90:62; DBLE(GRAV)**2. folds to its exact square.
  constexpr double grav = 0x1.39cacp+3;
  static_assert(grav == static_cast<double>(9.806f));
  constexpr double grav2 = 0x1.80a16a139p+6;
  static_assert(grav2 == grav * grav);
  // DBLE(DWAT), DWAT = 1000. (REAL), constants.F90:63.
  constexpr double dwat = 1000.0;
  const cplx g(grav, 0.0);
  const cplx im(0.0, 1.0);

  // :1556. CMPLX without KIND= has the default (REAL) kind: both parts are rounded to float.
  const cplx ve(static_cast<double>(static_cast<float>(nu)),
                static_cast<double>(static_cast<float>(es / dice / sigma)));
  // WN**2. etc. (a REAL exponent): cpow(z, (p, 0.0)) in the object, never expanded.
  const cplx wn2 = d::cpow(wn, 2.0);
  // :1557-1559.
  const cplx alpha = [&] {
    const cplx a = f::sub(wn2, f::mul(f::div(cplx(sigma, 0.0), ve), im));
    return d::csqrt(a.real(), a.imag());
  }();
  const cplx n = f::add(cplx(sigma, 0.0), f::mul(f::mul(f::mul(cplx(2.0, 0.0), ve), wn2), im));
  const cplx l = f::mul(
      f::mul(f::mul(f::mul(cplx(2.0, 0.0), wn), alpha), cplx(sigma, 0.0)), ve);
  // :1560-1563. EXP(+x) is called before EXP(-x), as in the object.
  const cplx two(2.0, 0.0);
  const cplx kh = f::mul(wn, cplx(hice, 0.0));
  const cplx ekh = f::cexp(kh);
  const cplx ekh_m = f::cexp(f::neg(kh));
  const cplx sk = f::div(f::sub(ekh, ekh_m), two);
  const cplx ck = f::div(f::add(ekh, ekh_m), two);
  const cplx ah = f::mul(alpha, cplx(hice, 0.0));
  const cplx eah = f::cexp(ah);
  const cplx eah_m = f::cexp(f::neg(ah));
  const cplx sa = f::div(f::sub(eah, eah_m), two);
  const cplx ca = f::div(f::add(eah, eah_m), two);

  // :1565-1574. THH (:1568-1569, :1572) is never read and the object does not compute it.
  cplx th(1.0, 0.0);
  const cplx kd = f::mul(wn, cplx(depth, 0.0));
  if (kd.real() <= 4.0) {
    // EXP(-x) is called before EXP(+x) here, as in the object.
    const cplx e_m = f::cexp(f::neg(kd));
    const cplx e_p = f::cexp(kd);
    th = f::div(f::sub(e_p, e_m), f::add(e_m, e_p));
  }

  // :1575-1576.
  const cplx m = f::sub(f::mul(cplx((dwat / dice - 1.0) * grav, 0.0), wn),
                        f::div(cplx(dwat / dice * (sigma * sigma), 0.0), th));

  if (es > 1.0e7) {
    // :1578-1599. AA(I,J) at aa[(I-1) + (J-1)*4].
    cplx aa[16];
    const cplx i2 = f::mul(cplx(2.0, 0.0), im);
    const cplx aw = f::add(d::cpow(alpha, 2.0), wn2);
    const cplx ns = f::mul(n, cplx(sigma, 0.0));
    const cplx im_m = f::mul(im, m);
    aa[0] = cplx(0.0, 0.0);
    aa[4] = f::mul(i2, wn2);
    aa[8] = aw;
    aa[12] = cplx(0.0, 0.0);

    aa[1] = ns;
    aa[5] = f::neg(f::mul(wn, g));
    aa[9] = f::mul(f::mul(im, wn), g);
    aa[13] = l;

    aa[2] = f::neg(f::mul(f::mul(i2, wn2), sk));
    aa[6] = f::mul(f::mul(i2, wn2), ck);
    aa[10] = f::mul(aw, ca);
    aa[14] = f::neg(f::mul(aw, sa));

    aa[3] = f::sub(f::mul(ns, ck), f::mul(m, sk));
    aa[7] = f::add(f::neg(f::mul(ns, sk)), f::mul(m, ck));
    aa[11] = f::sub(f::neg(f::mul(im_m, ca)), f::mul(l, sa));
    aa[15] = f::add(f::mul(im_m, sa), f::mul(l, ca));

    cplx mat[16];
    return bsdet(aa, 4, mat);
  }

  // :1601-1605.
  const cplx s2(sigma * sigma, 0.0);
  const cplx thwg = f::mul(f::mul(th, wn), g);
  const cplx rho = f::div(f::mul(th, cplx(dice, 0.0)), cplx(dwat, 0.0));
  const cplx skn = f::mul(f::mul(f::mul(wn2, cplx(grav2, 0.0)), sk), sa);
  const cplx bracket = f::add(
      d::cpow(n, 4.0),
      f::mul(f::mul(f::mul(cplx(16.0, 0.0), d::cpow(ve, 4.0)), d::cpow(wn, 6.0)),
             d::cpow(alpha, 2.0)));
  const cplx wn3 = d::cpow(wn, 3.0);
  const cplx ve2 = d::cpow(ve, 2.0);
  const cplx n2 = d::cpow(n, 2.0);
  const cplx t3 = f::mul(
      f::mul(f::mul(f::mul(f::mul(cplx(8.0, 0.0), wn3), alpha), ve2), n2),
      f::sub(f::mul(ck, ca), cplx(1.0, 0.0)));
  const cplx num = f::sub(f::sub(skn, f::mul(f::mul(bracket, sk), sa)), t3);
  const cplx den = f::sub(
      f::add(f::mul(f::mul(f::mul(f::mul(f::mul(cplx(4.0, 0.0), wn3), alpha), ve2), sk), ca),
             f::mul(f::mul(n2, sa), ck)),
      f::mul(f::mul(f::mul(g, wn), sk), sa));
  return f::sub(f::sub(s2, thwg), f::div(f::mul(rho, num), den));
}

/// FUNC1_ZHAO as the callable f_zhao_v1 takes, usable on the host and in a kernel.
struct Func1Zhao {
  KOKKOS_INLINE_FUNCTION Kokkos::complex<double> operator()(const Kokkos::complex<double>& wn,
                                                            double sigma, double es, double nu,
                                                            double dice, double hice,
                                                            double depth) const {
    return func1_zhao(wn, sigma, es, nu, dice, hice, depth);
  }
};

}  // namespace ww::ic3
