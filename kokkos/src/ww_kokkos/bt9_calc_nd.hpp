// kokkos/src/ww_kokkos/bt9_calc_nd.hpp
// CALC_ND: the factor ND that W3SBT9 (Ng 2000 fluid-mud dissipation) computes for the
// rock and the mud wave numbers. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sbt9md.F90:571-590 at NOAA-EMC/WW3 03adda91;
// authors M. Orzech, W. E. Rogers. Evidence: kokkos/port_report/CALC_ND_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cmath>

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::bt9 {

namespace calc_nd_detail {
// The object calls glibc's scalar sinhf.
// ⚠ On a device, Kokkos' sinh is CUDA's and not shown to return glibc's bits.
KOKKOS_INLINE_FUNCTION Real sinh_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::sinhf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::sinh(x);))
#else
  return Kokkos::sinh(x);
#endif
}
}  // namespace calc_nd_detail

/// CALC_ND(KWAVE, H_WDEPTH, SND2, ND), :571-590.
KOKKOS_INLINE_FUNCTION void calc_nd(Real kwave, Real h_wdepth, Real snd2, Real& nd) {
  const Real two = static_cast<Real>(2.);
  // :584-588. gfortran emits 2.*X as X+X, the same bits.
  const Real knd = kwave * h_wdepth;
  const Real fac1 = two * knd / calc_nd_detail::sinh_(two * knd);
  const Real fac2 = snd2 / knd;
  const Real fac3 = two * fac2 / (static_cast<Real>(1.) + fac2 * fac2);
  nd = fac1 * (static_cast<Real>(0.5) / h_wdepth - kwave / fac3);
}

}  // namespace ww::bt9
