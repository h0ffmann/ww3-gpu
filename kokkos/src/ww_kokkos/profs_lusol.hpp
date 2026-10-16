// kokkos/src/ww_kokkos/profs_lusol.hpp
// lusol: solves (LU) x = y with the ILU factors in modified sparse row storage
// (forward then backward substitution, diagonal stored inverted). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:3673-3699 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (module header); SPARSKIT routine. Evidence: kokkos/port_report/LUSOL_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// lusol, REAL*8 throughout. y, x: n (Fortran base 1). alu, jlu: Fortran base 1, jlu holds
/// 1-based indices and needs jlu[n] (jlu(n+1)); ju: n, 1-based indices.
/// y and x may be the same array: the Fortran caller at :4518 passes rhs twice, and the
/// order below reads y(i) before writing x(i), as the object does.
KOKKOS_INLINE_FUNCTION void lusol(int n, const double* y, double* x, const double* alu,
                                  const int* jlu, const int* ju) {
  // :3684-3689.
  for (int i = 1; i <= n; ++i) {
    x[i - 1] = y[i - 1];
    for (int k = jlu[i - 1]; k <= ju[i - 1] - 1; ++k) {
      x[i - 1] = x[i - 1] - alu[k - 1] * x[jlu[k - 1] - 1];
    }
  }
  // :3690-3695.
  for (int i = n; i >= 1; --i) {
    for (int k = ju[i - 1]; k <= jlu[i] - 1; ++k) {
      x[i - 1] = x[i - 1] - alu[k - 1] * x[jlu[k - 1] - 1];
    }
    x[i - 1] = alu[i - 1] * x[i - 1];
  }
}

}  // namespace ww::profs
