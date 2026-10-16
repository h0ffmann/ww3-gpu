// kokkos/src/ww_kokkos/profs_vbrmv.hpp
// VBRMV: b = A x for a REAL(8) matrix in variable block row (VBR) storage, a SPARSKIT routine
// kept inside W3PROFSMD. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:3203-3251 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (the routine is Y. Saad's, SPARSKIT).
// Evidence: kokkos/port_report/VBRMV_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// VBRMV(NR, NC, IA, JA, KA, A, KVSTR, KVSTC, X, B); ia is IA(1:nr+1), kvstr is KVSTR(1:nr+1),
/// ja, ka, a, kvstc, x and b are the assumed-size (1:*) arrays, all 0-based here; ia, ja, kvstr
/// and kvstc hold the Fortran's 1-based indices. ka is declared and never read, as in the Fortran.
KOKKOS_INLINE_FUNCTION void vbrmv(int nr, int nc, const int* ia, const int* ja, const int* ka,
                                  const double* a, const int* kvstr, const int* kvstc,
                                  const double* x, double* b) {
  const int n = kvstc[nc] - 1;
  for (int i = 1; i <= n; ++i) b[i - 1] = 0.0;
  int k = 1;
  for (int i = 1; i <= nr; ++i) {
    const int istart = kvstr[i - 1];
    const int istop = kvstr[i] - 1;
    const int jend = ia[i] - 1;
    for (int j = ia[i - 1]; j <= jend; ++j) {
      const int jjend = kvstc[ja[j - 1]] - 1;
      for (int jj = kvstc[ja[j - 1] - 1]; jj <= jjend; ++jj) {
        const double xjj = x[jj - 1];
        for (int ii = istart; ii <= istop; ++ii) {
          b[ii - 1] = b[ii - 1] + xjj * a[k - 1];
          k = k + 1;
        }
      }
    }
  }
}

}  // namespace ww::profs
