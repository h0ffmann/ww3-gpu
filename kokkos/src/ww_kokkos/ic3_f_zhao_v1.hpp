// kokkos/src/ww_kokkos/ic3_f_zhao_v1.hpp
// F_ZHAO_V1: the function whose complex root CMPLX_ROOT_MULLER_V1 finds (IC3 "V1"), FUNC0_ZHAO
// when JUDGE == 0 and FUNC1_ZHAO otherwise.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic3md.F90:1279-1369 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, S. Zieger (from Zhao and Shen's Clarkson code).
// Evidence: kokkos/port_report/F_ZHAO_V1_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "ic3_func0_zhao.hpp"

namespace ww::ic3 {

/// F_ZHAO_V1(X, JUDGE, SIGMA, ES, NU, DICE, HICE, DEPTH): DOUBLE COMPLEX X, INTEGER JUDGE, DOUBLE
/// PRECISION the rest, returns DOUBLE COMPLEX. FUNC1_ZHAO (w3sic3md.F90:1465-1608) is not ported:
/// `func1` stands for it, called as func1(x, sigma, es, nu, dice, hice, depth), until its header
/// exists.
template <class Func1>
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> f_zhao_v1(const Kokkos::complex<double>& x,
                                                          int judge, double sigma, double es,
                                                          double nu, double dice, double hice,
                                                          double depth, const Func1& func1) {
  if (judge == 0) {
    return func0_zhao(x, sigma, depth);
  }
  return func1(x, sigma, es, nu, dice, hice, depth);
}

}  // namespace ww::ic3
