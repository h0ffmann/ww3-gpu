// kokkos/src/ww_kokkos/ic5_balancing_matrix.hpp
// BALANCING_MATRIX of W3SIC5MD: balance a square REAL matrix in place by powers of the radix
// (Numerical Recipes' balanc), before the eigenvalues of POLYROOTS' companion matrix.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic5md.F90:956-1109 at NOAA-EMC/WW3 03adda91;
// author Q. Liu (module authors Q. Liu, E. Rogers).
// Evidence: kokkos/port_report/BALANCING_MATRIX_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::ic5 {

/// BALANCING_MATRIX(NMAT, MATRIX), :956-1109. `matrix` is MATRIX(NMAT, NMAT), column-major,
/// Fortran base 1: MATRIX(I, J) is matrix[(I-1) + (J-1)*nmat]. RADX = RADIX(MATRIX) = 2.0 and
/// SQRADX = 4.0 (:1060-1061); gfortran turns /RADX, /SQRADX into *0.5, *0.25 and *RADX into
/// C+C, all exact, as g++ does.
/// ⚠ A C or R that is NaN or infinite never leaves the radix loops (:1082-1093), as in the Fortran.
KOKKOS_INLINE_FUNCTION void balancing_matrix(int nmat, float* matrix) {
  constexpr float radx = 2.0f;
  constexpr float sqradx = radx * radx;
  const long ld = nmat > 0 ? nmat : 0;
  while (true) {
    int last = 1;
    for (int i = 0; i < nmat; ++i) {
      // :1073-1074, SUM in index order from 0.0 (the object sums lane by lane, in order).
      float c = 0.0f;
      for (int k = 0; k < nmat; ++k) c = c + Kokkos::fabs(matrix[k + i * ld]);
      c = c - matrix[i + i * ld];
      float r = 0.0f;
      for (int k = 0; k < nmat; ++k) r = r + Kokkos::fabs(matrix[i + k * ld]);
      r = r - matrix[i + i * ld];
      if (c != 0.0f && r != 0.0f) {
        float g = r / radx;
        float f = 1.0f;
        const float s = c + r;
        while (true) {
          if (c >= g) break;
          f = f * radx;
          c = c * sqradx;
        }
        g = r * radx;
        while (true) {
          if (c <= g) break;
          f = f / radx;
          c = c / sqradx;
        }
        // :1095, 0.95 is the default REAL 0.95f.
        if ((c + r) / f < 0.95f * s) {
          last = 0;
          g = 1.0f / f;
          for (int k = 0; k < nmat; ++k) matrix[i + k * ld] = matrix[i + k * ld] * g;
          for (int k = 0; k < nmat; ++k) matrix[k + i * ld] = matrix[k + i * ld] * f;
        }
      }
    }
    if (last != 0) break;
  }
}

}  // namespace ww::ic5
