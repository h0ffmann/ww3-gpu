// kokkos/src/ww_kokkos/profs_lutsol.hpp
// LUTSOL: solves (LU)^T x = y for an ILU factor held in modified sparse row form
// (SPARSKIT), one sequential triangular sweep. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:3701-3736 at NOAA-EMC/WW3 03adda91;
// module authors A. Roland, F. Ardhuin. Evidence: kokkos/port_report/LUTSOL_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// LUTSOL, one thread: the sweeps carry a dependence through x, so it is not parallel.
/// y, x: X(N), Y(N), REAL*8, Fortran base 1. alu, jlu, ju: assumed size, Fortran base 1;
/// jlu and ju hold 1-based positions into alu/jlu and 1-based row indices, as SPARSKIT's
/// ILUT writes them. Nothing in WW3 reaches it (BCGSTAB never asks for IPAR(1) = 4 or 6).
KOKKOS_INLINE_FUNCTION void lutsol(int n, const double* y, double* x, const double* alu,
                                   const int* jlu, const int* ju) {
  // :3712-3714.
  for (int i = 1; i <= n; ++i) x[i - 1] = y[i - 1];

  // :3718-3723, forward solve with U^T. The DO bounds are read once, before the loop.
  // x(i) is reloaded per k as in the object: jlu(k) may equal i.
  for (int i = 1; i <= n; ++i) {
    x[i - 1] = x[i - 1] * alu[i - 1];
    const int k0 = ju[i - 1];
    const int k1 = jlu[i] - 1;
    for (int k = k0; k <= k1; ++k) {
      const int j = jlu[k - 1];
      x[j - 1] = x[j - 1] - alu[k - 1] * x[i - 1];
    }
  }

  // :3727-3731, backward solve with L^T.
  for (int i = n; i >= 1; --i) {
    const int k0 = jlu[i - 1];
    const int k1 = ju[i - 1] - 1;
    for (int k = k0; k <= k1; ++k) {
      const int j = jlu[k - 1];
      x[j - 1] = x[j - 1] - alu[k - 1] * x[i - 1];
    }
  }
}

}  // namespace ww::profs
