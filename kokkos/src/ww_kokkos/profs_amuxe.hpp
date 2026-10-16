// kokkos/src/ww_kokkos/profs_amuxe.hpp
// AMUXE: y = A x for a REAL(8) matrix in Ellpack-Itpack (ELL) storage, a SPARSKIT routine
// kept inside W3PROFSMD. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:3046-3093 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (the routine is Y. Saad's, SPARSKIT).
// Evidence: kokkos/port_report/AMUXE_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// AMUXE(N, X, Y, NA, NCOL, A, JA); x is X(1:*), y is Y(1:N), a is A(1:NA,1:NCOL) and ja is
/// JA(1:NA,1:NCOL) column-major, all 0-based here. ja holds the Fortran's 1-based column numbers.
KOKKOS_INLINE_FUNCTION void amuxe(int n, const double* x, double* y, int na, int ncol,
                                  const double* a, const int* ja) {
  for (int i = 1; i <= n; ++i) y[i - 1] = 0.0;
  // gfortran takes the leading dimension as MAX(NA,0) in 64 bits.
  const long stride = na > 0 ? static_cast<long>(na) : 0L;
  for (int j = 1; j <= ncol; ++j) {
    const long off = stride * (static_cast<long>(j) - 1);
    for (int i = 1; i <= n; ++i)
      y[i - 1] = y[i - 1] + a[off + i - 1] * x[ja[off + i - 1] - 1];
  }
}

}  // namespace ww::profs
