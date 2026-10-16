// kokkos/src/ww_kokkos/gke_tfunc.hpp
// TFunc of W3GKEMD: the kernel T_{1,2,3,4} of the four-wave interaction (Janssen 2009;
// V~^{(2)}_{0,1,2,3} of Krasitskii 1994), direct part W minus the virtual-state terms.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3gkemd.F90:400-517 at NOAA-EMC/WW3 03adda91;
// authors O. Gramstad, Q. Liu. Evidence: kokkos/port_report/TFUNC_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "gke_qfunc.hpp"
#include "gke_ufunc.hpp"
#include "gke_vmfunc.hpp"
#include "gke_vpfunc.hpp"
#include "real.hpp"

namespace ww::gke {

namespace tfunc_detail {
/// qr_grav, :198: 9.806, 0x411ce560 in the object.
inline constexpr Real kQrGrav = static_cast<Real>(9.806);
}  // namespace tfunc_detail

/// TFunc(k0x, k0y, k1x, k1y, k2x, k2y, k3x, k3y), :400-517, for four wavenumber vectors with
/// n0 != n2 and n0 != n3. qr_depth is the module variable qr_depth (:165) that the inlined
/// QFunc and the out-of-line UFunc, VmFunc, VpFunc read. Only caller: PrepKernelIO (:1295),
/// once per quartet, out of line.
KOKKOS_INLINE_FUNCTION Real tfunc(Real k0x, Real k0y, Real k1x, Real k1y, Real k2x, Real k2y,
                                  Real k3x, Real k3y, Real qr_depth) {
  using tfunc_detail::kQrGrav;
  const Real one = static_cast<Real>(1.0);
  // :440-445.
  Real om0p1 = static_cast<Real>(0.);
  Real om2p3 = static_cast<Real>(0.);
  Real L56 = static_cast<Real>(0.);
  // :452-462. The object multiplies q*g (commuted), same bits.
  const Real om0 = Kokkos::sqrt(kQrGrav * qfunc(k0x, k0y, qr_depth));
  const Real om1 = Kokkos::sqrt(kQrGrav * qfunc(k1x, k1y, qr_depth));
  const Real om2 = Kokkos::sqrt(kQrGrav * qfunc(k2x, k2y, qr_depth));
  const Real om3 = Kokkos::sqrt(kQrGrav * qfunc(k3x, k3y, qr_depth));
  const Real om02 = Kokkos::sqrt(kQrGrav * qfunc(k0x - k2x, k0y - k2y, qr_depth));
  const Real om13 = Kokkos::sqrt(kQrGrav * qfunc(k1x - k3x, k1y - k3y, qr_depth));
  const Real om12 = Kokkos::sqrt(kQrGrav * qfunc(k1x - k2x, k1y - k2y, qr_depth));
  const Real om03 = Kokkos::sqrt(kQrGrav * qfunc(k0x - k3x, k0y - k3y, qr_depth));
  // :464. Ordered compares: a NaN sum leaves om0p1 = om2p3 = 0.
  if (Kokkos::fabs(k0x + k1x) > kQrEps || Kokkos::fabs(k0y + k1y) > kQrEps) {
    // :467-468.
    om0p1 = Kokkos::sqrt(kQrGrav * qfunc(k0x + k1x, k0y + k1y, qr_depth));
    om2p3 = Kokkos::sqrt(kQrGrav * qfunc(k2x + k3x, k2y + k3y, qr_depth));
  }
  // :473-478. Six out-of-line UFunc, summed left to right; -k is a negation, not 0-k.
  const Real W = ufunc(-k0x, -k0y, -k1x, -k1y, k2x, k2y, k3x, k3y, qr_depth) +
                 ufunc(k2x, k2y, k3x, k3y, -k0x, -k0y, -k1x, -k1y, qr_depth) -
                 ufunc(k2x, k2y, -k1x, -k1y, -k0x, -k0y, k3x, k3y, qr_depth) -
                 ufunc(-k0x, -k0y, k2x, k2y, -k1x, -k1y, k3x, k3y, qr_depth) -
                 ufunc(-k0x, -k0y, k3x, k3y, k2x, k2y, -k1x, -k1y, qr_depth) -
                 ufunc(k3x, k3y, -k1x, -k1y, k2x, k2y, -k0x, -k0y, qr_depth);
  // :482-487. k3x-k1x is computed as written, not as -(k1x-k3x).
  const Real L14 = vmfunc(k0x, k0y, k2x, k2y, k0x - k2x, k0y - k2y, qr_depth) *
                       vmfunc(k3x, k3y, k1x, k1y, k3x - k1x, k3y - k1y, qr_depth) *
                       (one / (om2 + om02 - om0) + one / (om1 + om13 - om3)) +
                   vmfunc(k1x, k1y, k3x, k3y, k1x - k3x, k1y - k3y, qr_depth) *
                       vmfunc(k2x, k2y, k0x, k0y, k2x - k0x, k2y - k0y, qr_depth) *
                       (one / (om3 + om13 - om1) + one / (om0 + om02 - om2));
  // :491-496.
  const Real L23 = vmfunc(k1x, k1y, k2x, k2y, k1x - k2x, k1y - k2y, qr_depth) *
                       vmfunc(k3x, k3y, k0x, k0y, k3x - k0x, k3y - k0y, qr_depth) *
                       (one / (om2 + om12 - om1) + one / (om0 + om03 - om3)) +
                   vmfunc(k0x, k0y, k3x, k3y, k0x - k3x, k0y - k3y, qr_depth) *
                       vmfunc(k2x, k2y, k1x, k1y, k2x - k1x, k2y - k1y, qr_depth) *
                       (one / (om3 + om03 - om0) + one / (om1 + om12 - om2));
  // :500-508. -k0x-k1x is (-k0x)-k1x.
  if (Kokkos::fabs(k0x + k1x) > kQrEps || Kokkos::fabs(k0y + k1y) > kQrEps) {
    L56 = vmfunc(k0x + k1x, k0y + k1y, k0x, k0y, k1x, k1y, qr_depth) *
              vmfunc(k2x + k3x, k2y + k3y, k2x, k2y, k3x, k3y, qr_depth) *
              (one / (om0p1 - om0 - om1) + one / (om2p3 - om2 - om3)) +
          vpfunc(-k0x - k1x, -k0y - k1y, k0x, k0y, k1x, k1y, qr_depth) *
              vpfunc(-k2x - k3x, -k2y - k3y, k2x, k2y, k3x, k3y, qr_depth) *
              (one / (om0p1 + om0 + om1) + one / (om2p3 + om2 + om3));
  }
  // :513.
  return W - L14 - L23 - L56;
}

}  // namespace ww::gke
