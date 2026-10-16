// kokkos/src/ww_kokkos/profs_amuxd.hpp
// AMUXD: y = A x for a REAL(8) matrix in diagonal (DIA) storage, a SPARSKIT routine kept
// inside W3PROFSMD. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:3095-3146 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (the routine is Y. Saad's, SPARSKIT).
// Evidence: kokkos/port_report/AMUXD_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// AMUXD(N, X, Y, DIAG, NDIAG, IDIAG, IOFF); x is X(1:N), y is Y(1:N), diag is
/// DIAG(1:NDIAG,1:IDIAG) column-major and ioff is IOFF(1:IDIAG), all 0-based here.
/// diag(k,j) holds A(k, k+ioff(j)).
KOKKOS_INLINE_FUNCTION void amuxd(int n, const double* x, double* y, const double* diag,
                                  int ndiag, int idiag, const int* ioff) {
  for (int j = 1; j <= n; ++j) y[j - 1] = 0.0;
  // gfortran takes the leading dimension as MAX(NDIAG,0) in 64 bits.
  const long stride = ndiag > 0 ? static_cast<long>(ndiag) : 0L;
  for (int j = 1; j <= idiag; ++j) {
    const int io = ioff[j - 1];
    const int i1 = 1 - io > 1 ? 1 - io : 1;  // MAX0(1,1-io), :3136
    const int i2 = n < n - io ? n : n - io;  // MIN0(n,n-io), :3137
    const long off = stride * (static_cast<long>(j) - 1);
    // k+io is a 32-bit sum, sign-extended for the index, as in the object.
    for (int k = i1; k <= i2; ++k)
      y[k - 1] = y[k - 1] + diag[off + k - 1] * x[static_cast<long>(k + io) - 1];
  }
}

}  // namespace ww::profs
