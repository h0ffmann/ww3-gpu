// kokkos/src/ww_kokkos/ic3_f_zhao_cheng.hpp
// F_ZHAO_CHENG: picks the IC3 "Cheng" dispersion residual by JUDGE (DRFUN_DBLE_CHENG for
// JUDGE > 0, DRFUN_QUAD_CHENG for JUDGE == 0), the function whose root CMPLX_ROOT_MULLER_CHENG finds.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic3md.F90:2713-2809 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, S. Zieger, X. Zhao, S. Cheng.
// Evidence: kokkos/port_report/F_ZHAO_CHENG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::ic3 {

/// F_ZHAO_CHENG(JUDGE, X, SIGMA, ES, NU, DICE, HICE, DEPTH), DOUBLE COMPLEX result.
/// DRFUN_DBLE_CHENG and DRFUN_QUAD_CHENG (:2812, :2967) are not translated yet and come in as
/// callables:
///   drfun_dble(x, sigma, es, nu, dice, hice, depth, judge) -> Kokkos::complex<double>
///   drfun_quad(x, sigma, es, nu, dice, hice, depth)        -> Kokkos::complex<double>
/// Trap: for JUDGE < 0 the Fortran never sets FZHAO (:2803-2807) and returns an undefined
/// value; the -O3 object returns (0.0, 0.0) on that path, and so does this. Every caller in
/// WW3 passes JUDGE = 1, 2 or 3 (:1939, :2284-2338).
template <class DrfunDble, class DrfunQuad>
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> f_zhao_cheng(
    int judge, const Kokkos::complex<double>& x, double sigma, double es, double nu, double dice,
    double hice, double depth, const DrfunDble& drfun_dble, const DrfunQuad& drfun_quad) {
  if (judge > 0) {
    return drfun_dble(x, sigma, es, nu, dice, hice, depth, judge);
  }
  if (judge == 0) {
    return drfun_quad(x, sigma, es, nu, dice, hice, depth);
  }
  return Kokkos::complex<double>(0.0, 0.0);
}

}  // namespace ww::ic3
