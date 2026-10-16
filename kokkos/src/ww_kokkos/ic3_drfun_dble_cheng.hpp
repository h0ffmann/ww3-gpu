// kokkos/src/ww_kokkos/ic3_drfun_dble_cheng.hpp
// DRFUN_DBLE_CHENG: residual of the IC3 "Cheng" viscoelastic ice-layer dispersion relation at a
// complex wavenumber, the JUDGE > 0 branch of F_ZHAO_CHENG (4 x 4 determinant or closed form).
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic3md.F90:2812-2962 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, S. Zieger, X. Zhao, S. Cheng.
// Evidence: kokkos/port_report/DRFUN_DBLE_CHENG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "ic3_bsdet.hpp"
#include "ic3_drfun_quad_cheng.hpp"

namespace ww::ic3 {

/// DRFUN_DBLE_CHENG(WN, SIGMA, ES, NU, DICE, HICE, DEPTH, JUDGE): DOUBLE COMPLEX WN, DOUBLE
/// PRECISION scalars, returns DOUBLE COMPLEX. The `drfun_dble` callable that F_ZHAO_CHENG
/// (ic3_f_zhao_cheng.hpp) takes. AA(4,4) and BSDET's automatic MAT(4,4) are local arrays here.
/// Every complex product and quotient is written out component by component as gfortran -O3
/// emits it (drfun_quad_cheng's conventions; its cexp/csqrt/cpow/recip helpers are reused).
/// Trap: when JUDGE is neither 1 nor 2 and the BSDET branch (:2924) is not taken, the Fortran
/// never sets FUNC1 (:2955-2959); the -O3 object returns whatever its registers hold.
/// This returns (0.0, 0.0) there. Every caller in WW3 passes JUDGE = 1, 2 or 3.
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> drfun_dble_cheng(const Kokkos::complex<double>& wn,
                                                                 double sigma, double es, double nu,
                                                                 double dice, double hice,
                                                                 double depth, int judge) {
  namespace d = drfun_quad_cheng_detail;
  // DBLE(GRAV), GRAV = 9.806 (REAL), constants.F90:62; DBLE(GRAV)**2. folds to its exact square.
  constexpr double grav = 0x1.39cacp+3;
  static_assert(grav == static_cast<double>(9.806f));
  constexpr double grav2 = 0x1.80a16a139p+6;
  static_assert(grav2 == grav * grav);
  // DBLE(DWAT), DWAT = 1000. (REAL), constants.F90:63.
  constexpr double dwat = 1000.0;
  // 1.e-20 at :2956 is a default REAL literal, widened to double.
  constexpr double tiny = 0x1.79ca1p-67;
  static_assert(tiny == static_cast<double>(1.0e-20f));

  // :2904. CMPLX without KIND= has the default (REAL) kind: both parts are rounded to float.
  const double ve_r = static_cast<double>(static_cast<float>(nu));
  const double ve_i = static_cast<double>(static_cast<float>(es / dice / sigma));
  const Kokkos::complex<double> ve(ve_r, ve_i);

  const double wr = wn.real();
  const double wi = wn.imag();
  const double wi0 = wi * 0.0;
  const double wr0 = wr * 0.0;
  // WN**2. (:2905), reused at :2906 and in AA and J1.
  const Kokkos::complex<double> w2 = d::cpow(wn, 2.0);
  const double w2r = w2.real();
  const double w2i = w2.imag();

  // :2905. SIGMA/VE, Smith's algorithm with SIGMA promoted to (SIGMA, 0.0).
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
  const double m0_r = q_r * 0.0 - q_i;
  const double m0_i = q_i * 0.0 + q_r;
  const Kokkos::complex<double> alpha = d::csqrt(w2r - m0_r, w2i - m0_i);
  const double al_r = alpha.real();
  const double al_i = alpha.imag();

  // :2906. N = SIGMA + 2. * VE * WN**2. * (0, 1).
  const double t_r = ve_r * 2.0 - ve_i * 0.0;
  const double t_i = ve_r * 0.0 + ve_i * 2.0;
  const double u_r = w2r * t_r - w2i * t_i;
  const double u_i = w2r * t_i + w2i * t_r;
  const double v_r = u_r * 0.0 - u_i;
  const double v_i = u_r + u_i * 0.0;
  const double n_r = sigma + v_r;
  const double n_i = v_i + 0.0;

  // :2908-2910. TEMP = EXP(WN*HICE); SK, CK; (..)/2.D0 is the product by 0.5.
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

  // :2912-2914. TEMP = EXP(ALPHA*HICE); SA, CA.
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

  // :2916-2922. TH = TANH(WN*DEPTH), or 1 when REAL(WN*DEPTH) >= 18 or NaN.
  double th_r = 1.0;
  double th_i = 0.0;
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
  }

  // :2924. 1.E5 is a REAL literal, exactly 100000.
  if ((es >= 1.0e5 && judge != 2) || judge == 3) {
    // :2925. L = 2 * WN * ALPHA * SIGMA * VE, left to right.
    const double l1_r = wr * 2.0 - wi0;
    const double l1_i = wr0 + wi * 2.0;
    const double l2_r = al_r * l1_r - al_i * l1_i;
    const double l2_i = al_i * l1_r + al_r * l1_i;
    const double l3_r = sigma * l2_r - l2_i * 0.0;
    const double l3_i = sigma * l2_i + l2_r * 0.0;
    const double l_r = ve_r * l3_r - ve_i * l3_i;
    const double l_i = ve_i * l3_r + ve_r * l3_i;

    // :2926-2927. M = (DWAT/DICE - 1) * GRAV * WN - DWAT/DICE * SIGMA**2 / TH.
    const double rho = dwat / dice;
    const double g1 = (rho - 1.0) * grav;
    const double mw_r = g1 * wr - wi0;
    const double mw_i = wr0 + g1 * wi;
    const double s2 = rho * (sigma * sigma);
    double mq_r;
    double mq_i;
    if (Kokkos::fabs(th_r) < Kokkos::fabs(th_i)) {
      const double ratio = th_r / th_i;
      const double denom = th_i + th_r * ratio;
      mq_r = (s2 * ratio + 0.0) / denom;
      mq_i = (ratio * 0.0 - s2) / denom;
    } else {
      const double ratio = th_i / th_r;
      const double denom = th_r + th_i * ratio;
      mq_r = (s2 + ratio * 0.0) / denom;
      mq_i = (0.0 - s2 * ratio) / denom;
    }
    const double m_r = mw_r - mq_r;
    const double m_i = mw_i - mq_i;

    // AA(I,J) at aa[(I-1) + (J-1)*4], column major as BSDET reads it.
    Kokkos::complex<double> aa[16];
    Kokkos::complex<double> mat[16];
    // 2 * CMPLX(0.,1.) * WN**2. and 2. * CMPLX(0.,1.) * WN**2.: (0, 2) * WN**2.
    const double iw_r = w2r * 0.0 - w2i * 2.0;
    const double iw_i = w2r * 2.0 + w2i * 0.0;
    const Kokkos::complex<double> al2 = d::cpow(alpha, 2.0);
    const double s_r = al2.real() + w2r;
    const double s_i = al2.imag() + w2i;
    aa[0] = Kokkos::complex<double>(0.0, 0.0);
    aa[4] = Kokkos::complex<double>(iw_r, iw_i);
    aa[8] = Kokkos::complex<double>(s_r, s_i);
    aa[12] = Kokkos::complex<double>(0.0, 0.0);

    // :2933-2935. N * SIGMA; -WN * GRAV is -(WN*GRAV); CMPLX(0.,1.) * WN * GRAV.
    const double ns_r = sigma * n_r - n_i * 0.0;
    const double ns_i = sigma * n_i + n_r * 0.0;
    aa[1] = Kokkos::complex<double>(ns_r, ns_i);
    aa[5] = Kokkos::complex<double>(-(wr * grav - wi0), -(wr0 + wi * grav));
    const double jw_r = wr0 - wi;
    const double jw_i = wr + wi0;
    aa[9] = Kokkos::complex<double>(jw_r * grav - jw_i * 0.0, jw_r * 0.0 + jw_i * grav);
    aa[13] = Kokkos::complex<double>(l_r, l_i);

    // :2938-2941.
    aa[2] = Kokkos::complex<double>(-(sk_r * iw_r - sk_i * iw_i), -(sk_i * iw_r + sk_r * iw_i));
    aa[6] = Kokkos::complex<double>(ck_r * iw_r - ck_i * iw_i, ck_i * iw_r + ck_r * iw_i);
    aa[10] = Kokkos::complex<double>(ca_r * s_r - ca_i * s_i, ca_i * s_r + ca_r * s_i);
    aa[14] = Kokkos::complex<double>(-(sa_r * s_r - sa_i * s_i), -(sa_i * s_r + sa_r * s_i));

    // :2943-2944. -N*SIGMA*SK + M*CK is folded to M*CK - N*SIGMA*SK.
    aa[3] = Kokkos::complex<double>((ck_r * ns_r - ck_i * ns_i) - (sk_r * m_r - sk_i * m_i),
                                    (ck_i * ns_r + ck_r * ns_i) - (sk_r * m_i + sk_i * m_r));
    aa[7] = Kokkos::complex<double>((ck_r * m_r - ck_i * m_i) - (sk_r * ns_r - sk_i * ns_i),
                                    (ck_r * m_i + ck_i * m_r) - (sk_i * ns_r + sk_r * ns_i));
    // :2945-2946. CMPLX(0.,1.) * M; -X*CA is -(X*CA).
    const double im_r = m_r * 0.0 - m_i;
    const double im_i = m_r + m_i * 0.0;
    aa[11] = Kokkos::complex<double>(-(ca_r * im_r - ca_i * im_i) - (sa_r * l_r - sa_i * l_i),
                                     -(ca_i * im_r + ca_r * im_i) - (sa_r * l_i + sa_i * l_r));
    aa[15] = Kokkos::complex<double>((sa_r * im_r - sa_i * im_i) + (ca_r * l_r - ca_i * l_i),
                                     (sa_i * im_r + sa_r * im_i) + (ca_r * l_i + ca_i * l_r));

    // :2948.
    return bsdet(aa, 4, mat);
  }

  // :2950-2952. J1, as in drfun_quad_cheng.
  const double x_r = w2r * grav2 - w2i * 0.0;
  const double x_i = w2r * 0.0 + w2i * grav2;
  double y_r = sk_r * x_r - sk_i * x_i;
  double y_i = sk_i * x_r + sk_r * x_i;
  const double ss1_r = sa_r * y_r - sa_i * y_i;
  const double ss1_i = sa_i * y_r + sa_r * y_i;

  const Kokkos::complex<double> n = Kokkos::complex<double>(n_r, n_i);
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
  const double sn_r = n4.real() + h_r;
  const double sn_i = n4.imag() + h_i;
  y_r = sk_r * sn_r - sk_i * sn_i;
  y_i = sk_i * sn_r + sk_r * sn_i;
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

  // :2953-2954. J2 = 4.*WN**3.*ALPHA*VE**2.*SK*CA + N**2.*SA*CK - DBLE(GRAV)*WN*SK*SA.
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
  const double nsa_r = sa_r * n2.real() - sa_i * n2.imag();
  const double nsa_i = sa_i * n2.real() + sa_r * n2.imag();
  const double nc_r = ck_r * nsa_r - ck_i * nsa_i;
  const double nc_i = ck_i * nsa_r + ck_r * nsa_i;
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

  // (SIGMA**2. - TH*WN*DBLE(GRAV)); SIGMA**2. folds to SIGMA*SIGMA.
  const double p_r = wr * th_r - wi * th_i;
  const double p_i = wi * th_r + wr * th_i;
  const double pg_r = p_r * grav - p_i * 0.0;
  const double pg_i = p_r * 0.0 + p_i * grav;
  const double e_r = sigma * sigma - pg_r;
  const double e_i = 0.0 - pg_i;
  const double tj_r = th_r * j1_r - th_i * j1_i;
  const double tj_i = th_r * j1_i + th_i * j1_r;

  if (judge == 2) {
    // :2966. FUNC1 = (..) - TH*J1/(J2+1.e-20), Smith's quotient.
    const double dr = j2_r + tiny;
    const double di = j2_i + 0.0;
    double qq_r;
    double qq_i;
    if (Kokkos::fabs(dr) < Kokkos::fabs(di)) {
      const double ratio = dr / di;
      const double denom = di + dr * ratio;
      qq_r = (tj_i + tj_r * ratio) / denom;
      qq_i = (tj_i * ratio - tj_r) / denom;
    } else {
      const double ratio = di / dr;
      const double denom = dr + di * ratio;
      qq_r = (tj_r + tj_i * ratio) / denom;
      qq_i = (tj_i - tj_r * ratio) / denom;
    }
    return Kokkos::complex<double>(e_r - qq_r, e_i - qq_i);
  }
  if (judge == 1) {
    // :2968. FUNC1 = (..)*J2 - TH*J1.
    const double ej_r = j2_r * e_r - j2_i * e_i;
    const double ej_i = j2_i * e_r + j2_r * e_i;
    return Kokkos::complex<double>(ej_r - tj_r, ej_i - tj_i);
  }
  return Kokkos::complex<double>(0.0, 0.0);
}

}  // namespace ww::ic3
