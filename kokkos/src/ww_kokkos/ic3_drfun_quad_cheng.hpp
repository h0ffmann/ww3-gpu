// kokkos/src/ww_kokkos/ic3_drfun_quad_cheng.hpp
// DRFUN_QUAD_CHENG: residual of the IC3 "Cheng" viscoelastic ice-layer dispersion relation at a
// complex wavenumber, the JUDGE == 0 branch of F_ZHAO_CHENG. COMPLEX(8) throughout, despite the name.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic3md.F90:2967-3088 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, S. Zieger, X. Zhao, S. Cheng.
// Evidence: kokkos/port_report/DRFUN_QUAD_CHENG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <complex>

#include <Kokkos_Core.hpp>

namespace ww::ic3 {

namespace drfun_quad_cheng_detail {

/// glibc's cexp, csqrt and cpow, which the Fortran object calls. On the host std::exp, std::sqrt
/// and std::pow(complex, complex) of std::complex<double> lower to __builtin_cexp, __builtin_csqrt
/// and __builtin_cpow. On a device Kokkos computes them with other algorithms: not bit-checked (⚠).
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> cexp(double re, double im) {
  KOKKOS_IF_ON_HOST((const std::complex<double> e = std::exp(std::complex<double>(re, im));
                     return Kokkos::complex<double>(e.real(), e.imag());))
  KOKKOS_IF_ON_DEVICE((return Kokkos::exp(Kokkos::complex<double>(re, im));))
}

KOKKOS_INLINE_FUNCTION Kokkos::complex<double> csqrt(double re, double im) {
  KOKKOS_IF_ON_HOST((const std::complex<double> e = std::sqrt(std::complex<double>(re, im));
                     return Kokkos::complex<double>(e.real(), e.imag());))
  KOKKOS_IF_ON_DEVICE((return Kokkos::sqrt(Kokkos::complex<double>(re, im));))
}

/// z**p for a REAL literal p: gfortran calls cpow(z, (p, 0.0)), it does not expand the power.
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> cpow(const Kokkos::complex<double>& z, double p) {
  KOKKOS_IF_ON_HOST((const std::complex<double> e = std::pow(std::complex<double>(z.real(), z.imag()),
                                                             std::complex<double>(p, 0.0));
                     return Kokkos::complex<double>(e.real(), e.imag());))
  KOKKOS_IF_ON_DEVICE((return Kokkos::pow(z, Kokkos::complex<double>(p, 0.0));))
}

/// (1.0, 0.0) / T, Smith's algorithm as gfortran inlines it with the zero numerator part kept.
KOKKOS_INLINE_FUNCTION void recip(double tr, double ti, double& qr, double& qi) {
  if (Kokkos::fabs(tr) < Kokkos::fabs(ti)) {
    const double ratio = tr / ti;
    const double denom = ti + tr * ratio;
    qr = (ratio + 0.0) / denom;
    qi = (ratio * 0.0 - 1.0) / denom;
  } else {
    const double ratio = ti / tr;
    const double denom = tr + ti * ratio;
    qr = (ratio * 0.0 + 1.0) / denom;
    qi = (0.0 - ratio) / denom;
  }
}

}  // namespace drfun_quad_cheng_detail

/// DRFUN_QUAD_CHENG(WN, SIGMA, ES, NU, DICE, HICE, DEPTH): DOUBLE COMPLEX WN, DOUBLE PRECISION
/// scalars, returns DOUBLE COMPLEX. The signature F_ZHAO_CHENG (ic3_f_zhao_cheng.hpp) takes as
/// `drfun_quad`. Every complex product and quotient is written out component by component as
/// gfortran -O3 emits it: a REAL operand promoted to (x, 0.0) keeps its products with 0.0 (NaN and
/// the sign of zero), and the quotients are Smith's algorithm inlined.
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> drfun_quad_cheng(const Kokkos::complex<double>& wn,
                                                                 double sigma, double es, double nu,
                                                                 double dice, double hice,
                                                                 double depth) {
  namespace d = drfun_quad_cheng_detail;
  // DBLE(GRAV), GRAV = 9.806 (REAL), constants.F90:62; DBLE(GRAV)**2. folds to its exact square.
  constexpr double grav = 0x1.39cacp+3;
  static_assert(grav == static_cast<double>(9.806f));
  constexpr double grav2 = 0x1.80a16a139p+6;
  static_assert(grav2 == grav * grav);
  // DBLE(DWAT), DWAT = 1000. (REAL), constants.F90:63.
  constexpr double dwat = 1000.0;

  // :3061. CMPLX without KIND= has the default (REAL) kind: both parts are rounded to float.
  const double ve_r = static_cast<double>(static_cast<float>(nu));
  const double ve_i = static_cast<double>(static_cast<float>(es / dice / sigma));
  const Kokkos::complex<double> ve(ve_r, ve_i);

  const double wr = wn.real();
  const double wi = wn.imag();
  // WN**2. (:3062), reused at :3063.
  const Kokkos::complex<double> w2 = d::cpow(wn, 2.0);
  const double w2r = w2.real();
  const double w2i = w2.imag();

  // :3062. SIGMA/VE, Smith's algorithm with SIGMA promoted to (SIGMA, 0.0).
  double q_r;
  double q_i;
  if (Kokkos::fabs(ve_r) < Kokkos::fabs(ve_i)) {
    const double ratio = ve_r / ve_i;
    const double denom = ve_i + ve_r * ratio;
    q_r = (sigma * ratio + 0.0) / denom;
    q_i = (ratio * 0.0 - sigma) / denom;
  } else {
    const double ratio = ve_i / ve_r;
    const double denom = ve_r + ve_i * ratio;
    q_r = (sigma + ratio * 0.0) / denom;
    q_i = (0.0 - sigma * ratio) / denom;
  }
  // * CMPLX(0.,1.D0) = (0.0, 1.0).
  const double m_r = q_r * 0.0 - q_i;
  const double m_i = q_i * 0.0 + q_r;
  const Kokkos::complex<double> alpha = d::csqrt(w2r - m_r, w2i - m_i);
  const double al_r = alpha.real();
  const double al_i = alpha.imag();

  // :3063. N = SIGMA + 2. * VE * WN**2. * (0, 1).
  const double t_r = ve_r * 2.0 - ve_i * 0.0;
  const double t_i = ve_r * 0.0 + ve_i * 2.0;
  const double u_r = w2r * t_r - w2i * t_i;
  const double u_i = w2r * t_i + w2i * t_r;
  const double v_r = u_r * 0.0 - u_i;
  const double v_i = u_r + u_i * 0.0;
  const Kokkos::complex<double> n(sigma + v_r, v_i + 0.0);

  // :3065-3067. TEMP = EXP(WN*HICE); SK, CK; (..)/2.D0 is the product by 0.5.
  const double wi0 = wi * 0.0;
  const double wr0 = wr * 0.0;
  const Kokkos::complex<double> e1 = d::cexp(hice * wr - wi0, hice * wi + wr0);
  double inv_r;
  double inv_i;
  d::recip(e1.real(), e1.imag(), inv_r, inv_i);
  double a_r = e1.real() - inv_r;
  double a_i = e1.imag() - inv_i;
  const double sk_r = (a_r + a_i * 0.0) * 0.5;
  const double sk_i = (a_i - a_r * 0.0) * 0.5;
  a_r = e1.real() + inv_r;
  a_i = e1.imag() + inv_i;
  const double ck_r = (a_r + a_i * 0.0) * 0.5;
  const double ck_i = (a_i - a_r * 0.0) * 0.5;

  // :3069-3071. TEMP = EXP(ALPHA*HICE); SA, CA.
  const Kokkos::complex<double> e2 = d::cexp(hice * al_r - al_i * 0.0, hice * al_i + al_r * 0.0);
  d::recip(e2.real(), e2.imag(), inv_r, inv_i);
  a_r = e2.real() - inv_r;
  a_i = e2.imag() - inv_i;
  const double sa_r = (a_r + a_i * 0.0) * 0.5;
  const double sa_i = (a_i - a_r * 0.0) * 0.5;
  a_r = e2.real() + inv_r;
  a_i = e2.imag() + inv_i;
  const double ca_r = (a_r + a_i * 0.0) * 0.5;
  const double ca_i = (a_i - a_r * 0.0) * 0.5;

  // :3073-3080. TH = TANH(WN*DEPTH), or 1 when REAL(WN*DEPTH) >= 18 or NaN; then TH*WN (:3086).
  double th_r = 1.0;
  double th_i = 0.0;
  double p_r;
  double p_i;
  const double kd_r = depth * wr - wi0;
  if (kd_r < 18.0) {
    const Kokkos::complex<double> e3 = d::cexp(kd_r, wr0 + depth * wi);
    d::recip(e3.real(), e3.imag(), inv_r, inv_i);
    const double b_r = e3.real() - inv_r;
    const double b_i = e3.imag() - inv_i;
    const double c_r = e3.real() + inv_r;
    const double c_i = e3.imag() + inv_i;
    if (Kokkos::fabs(c_r) < Kokkos::fabs(c_i)) {
      const double ratio = c_r / c_i;
      const double denom = c_i + c_r * ratio;
      th_r = (b_i + b_r * ratio) / denom;
      th_i = (b_i * ratio - b_r) / denom;
    } else {
      const double ratio = c_i / c_r;
      const double denom = c_r + c_i * ratio;
      th_r = (b_r + b_i * ratio) / denom;
      th_i = (b_i - b_r * ratio) / denom;
    }
    p_r = wr * th_r - wi * th_i;
    p_i = wi * th_r + wr * th_i;
  } else {
    // (1.0, 0.0) * WN with the products by 1.0 folded, as in the object.
    p_r = wr - wi0;
    p_i = wi + wr0;
  }

  // :3083-3084. J1.
  const double x_r = w2r * grav2 - w2i * 0.0;
  const double x_i = w2r * 0.0 + w2i * grav2;
  double y_r = sk_r * x_r - sk_i * x_i;
  double y_i = sk_i * x_r + sk_r * x_i;
  const double ss1_r = sa_r * y_r - sa_i * y_i;
  const double ss1_i = sa_i * y_r + sa_r * y_i;

  const Kokkos::complex<double> n4 = d::cpow(n, 4.0);
  const Kokkos::complex<double> ve4 = d::cpow(ve, 4.0);
  const double f_r = ve4.real() * 16.0 - ve4.imag() * 0.0;
  const double f_i = ve4.real() * 0.0 + ve4.imag() * 16.0;
  const Kokkos::complex<double> w6 = d::cpow(wn, 6.0);
  const double g_r = f_r * w6.real() - f_i * w6.imag();
  const double g_i = f_r * w6.imag() + f_i * w6.real();
  const Kokkos::complex<double> al2 = d::cpow(alpha, 2.0);
  const double h_r = g_r * al2.real() - g_i * al2.imag();
  const double h_i = g_r * al2.imag() + g_i * al2.real();
  const double s_r = n4.real() + h_r;
  const double s_i = n4.imag() + h_i;
  y_r = sk_r * s_r - sk_i * s_i;
  y_i = sk_i * s_r + sk_r * s_i;
  const double ss2_r = sa_r * y_r - sa_i * y_i;
  const double ss2_i = sa_i * y_r + sa_r * y_i;
  const double k1_r = ss1_r - ss2_r;
  const double k1_i = ss1_i - ss2_i;

  // 8.*WN**3.*ALPHA*VE**2.*N**2.*(CK*CA-1.); WN**3., VE**2. and N**2. are reused in J2.
  const Kokkos::complex<double> w3 = d::cpow(wn, 3.0);
  const double w3_r0 = w3.real() * 0.0;
  const double w3_i0 = w3.imag() * 0.0;
  const double o_r = w3.real() * 8.0 - w3_i0;
  const double o_i = w3_r0 + w3.imag() * 8.0;
  const double oa_r = al_r * o_r - al_i * o_i;
  const double oa_i = al_i * o_r + al_r * o_i;
  const Kokkos::complex<double> ve2 = d::cpow(ve, 2.0);
  const double ov_r = oa_r * ve2.real() - oa_i * ve2.imag();
  const double ov_i = oa_r * ve2.imag() + oa_i * ve2.real();
  const Kokkos::complex<double> n2 = d::cpow(n, 2.0);
  const double on_r = ov_r * n2.real() - ov_i * n2.imag();
  const double on_i = ov_r * n2.imag() + ov_i * n2.real();
  const double cc_r = ck_r * ca_r - ck_i * ca_i - 1.0;
  const double cc_i = ck_r * ca_i + ck_i * ca_r;
  const double k2_r = on_r * cc_r - on_i * cc_i;
  const double k2_i = on_r * cc_i + on_i * cc_r;
  const double k_r = k1_r - k2_r;
  const double k_i = k1_i - k2_i;
  const double rho = dice / dwat;
  const double j1_r = rho * k_r - k_i * 0.0;
  const double j1_i = rho * k_i + k_r * 0.0;

  // :3085-3086. J2 = 4.*WN**3.*ALPHA*VE**2.*SK*CA + N**2.*SA*CK - DBLE(GRAV)*WN*SK*SA.
  const double r_r = w3.real() * 4.0 - w3_i0;
  const double r_i = w3_r0 + w3.imag() * 4.0;
  const double ra_r = al_r * r_r - al_i * r_i;
  const double ra_i = al_i * r_r + al_r * r_i;
  const double rv_r = ve2.real() * ra_r - ve2.imag() * ra_i;
  const double rv_i = ve2.real() * ra_i + ve2.imag() * ra_r;
  const double rk_r = sk_r * rv_r - sk_i * rv_i;
  const double rk_i = sk_i * rv_r + sk_r * rv_i;
  const double rc_r = ca_r * rk_r - ca_i * rk_i;
  const double rc_i = ca_i * rk_r + ca_r * rk_i;
  const double ns_r = sa_r * n2.real() - sa_i * n2.imag();
  const double ns_i = sa_i * n2.real() + sa_r * n2.imag();
  const double nc_r = ck_r * ns_r - ck_i * ns_i;
  const double nc_i = ck_i * ns_r + ck_r * ns_i;
  const double z_r = rc_r + nc_r;
  const double z_i = rc_i + nc_i;
  const double gw_r = wr * grav - wi0;
  const double gw_i = wr0 + wi * grav;
  const double gk_r = sk_r * gw_r - sk_i * gw_i;
  const double gk_i = sk_i * gw_r + sk_r * gw_i;
  const double gs_r = sa_r * gk_r - sa_i * gk_i;
  const double gs_i = sa_i * gk_r + sa_r * gk_i;
  const double j2_r = z_r - gs_r;
  const double j2_i = z_i - gs_i;

  // :3086. FUNC1 = (SIGMA**2. - TH*WN*DBLE(GRAV))*J2 - TH*J1; SIGMA**2. folds to SIGMA*SIGMA.
  const double pg_r = p_r * grav - p_i * 0.0;
  const double pg_i = p_r * 0.0 + p_i * grav;
  const double l_r = sigma * sigma - pg_r;
  const double l_i = 0.0 - pg_i;
  const double lj_r = j2_r * l_r - j2_i * l_i;
  const double lj_i = j2_i * l_r + j2_r * l_i;
  const double tj_r = th_r * j1_r - th_i * j1_i;
  const double tj_i = th_r * j1_i + th_i * j1_r;
  return Kokkos::complex<double>(lj_r - tj_r, lj_i - tj_i);
}

}  // namespace ww::ic3
