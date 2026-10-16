// kokkos/src/ww_kokkos/gke_vpfunc.hpp
// VpFunc of W3GKEMD: the second-order coefficient V^{(+)}_{1,2,3} of the Zakharov kernel
// (Janssen 2009, appendix A; U^{(3)}_{0,1,2} of Krasitskii 1994 divided by 2 pi).
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3gkemd.F90:275-315 at NOAA-EMC/WW3 03adda91;
// authors O. Gramstad, Q. Liu. Evidence: kokkos/port_report/VPFUNC_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "gke_qfunc.hpp"
#include "real.hpp"

namespace ww::gke {

namespace vpfunc_detail {
/// qr_grav, :198: 9.806, 0x411ce560 in the object (module constant, not constants.F90's GRAV).
inline constexpr Real kQrGrav = static_cast<Real>(9.806);
/// SQRT(1.0/32.0), :309, folded by gfortran to 0x3e3504f3.
inline constexpr Real kSqrt1o32 = 0x1.6a09e6p-3f;
#if defined(__GNUC__) && !defined(__clang__) && !defined(__CUDACC__)
static_assert(kSqrt1o32 == __builtin_sqrtf(static_cast<Real>(1.0) / static_cast<Real>(32.0)));
#endif
}  // namespace vpfunc_detail

/// VpFunc(k0x, k0y, k1x, k1y, k2x, k2y), :275-315. qr_depth is the module variable read by
/// the three inlined QFunc calls (see gke_qfunc.hpp). Called only by TFunc (:506-507),
/// out of line (the object keeps __w3gkemd_MOD_vpfunc).
KOKKOS_INLINE_FUNCTION Real vpfunc(Real k0x, Real k0y, Real k1x, Real k1y, Real k2x, Real k2y,
                                   Real qr_depth) {
  using vpfunc_detail::kQrGrav;
  using vpfunc_detail::kSqrt1o32;
  // :299-301.
  const Real q0 = qfunc(k0x, k0y, qr_depth);
  const Real q1 = qfunc(k1x, k1y, qr_depth);
  const Real q2 = qfunc(k2x, k2y, qr_depth);
  // :309-312. Left to right: (t01 + t02) + t12; the factor multiplies last.
  return kSqrt1o32 *
         ((k0x * k1x + k0y * k1y + q0 * q1) * Kokkos::sqrt(Kokkos::sqrt(kQrGrav * q2 / (q0 * q1))) +
          (k0x * k2x + k0y * k2y + q0 * q2) * Kokkos::sqrt(Kokkos::sqrt(kQrGrav * q1 / (q0 * q2))) +
          (k1x * k2x + k1y * k2y + q1 * q2) * Kokkos::sqrt(Kokkos::sqrt(kQrGrav * q0 / (q1 * q2))));
}

}  // namespace ww::gke
