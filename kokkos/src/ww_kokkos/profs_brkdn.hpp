// kokkos/src/ww_kokkos/profs_brkdn.hpp
// BRKDN: breakdown test of the BCGSTAB solver, true when alpha is zero or abnormal.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:2594-2627 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin. Evidence: kokkos/port_report/BRKDN_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// BRKDN; ipar is IPAR(16), 0-based, only ipar[0] (IPAR(1)) is written, and only when true.
/// The division is kept: 1/alpha is not > 0 for alpha = +Inf, and a NaN alpha takes the last branch.
KOKKOS_INLINE_FUNCTION bool brkdn(const double& alpha, int* ipar) {
  const double zero = 0.0;
  const double one = 1.0;
  bool r = false;
  if (alpha > zero) {
    const double beta = one / alpha;
    if (!(beta > zero)) {
      r = true;
      ipar[0] = -9;
    }
  } else if (alpha < zero) {
    const double beta = one / alpha;
    if (!(beta < zero)) {
      r = true;
      ipar[0] = -9;
    }
  } else if (alpha == zero) {
    r = true;
    ipar[0] = -3;
  } else {
    r = true;
    ipar[0] = -9;
  }
  return r;
}

}  // namespace ww::profs
