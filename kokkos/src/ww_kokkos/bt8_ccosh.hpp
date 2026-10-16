// kokkos/src/ww_kokkos/bt8_ccosh.hpp
// CCOSH of W3SBT8MD: complex hyperbolic cosine of a COMPLEX(4), as the module writes it.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sbt8md.F90:523-530 at NOAA-EMC/WW3 03adda91;
// module authors M. Orzech, W. E. Rogers (the routine itself says "NA").
// Evidence: kokkos/port_report/CCOSH_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::bt8 {

/// CCOSH(C,CC): CC = CMPLX(COSH(X)*COS(Y), SIN(Y)*SINH(X)), X = REAL(C), Y = AIMAG(C).
/// X and Y are implicit default REAL (the module has no IMPLICIT NONE), so float.
/// gfortran -O3 fuses SIN(Y) and COS(Y) into one sincosf; the host build here does too.
KOKKOS_INLINE_FUNCTION void ccosh(const Kokkos::complex<float>& c, Kokkos::complex<float>& cc) {
  const float x = c.real();  // :526
  const float y = c.imag();  // :527
  // :528, operand order as written: COSH(X)*COS(Y), SIN(Y)*SINH(X).
  cc = Kokkos::complex<float>(Kokkos::cosh(x) * Kokkos::cos(y), Kokkos::sin(y) * Kokkos::sinh(x));
}

}  // namespace ww::bt8
