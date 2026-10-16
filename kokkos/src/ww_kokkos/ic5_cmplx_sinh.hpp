// kokkos/src/ww_kokkos/ic5_cmplx_sinh.hpp
// CMPLX_SINH of W3SIC5MD: complex hyperbolic sine of a COMPLEX(KDPC) (double) argument,
// (EXP(X) - EXP(-X)) * 0.5. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic5md.F90:1940-2034 at NOAA-EMC/WW3 03adda91;
// author Q. Liu. Evidence: kokkos/port_report/CMPLX_SINH_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <complex>

#include <Kokkos_Complex.hpp>
#include <Kokkos_Core.hpp>

namespace ww::ic5 {

namespace cmplx_sinh_detail {
/// The Fortran's EXP of a COMPLEX(8) is glibc's cexp (w3sic5md.o calls `cexp`); on the host
/// std::exp of std::complex<double> lowers to the same __builtin_cexp. ⚠ A device has no
/// glibc cexp: Kokkos::exp (exp(re)*(cos(im), sin(im))) is not bit for bit with it.
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> cexp(const Kokkos::complex<double>& z) {
  KOKKOS_IF_ON_HOST((const std::complex<double> e = std::exp(std::complex<double>(z.real(), z.imag()));
                     return Kokkos::complex<double>(e.real(), e.imag());))
  KOKKOS_IF_ON_DEVICE((return Kokkos::exp(z);))
}
}  // namespace cmplx_sinh_detail

/// CMPLX_SINH(X), :2030. -X negates both parts; the product by the default REAL 0.5 is a
/// full complex product by (0.5, 0.0) without NaN recovery (gfortran's Fortran complex
/// rules), so the x*0.0 terms stay: they turn an infinite part into NaN.
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> cmplx_sinh(const Kokkos::complex<double>& x) {
  const Kokkos::complex<double> ep = cmplx_sinh_detail::cexp(x);
  const Kokkos::complex<double> em =
      cmplx_sinh_detail::cexp(Kokkos::complex<double>(-x.real(), -x.imag()));
  const double dr = ep.real() - em.real();
  const double di = ep.imag() - em.imag();
  const double re = dr * 0.5 - di * 0.0;
  const double im = dr * 0.0 + di * 0.5;
  return Kokkos::complex<double>(re, im);
}

}  // namespace ww::ic5
