// kokkos/src/ww_kokkos/profs_ilu0.hpp
// ilu0: incomplete LU factorisation with no fill-in of a sparse matrix in compressed sparse row
// storage, written to modified sparse row storage (alu, jlu, ju) with the diagonal stored inverted.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:4241-4300 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (module header); SPARSKIT routine. Evidence: kokkos/port_report/ILU0_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// ilu0, REAL*8 throughout. a, ja: nnz (Fortran base 1), ja holds 1-based column indices;
/// ia: n+1, 1-based row pointers. alu, jlu: Fortran base 1, at least ia(n+1)+1 entries (jlu holds
/// 1-based indices); ju: n, 1-based indices. iw: n, a work array (the Fortran dummy iw(n)), left
/// zero on a normal return. ierr: 0, or the 1-based row of a zero pivot.
KOKKOS_INLINE_FUNCTION void ilu0(int n, const double* a, const int* ja, const int* ia,
                                 double* alu, int* jlu, int* ju, int* iw, int& ierr) {
  // :4249-4251.
  int ju0 = n + 2;
  jlu[0] = ju0;
  for (int k = 0; k < n; ++k) {
    iw[k] = 0;
  }
  for (int ii = 1; ii <= n; ++ii) {
    const int js = ju0;
    // :4254-4266; DO bounds are fixed on entry, as in the Fortran.
    const int jb = ia[ii - 1];
    const int je = ia[ii] - 1;
    for (int j = jb; j <= je; ++j) {
      const int jcol = ja[j - 1];
      if (jcol == ii) {
        alu[ii - 1] = a[j - 1];
        iw[jcol - 1] = ii;
        ju[ii - 1] = ju0;
      } else {
        alu[ju0 - 1] = a[j - 1];
        jlu[ju0 - 1] = ja[j - 1];
        iw[jcol - 1] = ju0;
        ju0 = ju0 + 1;
      }
    }
    jlu[ii] = ju0;
    const int jf = ju0 - 1;
    // A row with no diagonal entry leaves ju(ii) unset here (:4269), as in the Fortran.
    const int jm = ju[ii - 1] - 1;
    // :4271-4282.
    for (int j = js; j <= jm; ++j) {
      const int jrow = jlu[j - 1];
      const double tl = alu[j - 1] * alu[jrow - 1];
      alu[j - 1] = tl;
      const int kb = ju[jrow - 1];
      const int ke = jlu[jrow] - 1;
      for (int jj = kb; jj <= ke; ++jj) {
        const int jw = iw[jlu[jj - 1] - 1];
        if (jw != 0) {
          alu[jw - 1] = alu[jw - 1] - tl * alu[jj - 1];
        }
      }
    }
    // :4285: tiny(1.) is the REAL(4) tiny, FLT_MIN, compared as a double.
    if (Kokkos::fabs(alu[ii - 1]) < 0x1p-126) {
      ierr = ii;
      return;
    }
    alu[ii - 1] = 1.0 / alu[ii - 1];
    // :4291-4295.
    iw[ii - 1] = 0;
    for (int i = js; i <= jf; ++i) {
      iw[jlu[i - 1] - 1] = 0;
    }
  }
  ierr = 0;
}

}  // namespace ww::profs
