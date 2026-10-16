// kokkos/src/ww_kokkos/ic3_delta.hpp
// DELTA: bin widths of a discretised function (one-sided at the ends, centred inside), used by
// the IC3 group velocity Cg = DELTA(SIGMA) / DELTA(WN_R). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic3md.F90:1732-1790 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, S. Zieger. Evidence: kokkos/port_report/DELTA_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::ic3 {

/// DELTA. x: Fortran X(1:NX), here 0-based and contiguous (the Fortran's assumed-shape X also
/// takes a strided section; at :827 it gets SIGMA and the dummy WN_R(:)). dx (nx) is the Fortran's ALLOCATABLE result DX
/// (:1777), provided by the caller; it must not overlap x (the Fortran result is a fresh
/// allocation: the zeroing at :1778 is dead there, a memset here). For nx == 1 the Fortran
/// reads X(2), outside X (:1782); the caller must pass nx >= 2.
KOKKOS_INLINE_FUNCTION void delta(const Real* x, int nx, Real* dx) {
  const Real zero = static_cast<Real>(0.);
  const Real two = static_cast<Real>(2.);
  // :1778.
  for (int ix = 0; ix < nx; ++ix) dx[ix] = zero;
  // ix is the Fortran's 1-based index; X(i) is x[i - 1].
  for (int ix = 1; ix <= nx; ++ix) {
    if (ix == 1) {
      dx[ix - 1] = x[ix] - x[ix - 1];
    } else if (ix == nx) {
      dx[ix - 1] = x[ix - 1] - x[ix - 2];
    } else {
      // :1786. Both compilers fold / 2. into the exact * 0.5.
      dx[ix - 1] = (x[ix] - x[ix - 2]) / two;
    }
  }
}

}  // namespace ww::ic3
