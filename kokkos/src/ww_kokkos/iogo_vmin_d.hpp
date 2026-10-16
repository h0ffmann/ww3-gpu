// kokkos/src/ww_kokkos/iogo_vmin_d.hpp
// VMIN_D: Zakharov's deep-water, no-current coefficient of the three-wave difference
// interaction, internal to SECONDHH. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3iogomd.F90:4777-4816 (DEL1, ZCONST at :4654-4655)
// at NOAA-EMC/WW3 03adda91; author P. Janssen. Evidence: kokkos/port_report/VMIN_D_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::iogo {

namespace vmin_d_detail {
// SECONDHH's REAL(KIND=4) PARAMETERs, rounded once from the decimal literal as gfortran does.
inline constexpr Real kDel1 = 1.0E-8f;
inline constexpr Real kZconst = 0.0281349f;
}  // namespace vmin_d_detail

/// VMIN_D(XI,XJ,XK,XIJ,XIK,XJK,XOI,XOJ,XOK), all scalars.
KOKKOS_INLINE_FUNCTION Real vmin_d(Real xi, Real xj, Real xk, Real xij, Real xik, Real xjk,
                                   Real xoi, Real xoj, Real xok) {
  using vmin_d_detail::kDel1;
  using vmin_d_detail::kZconst;
  const Real ri = Kokkos::fabs(xi) + kDel1;
  const Real rj = Kokkos::fabs(xj) + kDel1;
  const Real rk = Kokkos::fabs(xk) + kDel1;
  const Real oi = xoi + kDel1;
  const Real oj = xoj + kDel1;
  const Real ok = xok + kDel1;
  const Real sqijk = Kokkos::sqrt(oi * oj * rk / (ok * ri * rj));
  const Real sqikj = Kokkos::sqrt(oi * ok * rj / (oj * ri * rk));
  const Real sqjki = Kokkos::sqrt(oj * ok * ri / (oi * rj * rk));
  return kZconst * ((xij - ri * rj) * sqijk + (xik - ri * rk) * sqikj + (xjk + rj * rk) * sqjki);
}

}  // namespace ww::iogo
