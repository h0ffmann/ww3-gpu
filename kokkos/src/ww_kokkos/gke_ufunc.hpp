// kokkos/src/ww_kokkos/gke_ufunc.hpp
// UFunc of W3GKEMD: the intermediate quantity U_{1,2,3,4} (Janssen 2009; V_{0,1,2,3} of
// Krasitskii 1994) from which TFunc builds the third-order coefficient. Phase 1, translated,
// not regtested.
// WW3 heritage: WW3-noaa/model/src/w3gkemd.F90:358-396 at NOAA-EMC/WW3 03adda91;
// authors O. Gramstad, Q. Liu. Evidence: kokkos/port_report/UFUNC_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "gke_qfunc.hpp"
#include "real.hpp"

namespace ww::gke {

/// UFunc(k0x, k0y, k1x, k1y, k2x, k2y, k3x, k3y), :358-396, for four wavenumber vectors.
/// qr_depth is the module variable w3gkemd's qr_depth (:165), as QFunc reads it. Only TFunc
/// calls it (:473-478).
KOKKOS_INLINE_FUNCTION Real ufunc(Real k0x, Real k0y, Real k1x, Real k1y, Real k2x, Real k2y,
                                  Real k3x, Real k3y, Real qr_depth) {
  // :382-385.
  const Real q0 = qfunc(k0x, k0y, qr_depth);
  const Real q1 = qfunc(k1x, k1y, qr_depth);
  const Real q2 = qfunc(k2x, k2y, qr_depth);
  const Real q3 = qfunc(k3x, k3y, qr_depth);
  // :389-392, left to right as written; 1.0/16.0 is exact (0x3d800000 in the object).
  return (static_cast<Real>(1.0) / static_cast<Real>(16.0)) *
         Kokkos::sqrt(Kokkos::sqrt(q2 * q3 / (q0 * q1))) *
         (static_cast<Real>(2.0) * ((k0x * k0x + k0y * k0y) * q1 + (k1x * k1x + k1y * k1y) * q0) -
          q0 * q1 *
              (qfunc(k0x + k2x, k0y + k2y, qr_depth) + qfunc(k1x + k2x, k1y + k2y, qr_depth) +
               qfunc(k0x + k3x, k0y + k3y, qr_depth) + qfunc(k1x + k3x, k1y + k3y, qr_depth)));
}

}  // namespace ww::gke
