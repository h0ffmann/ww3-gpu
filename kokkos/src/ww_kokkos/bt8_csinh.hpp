// kokkos/src/ww_kokkos/bt8_csinh.hpp
// CSINH of W3SBT8MD: complex hyperbolic sine of a COMPLEX (kind 4) argument.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sbt8md.F90:504-511 at NOAA-EMC/WW3 03adda91;
// authors M. Orzech, W. E. Rogers (module header; the routine says "NA").
// Evidence: kokkos/port_report/CSINH_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Complex.hpp>
#include <Kokkos_Core.hpp>

namespace ww::bt8 {

/// CSINH(C,CS): CS = CMPLX(SINH(X)*COS(Y), SIN(Y)*COSH(X)), X = REAL(C), Y = AIMAG(C).
/// X and Y are implicit default REAL (the module has no IMPLICIT NONE).
KOKKOS_INLINE_FUNCTION void csinh(const Kokkos::complex<float>& c, Kokkos::complex<float>& cs) {
  const float x = c.real();
  const float y = c.imag();
  // gfortran -O3 fuses SIN(Y) and COS(Y) into one sincosf call; g++ -O3 does the same here.
  const float re = Kokkos::sinh(x) * Kokkos::cos(y);
  const float im = Kokkos::sin(y) * Kokkos::cosh(x);
  cs = Kokkos::complex<float>(re, im);
}

}  // namespace ww::bt8
