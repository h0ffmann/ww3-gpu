// kokkos/src/ww_kokkos/ic3_func0_zhao.hpp
// FUNC0_ZHAO: residual SIGMA**2 - G*K*TANH(K*D) of the open-water dispersion relation at a
// complex wavenumber, the JUDGE == 0 branch of F_ZHAO_V1 (Muller root finder, IC3 "V1").
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic3md.F90:1372-1462 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, S. Zieger (from Zhao and Shen's Clarkson code).
// Evidence: kokkos/port_report/FUNC0_ZHAO_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <complex>

#include <Kokkos_Core.hpp>

namespace ww::ic3 {

/// glibc's cexp, which the Fortran object calls (__builtin_cexp). On the device Kokkos::exp is a
/// different algorithm: not bit-checked there.
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> func0_zhao_cexp(double re, double im) {
  KOKKOS_IF_ON_HOST((const std::complex<double> e = std::exp(std::complex<double>(re, im));
                     return Kokkos::complex<double>(e.real(), e.imag());))
  KOKKOS_IF_ON_DEVICE((return Kokkos::exp(Kokkos::complex<double>(re, im));))
}

/// FUNC0_ZHAO(WN, SIGMA, DEPTH): DOUBLE COMPLEX WN, DOUBLE PRECISION SIGMA, DEPTH, returns
/// DOUBLE COMPLEX. Every complex product, quotient and difference is written out component by
/// component as gfortran -O3 emits it: a real operand promoted to (x, 0) keeps its products with
/// 0.0 (they carry NaN and the sign of zero), SIGMA**2. - z is (s2 - z.re, 0.0 - z.im), and the
/// quotient is Smith's algorithm inlined, not Kokkos::complex's operator/.
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> func0_zhao(const Kokkos::complex<double>& wn,
                                                           double sigma, double depth) {
  // DBLE(GRAV), GRAV = 9.806 (REAL) in constants.F90:62.
  constexpr double grav = 0x1.39cacp+3;
  static_assert(grav == static_cast<double>(9.806f));
  const double wr = wn.real();
  const double wi = wn.imag();
  // WN*DEPTH (:1454), DEPTH promoted to (DEPTH, 0).
  const double wi0 = wi * 0.0;
  const double wr0 = wr * 0.0;
  const double kd_r = depth * wr - wi0;
  // SIGMA**2. folds to SIGMA*SIGMA (:1457, :1459).
  const double s2 = sigma * sigma;
  if (kd_r <= 4.0) {
    const double kd_i = wr0 + depth * wi;
    // :1455-1456.
    const Kokkos::complex<double> em = func0_zhao_cexp(-kd_r, -kd_i);
    const Kokkos::complex<double> ep = func0_zhao_cexp(kd_r, kd_i);
    const double a_r = ep.real() - em.real();
    const double a_i = ep.imag() - em.imag();
    const double b_r = em.real() + ep.real();
    const double b_i = em.imag() + ep.imag();
    double th_r;
    double th_i;
    if (Kokkos::fabs(b_r) < Kokkos::fabs(b_i)) {
      const double ratio = b_r / b_i;
      const double denom = b_i + b_r * ratio;
      th_r = (a_i + a_r * ratio) / denom;
      th_i = (a_i * ratio - a_r) / denom;
    } else {
      const double ratio = b_i / b_r;
      const double denom = b_r + b_i * ratio;
      th_r = (a_r + a_i * ratio) / denom;
      th_i = (a_i - a_r * ratio) / denom;
    }
    // TH * WN, then * (GRAV, 0) (:1457).
    const double p_r = wr * th_r - wi * th_i;
    const double p_i = wr * th_i + wi * th_r;
    const double q_r = p_r * grav - p_i * 0.0;
    const double q_i = p_r * 0.0 + p_i * grav;
    return Kokkos::complex<double>(s2 - q_r, 0.0 - q_i);
  }
  // WN * (GRAV, 0) (:1459).
  const double q_r = wr * grav - wi0;
  const double q_i = wr0 + wi * grav;
  return Kokkos::complex<double>(s2 - q_r, 0.0 - q_i);
}

}  // namespace ww::ic3
