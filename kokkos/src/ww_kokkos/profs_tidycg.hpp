// kokkos/src/ww_kokkos/profs_tidycg.hpp
// TIDYCG: closing bookkeeping of the BCGSTAB solver (return code, convergence rate, sol += delx).
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:2560-2591 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin. Evidence: kokkos/port_report/TIDYCG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// TIDYCG, arrays 0-based: ipar[16], fpar[16] (Fortran IPAR(1:16), FPAR(1:16)), sol[n], delx[n].
/// REAL*8 stays double.
KOKKOS_INLINE_FUNCTION void tidycg(int n, int* ipar, double* fpar, double* sol,
                                   const double* delx) {
  const double zero = 0.0;

  // :2570-2581.
  if (ipar[11] != 0) {
    ipar[0] = ipar[11];
  } else if (ipar[0] > 0) {
    if ((ipar[2] == 999 && ipar[10] == 1) || fpar[5] <= fpar[3]) {
      ipar[0] = 0;
    } else if (ipar[6] >= ipar[5] && ipar[5] > 0) {
      ipar[0] = -1;
    } else {
      ipar[0] = -10;
    }
  }
  // :2582-2586; the object calls libm log10 (double), divsd, cvtsi2sd of the int difference.
  if (fpar[2] > zero && fpar[5] > zero && ipar[6] > ipar[12]) {
    fpar[6] = Kokkos::log10(fpar[2] / fpar[5]) / static_cast<double>(ipar[6] - ipar[12]);
  } else {
    fpar[6] = zero;
  }
  // :2587-2589; the object vectorises this with addpd, element-wise, so the same bits.
  for (int i = 0; i < n; ++i) {
    sol[i] = sol[i] + delx[i];
  }
}

}  // namespace ww::profs
