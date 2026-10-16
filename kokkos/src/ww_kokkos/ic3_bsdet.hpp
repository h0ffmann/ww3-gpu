// kokkos/src/ww_kokkos/ic3_bsdet.hpp
// BSDET: determinant of an N x N DOUBLE COMPLEX matrix by Gaussian elimination with full
// pivoting, used for the 4 x 4 dispersion-relation determinant of the IC3 "V1" ice model.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic3md.F90:1611-1730 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, S. Zieger (from Zhao and Shen's Clarkson code).
// Evidence: kokkos/port_report/BSDET_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <complex>

#include <Kokkos_Core.hpp>

namespace ww::ic3 {

/// ABS of a DOUBLE COMPLEX: the Fortran object calls glibc's cabs. On the device Kokkos::abs is
/// a different algorithm: not bit-checked there.
KOKKOS_INLINE_FUNCTION double bsdet_cabs(const Kokkos::complex<double>& z) {
  KOKKOS_IF_ON_HOST((return std::abs(std::complex<double>(z.real(), z.imag()));))
  KOKKOS_IF_ON_DEVICE((return Kokkos::abs(z);))
}

/// BSDET(AA, N): DOUBLE COMPLEX AA(N,N), returns DOUBLE COMPLEX DET. `aa` is column major,
/// Fortran AA(I,J) at aa[(I-1) + (J-1)*n]. `mat` is a caller-provided work array of n*n
/// elements, the Fortran's automatic MAT(N,N) (malloc'd per call in the object); FUNC1_ZHAO and
/// DRFUN_DBLE_CHENG call it with N = 4. Requires n >= 1: the Fortran reads MAT(N,N) outside the
/// array for n <= 0. Every complex product and quotient is written out as gfortran -O3 emits it
/// (no __muldc3, Smith's quotient inlined), operands in the GIMPLE's order.
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> bsdet(const Kokkos::complex<double>* aa, int n,
                                                      Kokkos::complex<double>* mat) {
  const long ld = n;
  auto at = [&](int i, int j) -> Kokkos::complex<double>& {
    return mat[(static_cast<long>(i) - 1) + (static_cast<long>(j) - 1) * ld];
  };
  for (long e = 0; e < ld * ld; ++e) {
    mat[e] = aa[e];
  }
  double f_r = 1.0;
  double f_i = 0.0;
  double det_r = 1.0;
  double det_i = 0.0;
  int is = 0;
  int js = 0;
  for (int k = 1; k <= n - 1; ++k) {
    double q = 0.0;
    for (int i = k; i <= n; ++i) {
      for (int j = k; j <= n; ++j) {
        const double a = bsdet_cabs(at(i, j));
        if (a > q) {
          q = a;
          is = i;
          js = j;
        }
      }
    }
    // :1697-1700.
    if (q + 1.0 == 1.0) {
      return Kokkos::complex<double>(0.0, 0.0);
    }
    if (is != k) {
      f_r = -f_r;
      f_i = -f_i;
      for (int j = k; j <= n; ++j) {
        const Kokkos::complex<double> d = at(k, j);
        at(k, j) = at(is, j);
        at(is, j) = d;
      }
    }
    if (js != k) {
      f_r = -f_r;
      f_i = -f_i;
      for (int i = k; i <= n; ++i) {
        const Kokkos::complex<double> d = at(i, js);
        at(i, js) = at(i, k);
        at(i, k) = d;
      }
    }
    // DET = DET * MAT(K,K) (:1717).
    const double p_r = at(k, k).real();
    const double p_i = at(k, k).imag();
    {
      const double t_r = det_r * p_r - det_i * p_i;
      const double t_i = det_i * p_r + det_r * p_i;
      det_r = t_r;
      det_i = t_i;
    }
    for (int i = k + 1; i <= n; ++i) {
      // D = MAT(I,K) / MAT(K,K) (:1719), Smith's algorithm.
      const double a_r = at(i, k).real();
      const double a_i = at(i, k).imag();
      double d_r;
      double d_i;
      if (Kokkos::fabs(p_r) < Kokkos::fabs(p_i)) {
        const double ratio = p_r / p_i;
        const double denom = ratio * p_r + p_i;
        d_r = (ratio * a_r + a_i) / denom;
        d_i = (ratio * a_i - a_r) / denom;
      } else {
        const double ratio = p_i / p_r;
        const double denom = ratio * p_i + p_r;
        d_r = (a_r + a_i * ratio) / denom;
        d_i = (a_i - a_r * ratio) / denom;
      }
      for (int j = k + 1; j <= n; ++j) {
        // MAT(I,J) = MAT(I,J) - D * MAT(K,J) (:1721).
        const double m_r = at(k, j).real();
        const double m_i = at(k, j).imag();
        const double s_r = m_r * d_r - m_i * d_i;
        const double s_i = m_r * d_i + m_i * d_r;
        Kokkos::complex<double>& x = at(i, j);
        x = Kokkos::complex<double>(x.real() - s_r, x.imag() - s_i);
      }
    }
  }
  // DET = F * DET * MAT(N,N) (:1726), left to right.
  const double g_r = det_r * f_r - det_i * f_i;
  const double g_i = det_r * f_i + det_i * f_r;
  const double m_r = at(n, n).real();
  const double m_i = at(n, n).imag();
  return Kokkos::complex<double>(m_r * g_r - m_i * g_i, m_i * g_r + m_r * g_i);
}

}  // namespace ww::ic3
