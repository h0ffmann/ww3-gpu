// kokkos/src/ww_kokkos/profs_bisinit.hpp
// BISINIT: common initialisation of the iterative solvers' IPAR, FPAR and work array.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:2630-2699 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin. Evidence: kokkos/port_report/BISINIT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// BISINIT; ipar is IPAR(16) and fpar FPAR(16), 0-based; wk is WK(*), 0-based, wksize elements
/// cleared. On return with ipar[0] = -2 or -4 nothing after that point is touched (:2644-2648, :2680-2682).
KOKKOS_INLINE_FUNCTION void bisinit(int* ipar, double* fpar, int wksize, int dsc, bool& lp, bool& rp,
                                    double* wk) {
  const double zero = 0.0;
  const double one = 1.0;
  if (ipar[3] < wksize) {
    ipar[0] = -2;
    ipar[3] = wksize;
    return;
  }
  if (ipar[1] > 2) {
    lp = true;
    rp = true;
  } else if (ipar[1] == 2) {
    lp = false;
    rp = true;
  } else if (ipar[1] == 1) {
    lp = true;
    rp = false;
  } else {
    lp = false;
    rp = false;
  }
  if (ipar[2] == 0) ipar[2] = dsc;
  ipar[6] = 0;
  ipar[7] = 0;
  ipar[8] = 0;
  ipar[9] = 0;
  ipar[10] = 0;
  ipar[11] = 0;
  ipar[12] = 0;
  if (fpar[0] < zero || fpar[0] >= one || fpar[1] < zero || (fpar[0] == zero && fpar[1] == zero)) {
    if (ipar[0] == 0) {
      ipar[0] = -4;
      return;
    } else {
      fpar[0] = 1.0e-6;
      fpar[1] = 1.0e-16;
    }
  }
  for (int i = 2; i < 10; ++i) fpar[i] = zero;
  if (fpar[10] < zero) fpar[10] = zero;
  for (int i = 0; i < wksize; ++i) wk[i] = zero;
}

}  // namespace ww::profs
