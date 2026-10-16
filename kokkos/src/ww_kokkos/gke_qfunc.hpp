// kokkos/src/ww_kokkos/gke_qfunc.hpp
// QFunc of W3GKEMD: q = w**2/g = k tanh(k d) for the wavenumber vector (kx, ky), the q of
// the Zakharov kernel (Krasitskii 1994, Janssen 2009). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3gkemd.F90:234-271 at NOAA-EMC/WW3 03adda91;
// authors O. Gramstad, Q. Liu. Evidence: kokkos/port_report/QFUNC_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cmath>

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::gke {

/// qr_eps = EPSILON(100.0), :193: FLT_EPSILON, 0x34000000 in the object.
inline constexpr Real kQrEps = 0x1p-23f;
/// qr_dmax, :201: 3000.0, 0x453b8000 in the object.
inline constexpr Real kQrDmax = static_cast<Real>(3000.0);

namespace qfunc_detail {
// glibc's scalar tanhf on the host, as the object calls it (no libmvec variant).
// ⚠ On a device Kokkos::tanh (CUDA libdevice) is not shown to return glibc's bits.
KOKKOS_INLINE_FUNCTION Real tanh_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::tanhf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::tanh(x);))
#else
  return Kokkos::tanh(x);
#endif
}
}  // namespace qfunc_detail

/// QFunc(kx, ky), :234-271. qr_depth is the module variable w3gkemd's qr_depth (:165), the
/// water depth d in m that the caller has set (clipped to [0, qr_dmax] at :1256); pass it
/// as the Fortran reads it at the call. Deep water (d <= eps or d >= 3000, or NaN) gives
/// q = |k|. Called by VpFunc, VmFunc, UFunc, TFunc, FindQuartetNumber and FindQuartetConfig;
/// gfortran inlines every call (the object never calls __w3gkemd_MOD_qfunc).
KOKKOS_INLINE_FUNCTION Real qfunc(Real kx, Real ky, Real qr_depth) {
  // :261. sqrtss; the object squares both, then adds (commutative, same bits).
  Real q = Kokkos::sqrt(kx * kx + ky * ky);
  // :265. Both comiss are ordered: a NaN depth takes the deep-water branch.
  if (qr_depth > kQrEps && qr_depth < kQrDmax) {
    // :266.
    q = q * qfunc_detail::tanh_(q * qr_depth);
  }
  return q;
}

}  // namespace ww::gke
