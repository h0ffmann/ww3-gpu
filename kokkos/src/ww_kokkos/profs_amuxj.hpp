// kokkos/src/ww_kokkos/profs_amuxj.hpp
// AMUXJ: y = A x for a REAL(8) matrix in jagged-diagonal (JAD) storage, a SPARSKIT routine
// kept inside W3PROFSMD. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:3148-3201 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (the routine is Y. Saad's, SPARSKIT).
// Evidence: kokkos/port_report/AMUXJ_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// AMUXJ(N, X, Y, JDIAG, A, JA, IA); x is X(1:N), y is Y(1:N), a is A(1:*), ja is JA(1:*) and
/// ia is IA(1:JDIAG+1), all 0-based here. ja holds the Fortran's 1-based column numbers and ia
/// the 1-based start of each jagged diagonal in a and ja. The permutation of the JAD format is
/// not applied (the caller's PERMVEC, as in the Fortran).
KOKKOS_INLINE_FUNCTION void amuxj(int n, const double* x, double* y, int jdiag, const double* a,
                                  const int* ja, const int* ia) {
  for (int i = 1; i <= n; ++i) y[i - 1] = 0.0;
  for (int ii = 1; ii <= jdiag; ++ii) {
    const int k1 = ia[ii - 1] - 1;
    const int ilen = ia[ii] - k1 - 1;
    for (int j = 1; j <= ilen; ++j)
      y[j - 1] = y[j - 1] + a[k1 + j - 1] * x[ja[k1 + j - 1] - 1];
  }
}

}  // namespace ww::profs
