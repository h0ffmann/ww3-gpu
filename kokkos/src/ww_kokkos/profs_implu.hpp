// kokkos/src/ww_kokkos/profs_implu.hpp
// IMPLU: one implicit step of the LU factorisation of a banded Hessenberg matrix, in REAL(8),
// of the SPARSKIT solvers inside W3PROFSMD. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:2372-2416 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (module header); SPARSKIT routine. Evidence: kokkos/port_report/IMPLU_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// IMPLU. ypiv, u: np (Fortran base 1, assumed size), REAL*8. permut: np, LOGICAL(4) as int,
/// nonzero true, written 0/1 (seta in the object). umm is INTENT(OUT) in effect.
/// For np <= 1 with full true the Fortran reads npm1 undefined (:2410); here npm1 = 0 there,
/// so no shift: see the port report.
KOKKOS_INLINE_FUNCTION void implu(int np, double& umm, double beta, double* ypiv, double* u,
                                  int* permut, bool full) {
  double x;
  double xpiv;
  int npm1 = 0;

  if (np > 1) {
    // :2381-2394.
    npm1 = np - 1;
    for (int k = 1; k <= npm1; ++k) {
      if (permut[k - 1] != 0) {
        x = u[k - 1];
        u[k - 1] = u[k];
        u[k] = x;
      }
      u[k] = u[k] - ypiv[k - 1] * u[k - 1];
    }
  }
  // :2398-2407.
  umm = u[np - 1];
  const bool perm = (beta > Kokkos::fabs(umm));
  if (perm) {
    xpiv = umm / beta;
    u[np - 1] = beta;
  } else {
    xpiv = beta / umm;
  }
  permut[np - 1] = perm ? 1 : 0;
  ypiv[np - 1] = xpiv;
  if (!full) return;
  // :2410-2413. The object emits these two loops as memmove calls; a forward element copy
  // gives the same bytes.
  for (int k = 1; k <= npm1; ++k) {
    ypiv[k - 1] = ypiv[k];
    permut[k - 1] = permut[k];
  }
}

}  // namespace ww::profs
