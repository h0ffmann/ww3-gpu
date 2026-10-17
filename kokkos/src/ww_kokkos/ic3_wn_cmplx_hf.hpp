// kokkos/src/ww_kokkos/ic3_wn_cmplx_hf.hpp
// WN_CMPLX_HF: complex wavenumber in ice (IC3 "V1", Wang and Shen 2010) for a short period,
// continued from the previous frequency's root in steps of 0.1 s of period.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic3md.F90:984-1102 at NOAA-EMC/WW3 03adda91;
// authors H. Shen, E. Rogers (from Zhao and Shen's WN_CMPLXA).
// Evidence: kokkos/port_report/WN_CMPLX_HF_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::ic3 {

namespace wn_cmplx_hf_detail {

/// INT() of a DOUBLE PRECISION as cvttsd2si: NaN and out of range give INT_MIN.
KOKKOS_INLINE_FUNCTION int fint_d(double x) {
  return (x > -2147483649.0 && x < 2147483648.0) ? static_cast<int>(x)
                                                  : static_cast<int>(0x80000000u);
}

/// An opaque copy (PAREN_EXPR) for g++'s SLP vectoriser; no arithmetic. See the loop below.
KOKKOS_INLINE_FUNCTION double slp_barrier(double x) {
// nvcc's front end reports GCC's builtin and cannot parse it, in its host pass too; under
// nvcc the barrier is absent (the CUDA build is not regtested).
#if defined(__has_builtin) && !defined(__CUDACC__)
#if __has_builtin(__builtin_assoc_barrier)
  return __builtin_assoc_barrier(x);
#else
  return x;
#endif
#else
  return x;
#endif
}

}  // namespace wn_cmplx_hf_detail

/// WN_CMPLX_HF(SIGMA, WN_O, ES, NU, DICE, HICE, DEPTH, SIGMA_LAST, WN_LAST): DOUBLE PRECISION
/// scalars, DOUBLE COMPLEX WN_LAST, returns DOUBLE COMPLEX, here written to `wn`.
/// CMPLX_ROOT_MULLER_V1 (w3sic3md.F90:1106-1276) has no header yet: `root` stands for it, called
/// as root(x0, x1, x2, judge, sigma, es, nu, dice, hice, depth, p3) and returning 0, or the
/// EXTCDE code where the Fortran stops (:1244, :1268); a nonzero code is returned at once.
/// Every product of a REAL literal and a DOUBLE COMPLEX is the full complex product with
/// (literal, 0.0), as gfortran -O3 emits it: the products with 0.0 carry NaN and the sign of zero.
template <class RootMuller>
KOKKOS_INLINE_FUNCTION int wn_cmplx_hf(double sigma, double wn_o, double es, double nu,
                                       double dice, double hice, double depth,
                                       double sigma_last,
                                       const Kokkos::complex<double>& wn_last,
                                       Kokkos::complex<double>& wn, const RootMuller& root) {
  // DBLE(TPI), TPI = 2.0 * PI (REAL), PI = 3.141592653589793 (constants.F90:72-73).
  constexpr double tpi = 0x1.921fb6p+2;
  static_assert(tpi == static_cast<double>(2.0f * 3.141592653589793f));
  // 0.001, 0.90, 1.1 are REAL literals promoted to DOUBLE (:1084, :1087, :1089).
  constexpr double hmin = 0x1.0624dep-10;
  static_assert(hmin == static_cast<double>(0.001f));
  constexpr double c09 = 0x1.ccccccp-1;
  static_assert(c09 == static_cast<double>(0.90f));
  constexpr double c11 = 0x1.19999ap+0;
  static_assert(c11 == static_cast<double>(1.1f));

  // :1084-1085. CMPLX(WN_O, 0.) has the default (REAL) kind: WN_O is rounded to float.
  if (hice < hmin) {
    wn = Kokkos::complex<double>(static_cast<double>(static_cast<float>(wn_o)), 0.0);
    return 0;
  }
  const double t = tpi / sigma;        // :1080
  const double ts = tpi / sigma_last;  // :1081
  const double dt = ts - t;
  const int nsub = wn_cmplx_hf_detail::fint_d(dt * 10.0);  // :1082

  const double lr = wn_last.real();
  const double li = wn_last.imag();
  // :1087-1089. In this vectorised block the Fortran's imaginary part of X0 is li*0.9 + lr*0.0.
  Kokkos::complex<double> x0(lr * c09 - li * 0.0, li * c09 + lr * 0.0);
  Kokkos::complex<double> x1(lr, li);
  Kokkos::complex<double> x2(lr * c11 - li * 0.0, lr * 0.0 + li * c11);
  Kokkos::complex<double> p3;
  // :1090-1091.
  int ierr = root(x0, x1, x2, 1, tpi / ts, es, nu, dice, hice, depth, p3);
  if (ierr != 0) return ierr;
  for (int i = 1; i <= nsub; ++i) {
    const double wr = p3.real();
    const double wi = p3.imag();
    // :1093-1095. In the object both imaginary parts are addsd with wi*c as the first operand
    // (the NaN kept when both addends are NaN). g++ pairs the lanes of X0 that way from this
    // order; without the barriers it pairs those of X2 as (wr*0.0) + (wi*1.1) in vaddsubpd.
    using wn_cmplx_hf_detail::slp_barrier;
    x0 = Kokkos::complex<double>(wr * c09 - wi * 0.0, wi * c09 + wr * 0.0);
    x1 = Kokkos::complex<double>(wr, wi);
    x2 = Kokkos::complex<double>(wr * c11 - slp_barrier(wi * 0.0),
                                 wi * c11 + slp_barrier(wr * 0.0));
    // :1096. REAL(NSUB) and REAL(I) are REAL (float), then promoted.
    const double tt = ts - dt / static_cast<double>(static_cast<float>(nsub)) *
                               static_cast<double>(static_cast<float>(i));
    // :1097-1098.
    ierr = root(x0, x1, x2, 1, tpi / tt, es, nu, dice, hice, depth, p3);
    if (ierr != 0) return ierr;
  }
  wn = p3;
  return 0;
}

}  // namespace ww::ic3
