// kokkos/src/ww_kokkos/ic5_cmplx_cosh.hpp
// CMPLX_COSH of W3SIC5MD: complex hyperbolic cosine of a COMPLEX(8), (EXP(X) + EXP(-X)) * 0.5.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic5md.F90:2050-2144 at NOAA-EMC/WW3 03adda91;
// author Q. Liu (module authors Q. Liu, E. Rogers).
// Evidence: kokkos/port_report/CMPLX_COSH_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <complex>

#include <Kokkos_Core.hpp>

namespace ww::ic5 {

namespace detail {

/// EXP of a COMPLEX(8) is glibc's cexp (w3sic5md.o calls `cexp`); on the host std::exp of
/// std::complex<double> lowers to the same __builtin_cexp. Kokkos::exp(complex) computes
/// exp(re)*(cos(im), sin(im)) itself and need not give cexp's bits; a device has no cexp (⚠).
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> cmplx_cosh_cexp(const Kokkos::complex<double>& z) {
  KOKKOS_IF_ON_HOST((const std::complex<double> e = std::exp(std::complex<double>(z.real(), z.imag()));
                     return Kokkos::complex<double>(e.real(), e.imag());))
  KOKKOS_IF_ON_DEVICE((return Kokkos::exp(z);))
}

}  // namespace detail

/// CMPLX_COSH(X) = (EXP(X) + EXP(-X)) * 0.5, X COMPLEX(KDPC) = COMPLEX(8) (w3sic5md.F90:143).
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> cmplx_cosh(const Kokkos::complex<double>& x) {
  // EXP(-X) is called first on purpose: g++ then adds with EXP(X) as first source, as the
  // Fortran's addsd does, so a NaN in both parts keeps EXP(X)'s payload and sign.
  // -X negates both parts (xorpd), not 0 - X.
  const Kokkos::complex<double> em = detail::cmplx_cosh_cexp(Kokkos::complex<double>(-x.real(), -x.imag()));
  const Kokkos::complex<double> ep = detail::cmplx_cosh_cexp(x);
  const double sr = ep.real() + em.real();
  const double si = ep.imag() + em.imag();
  // :2140, the default REAL 0.5 becomes the COMPLEX(8) (0.5, 0.0) and gfortran multiplies in full
  // (Fortran complex rules, no NaN recovery): the products by 0.0 are kept, Inf*0 gives NaN.
  return Kokkos::complex<double>(sr * 0.5 - si * 0.0, sr * 0.0 + si * 0.5);
}

}  // namespace ww::ic5
