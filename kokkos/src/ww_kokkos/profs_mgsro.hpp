// kokkos/src/ww_kokkos/profs_mgsro.hpp
// MGSRO: modified Gram-Schmidt with selective re-orthogonalisation of one REAL(8) column
// against the others, of the SPARSKIT solvers inside W3PROFSMD. Phase 1, translated, not
// regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:2701-2822 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin. Evidence: kokkos/port_report/MGSRO_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "profs_ddot.hpp"

namespace ww::profs {

/// MGSRO(FULL, LDA, N, M, IND, OPS, VEC, HH, IERR). vec is VEC(LDA, M) column-major,
/// 0-based (VEC(k, i) is vec[(k-1) + (i-1)*lda]); hh is HH(1:M) 0-based; ind is the
/// Fortran 1-based column index. ops and ierr are INOUT/OUT as in the Fortran.
KOKKOS_INLINE_FUNCTION void mgsro(bool full, int lda, int n, int m, int ind, double& ops,
                                  double* vec, double* hh, int& ierr) {
  const double zero = 0.0;
  const double one = 1.0;
  // :2738, 0.98D0 (the comment at :2714 says 0.99**2 = 0.9801).
  const double reorth = 0.98;
  // gfortran's column stride is MAX(LDA, 0).
  const long ld = lda > 0 ? static_cast<long>(lda) : 0L;
  double* const vind = vec + static_cast<long>(ind - 1) * ld;

  // :2742-2744.
  double nrm0 = ddot(n, vind, vind);
  ops = ops + static_cast<double>(n) + static_cast<double>(n);
  double thr = nrm0 * reorth;
  // :2745-2753.
  if (nrm0 <= zero) {
    ierr = -1;
    return;
  } else if (nrm0 > zero && one / nrm0 > zero) {
    ierr = 0;
  } else {
    ierr = -2;
    return;
  }

  double fct;
  // :2757-2777.
  if (full) {
    for (int i = ind + 1; i <= m; ++i) {
      const double* const vi = vec + static_cast<long>(i - 1) * ld;
      fct = ddot(n, vind, vi);
      hh[i - 1] = fct;
      for (int k = 1; k <= n; ++k) vind[k - 1] = vind[k - 1] - fct * vi[k - 1];
      ops = ops + static_cast<double>(4 * n) + 2.0;
      if (fct * fct > thr) {
        fct = ddot(n, vind, vi);
        hh[i - 1] = hh[i - 1] + fct;
        for (int k = 1; k <= n; ++k) vind[k - 1] = vind[k - 1] - fct * vi[k - 1];
        ops = ops + static_cast<double>(4 * n) + 1.0;
      }
      nrm0 = nrm0 - hh[i - 1] * hh[i - 1];
      if (nrm0 < zero) nrm0 = zero;
      thr = nrm0 * reorth;
    }
  }

  // :2779-2797.
  for (int i = 1; i <= ind - 1; ++i) {
    const double* const vi = vec + static_cast<long>(i - 1) * ld;
    fct = ddot(n, vind, vi);
    hh[i - 1] = fct;
    for (int k = 1; k <= n; ++k) vind[k - 1] = vind[k - 1] - fct * vi[k - 1];
    ops = ops + static_cast<double>(4 * n) + 2.0;
    if (fct * fct > thr) {
      fct = ddot(n, vind, vi);
      hh[i - 1] = hh[i - 1] + fct;
      for (int k = 1; k <= n; ++k) vind[k - 1] = vind[k - 1] - fct * vi[k - 1];
      ops = ops + static_cast<double>(4 * n) + 1.0;
    }
    nrm0 = nrm0 - hh[i - 1] * hh[i - 1];
    if (nrm0 < zero) nrm0 = zero;
    thr = nrm0 * reorth;
  }

  // :2801-2807.
  const double nrm1 = Kokkos::sqrt(ddot(n, vind, vind));
  ops = ops + static_cast<double>(n) + static_cast<double>(n);
  hh[ind - 1] = nrm1;
  if (nrm1 <= zero) {
    ierr = -3;
    return;
  }

  // :2811-2815.
  fct = one / nrm1;
  for (int k = 1; k <= n; ++k) vind[k - 1] = vind[k - 1] * fct;
  ops = ops + static_cast<double>(n) + 1.0;

  // :2819.
  ierr = 0;
}

}  // namespace ww::profs
