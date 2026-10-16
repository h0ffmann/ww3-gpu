// kokkos/src/ww_kokkos/profs_givens.hpp
// GIVENS: the Givens rotation (c, s) that maps (x, y) to (sqrt(x**2 + y**2), 0), in
// REAL(8), of the SPARSKIT solvers inside W3PROFSMD. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:2461-2500 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin. Evidence: kokkos/port_report/GIVENS_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// GIVENS. All four dummies are REAL*8 INTENT(INOUT) by default: on return x holds
/// |x*y| as the Fortran computes it at :2495 (y is overwritten in the |y| <= |x| branch).
KOKKOS_INLINE_FUNCTION void givens(double& x, double& y, double& c, double& s) {
  const double zero = 0.0;
  const double one = 1.0;
  double t;

  if (x == zero && y == zero) {
    // :2473-2475.
    c = one;
    s = zero;
  } else if (Kokkos::fabs(y) > Kokkos::fabs(x)) {
    // :2476-2480. SIGN(a, b) on REAL is copysign in the object.
    t = x / y;
    x = Kokkos::sqrt(one + t * t);
    s = Kokkos::copysign(one / x, y);
    c = t * s;
  } else if (Kokkos::fabs(y) <= Kokkos::fabs(x)) {
    // :2481-2485.
    t = y / x;
    y = Kokkos::sqrt(one + t * t);
    c = Kokkos::copysign(one / y, x);
    s = t * c;
  } else {
    // :2486-2493. Reached only when x or y is a NaN.
    x = zero;
    y = zero;
    c = one;
    s = zero;
  }
  // :2495.
  x = Kokkos::fabs(x * y);
}

}  // namespace ww::profs
