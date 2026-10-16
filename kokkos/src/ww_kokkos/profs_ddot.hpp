// kokkos/src/ww_kokkos/profs_ddot.hpp
// DDOT: the LINPACK dot product of two REAL(8) vectors, unrolled by 5, used by the
// SPARSKIT solvers inside W3PROFSMD. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:4684-4710 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (the routine is J. Dongarra's, LINPACK 1978).
// Evidence: kokkos/port_report/DDOT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// DDOT(N, DX, DY); dx, dy are DX(1:N), DY(1:N) 0-based. Unit stride only, as the Fortran.
/// The sum is strictly left to right: the object vectorises the products of the first loop
/// (mulpd) but keeps the adds scalar and in order, so no reassociation is allowed here.
KOKKOS_INLINE_FUNCTION double ddot(int n, const double* dx, const double* dy) {
  // The function result DDOT.
  double r = 0.0;
  // :4695.
  if (n <= 0) return r;

  // :4697. n > 0, so % is MOD.
  const int m = n % 5;
  if (m != 0) {
    // :4699-4701.
    for (int i = 1; i <= m; ++i) r = r + dx[i - 1] * dy[i - 1];
    // :4702.
    if (n < 5) return r;
  }
  // :4704-4708. Left-associative, as the object adds it.
  const int mp1 = m + 1;
  for (int i = mp1; i <= n; i += 5) {
    r = r + dx[i - 1] * dy[i - 1] + dx[i] * dy[i] + dx[i + 1] * dy[i + 1] +
        dx[i + 2] * dy[i + 2] + dx[i + 3] * dy[i + 3];
  }
  return r;
}

}  // namespace ww::profs
