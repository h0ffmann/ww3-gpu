// kokkos/src/ww_kokkos/gke_vmfunc.hpp
// VmFunc of W3GKEMD: the second-order coefficient V^(-)_{0,1,2} of Janssen (2009), U^(1) of
// Krasitskii (1994) up to 1/(2 pi), for three wavenumber vectors. Phase 1, translated, not
// regtested.
// WW3 heritage: WW3-noaa/model/src/w3gkemd.F90:319-354 at NOAA-EMC/WW3 03adda91;
// authors O. Gramstad, Q. Liu. Evidence: kokkos/port_report/VMFUNC_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "gke_qfunc.hpp"
#include "real.hpp"

namespace ww::gke {

namespace vmfunc_detail {
/// qr_grav, :198: 9.806, 0x411ce560 in the object.
inline constexpr Real kQrGrav = static_cast<Real>(9.806);
/// sqrt(1.0/32.0), :347, folded by gfortran to 0x3e3504f3.
inline constexpr Real kSqrt1o32 = 0x1.6a09e6p-3f;
#if defined(__GNUC__) && !defined(__clang__) && !defined(__CUDACC__)
static_assert(kSqrt1o32 == static_cast<Real>(__builtin_sqrtf(1.0f / 32.0f)));
#endif
}  // namespace vmfunc_detail

/// VmFunc(k0x, k0y, k1x, k1y, k2x, k2y), :319-354. qr_depth is the module variable qr_depth
/// (:165) as the Fortran reads it at the call; the object loads it once for the three QFunc.
/// Called out of line by TFunc (:482-486, :491-495, :503-504).
KOKKOS_INLINE_FUNCTION Real vmfunc(Real k0x, Real k0y, Real k1x, Real k1y, Real k2x, Real k2y,
                                   Real qr_depth) {
  using vmfunc_detail::kQrGrav;
  using vmfunc_detail::kSqrt1o32;
  // :341-343.
  const Real q0 = qfunc(k0x, k0y, qr_depth);
  const Real q1 = qfunc(k1x, k1y, qr_depth);
  const Real q2 = qfunc(k2x, k2y, qr_depth);
  // :347-350. Left to right, no contraction; the object computes g*q / (qa*qb) as written.
  return kSqrt1o32 *
         ((k0x * k1x + k0y * k1y - q0 * q1) *
              Kokkos::sqrt(Kokkos::sqrt(kQrGrav * q2 / (q0 * q1))) +
          (k0x * k2x + k0y * k2y - q0 * q2) *
              Kokkos::sqrt(Kokkos::sqrt(kQrGrav * q1 / (q0 * q2))) +
          (k1x * k2x + k1y * k2y + q1 * q2) *
              Kokkos::sqrt(Kokkos::sqrt(kQrGrav * q0 / (q1 * q2))));
}

}  // namespace ww::gke
