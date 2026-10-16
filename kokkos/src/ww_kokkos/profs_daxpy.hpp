// kokkos/src/ww_kokkos/profs_daxpy.hpp
// DAXPY: the LINPACK y := y + a*x on two REAL(8) vectors, unrolled by 4, used by the
// SPARSKIT solver PGMRES inside W3PROFSMD. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:4712-4761 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, F. Ardhuin (the routine is J. Dongarra's, LINPACK 1978).
// Evidence: kokkos/port_report/DAXPY_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::profs {

/// DAXPY(N, DA, DX, INCX, DY, INCY); dx, dy are the assumed-size DX(1), DY(1) 0-based.
/// Every update is element-wise (no reduction), so the object's 2-wide vectorised clean-up
/// loop gives the same bits as this scalar loop.
KOKKOS_INLINE_FUNCTION void daxpy(int n, double da, const double* dx, int incx, double* dy,
                                  int incy) {
  // :4722.
  if (n <= 0) return;
  // :4723. TINY(1.D0) = 2**-1022, DBL_MIN.
  if (Kokkos::fabs(da) < 0x1p-1022) return;
  if (incx != 1 || incy != 1) {
    // :4729-4738.
    int ix = 1;
    int iy = 1;
    if (incx < 0) ix = (-n + 1) * incx + 1;
    if (incy < 0) iy = (-n + 1) * incy + 1;
    for (int i = 1; i <= n; ++i) {
      dy[iy - 1] = dy[iy - 1] + da * dx[ix - 1];
      ix = ix + incx;
      iy = iy + incy;
    }
    return;
  }
  // :4746. n > 0, so % is MOD.
  const int m = n % 4;
  if (m != 0) {
    // :4748-4750.
    for (int i = 1; i <= m; ++i) dy[i - 1] = dy[i - 1] + da * dx[i - 1];
    // :4751.
    if (n < 4) return;
  }
  // :4753-4759.
  const int mp1 = m + 1;
  for (int i = mp1; i <= n; i += 4) {
    dy[i - 1] = dy[i - 1] + da * dx[i - 1];
    dy[i] = dy[i] + da * dx[i];
    dy[i + 1] = dy[i + 1] + da * dx[i + 1];
    dy[i + 2] = dy[i + 2] + da * dx[i + 2];
  }
}

}  // namespace ww::profs
