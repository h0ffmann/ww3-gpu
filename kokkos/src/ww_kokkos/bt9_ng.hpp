// kokkos/src/ww_kokkos/bt9_ng.hpp
// NG: the dissipation rate and the muddy wave number of a viscous fluid-mud layer after
// Ng (2000), for one frequency of W3SBT9. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sbt9md.F90:429-557 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, M. Orzech. Evidence: kokkos/port_report/NG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cmath>

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::bt9 {

namespace ng_detail {
// The object calls glibc's scalar sinhf, coshf and, for SIN(DTILDE) with COS(DTILDE),
// one sincosf; g++ merges ::sinf and ::cosf of one argument into the same sincosf.
// ⚠ On a device, Kokkos' functions are CUDA's and not shown to return glibc's bits.
KOKKOS_INLINE_FUNCTION Real sinh_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::sinhf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::sinh(x);))
#else
  return Kokkos::sinh(x);
#endif
}
KOKKOS_INLINE_FUNCTION Real cosh_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::coshf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::cosh(x);))
#else
  return Kokkos::cosh(x);
#endif
}
KOKKOS_INLINE_FUNCTION Real sin_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::sinf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::sin(x);))
#else
  return Kokkos::sin(x);
#endif
}
KOKKOS_INLINE_FUNCTION Real cos_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::cosf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::cos(x);))
#else
  return Kokkos::cos(x);
#endif
}
}  // namespace ng_detail

/// NG(SIGMA, H_WDEPTH, DTILDE, ZETA, SBLTM, GAMMA, WK, WKDR, DISS), :429-557.
/// SIGMA is a dummy the Fortran never reads.
KOKKOS_INLINE_FUNCTION void ng(Real sigma, Real h_wdepth, Real dtilde, Real zeta, Real sbltm,
                               Real gamma, Real wk, Real& wkdr, Real& diss) {
  (void)sigma;
  const Real one = static_cast<Real>(1.);
  const Real two = static_cast<Real>(2.);
  const Real dm = dtilde * sbltm;  // :529
  const Real sh = ng_detail::sinh_(dtilde);
  const Real ch = ng_detail::cosh_(dtilde);
  const Real sn = ng_detail::sin_(dtilde);
  const Real cs = ng_detail::cos_(dtilde);
  // :531-536. -2.0*GAMMA**2+2.0*GAMMA as gfortran folds it, 2.0*GAMMA-2.0*GAMMA**2
  // (the same bits: y+(-x) is y-x in IEEE arithmetic).
  const Real gpoly = two * gamma - two * (gamma * gamma) - one;
  const Real b1 = gamma * (gpoly - zeta * zeta) * sh * ch -
                  (gamma * gamma) * zeta * (ch * ch + sh * sh) -
                  ((gamma - one) * (gamma - one)) * zeta * (ch * ch * (cs * cs) + sh * sh * (sn * sn)) -
                  two * gamma * (one - gamma) * (zeta * ch + gamma * sh) * cs;
  // :538-540
  const Real b2 = gamma * (gpoly + zeta * zeta) * sn * cs -
                  two * gamma * (one - gamma) * (zeta * sh + gamma * ch) * sn;
  // :542-543
  const Real b3 = (zeta * ch + gamma * sh) * (zeta * ch + gamma * sh) * (cs * cs) +
                  (zeta * sh + gamma * ch) * (zeta * sh + gamma * ch) * (sn * sn);
  // :545. BI (:547) is computed by the Fortran and never read; gfortran drops it.
  const Real br = wk * sbltm * (b1 - b2) / (two * b3) + gamma * wk * dm;
  const Real brp = b1 / b3;  // :548
  const Real bip = b2 / b3;  // :549
  // :552-553
  diss = -(sbltm * (brp + bip) * (wk * wk) /
           (ng_detail::sinh_(two * wk * h_wdepth) + two * wk * h_wdepth));
  wkdr = wk - br * wk / (ng_detail::sinh_(wk * h_wdepth) * ng_detail::cosh_(wk * h_wdepth) +
                         wk * h_wdepth);
}

}  // namespace ww::bt9
