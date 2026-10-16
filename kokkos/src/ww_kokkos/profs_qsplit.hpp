// kokkos/src/ww_kokkos/profs_qsplit.hpp
// QSPLIT: quick-sort split of a REAL*8 array by absolute value, with a companion index array
// (SPARSKIT, used by ILUT to keep the largest entries). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:3738-3800 at NOAA-EMC/WW3 03adda91;
// module authors A. Roland, F. Ardhuin. Evidence: kokkos/port_report/QSPLIT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// QSPLIT, one thread: permutes a and ind in place so that |a(i)| >= |a(ncut)| for i < ncut
/// and |a(i)| <= |a(ncut)| for i > ncut. a: A(N), REAL*8; ind: IND(N); both Fortran base 1.
/// n and ncut are 1-based counts/positions as in the Fortran. Its only caller, ILUT (:4172,
/// :4203), is called by nothing in WW3.
KOKKOS_INLINE_FUNCTION void qsplit(double* a, int* ind, int n, int ncut) {
  int first = 1;
  int last = n;
  if (ncut < first || ncut > last) return;  // :3759

  for (;;) {
    int mid = first;
    const double abskey = Kokkos::fabs(a[mid - 1]);
    for (int j = first + 1; j <= last; ++j) {
      // :3767, ordered compare: a NaN never moves.
      if (Kokkos::fabs(a[j - 1]) > abskey) {
        mid = mid + 1;
        const double tmp = a[mid - 1];
        const int itmp = ind[mid - 1];
        a[mid - 1] = a[j - 1];
        ind[mid - 1] = ind[j - 1];
        a[j - 1] = tmp;
        ind[j - 1] = itmp;
      }
    }

    const double tmp = a[mid - 1];
    a[mid - 1] = a[first - 1];
    a[first - 1] = tmp;

    const int itmp = ind[mid - 1];
    ind[mid - 1] = ind[first - 1];
    ind[first - 1] = itmp;

    if (mid == ncut) return;
    if (mid > ncut) {
      last = mid - 1;
    } else {
      first = mid + 1;
    }
  }
}

}  // namespace ww::profs
