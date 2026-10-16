// kokkos/src/ww_kokkos/wavset_fd_wave_setup_scalar_prod.hpp
// FD_WAVE_SETUP_SCALAR_PROD: dot product of two NX vectors, used by the conjugate gradient
// of the wave setup Poisson solver on finite-difference grids. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3wavset.F90:2655-2729 at NOAA-EMC/WW3 03adda91;
// authors M. Dutour-Sikiric, A. Roland.
// Evidence: kokkos/port_report/FD_WAVE_SETUP_SCALAR_PROD_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::wavset {

/// eScal = sum over IP=1..NX of V1(IP)*V2(IP), REAL(rkind) with rkind = 4 (yowdatapool.F90:48).
/// v1, v2: Fortran V1(NX), V2(NX), 1-based there, 0-based here; nx is W3GDATMD NX.
/// The sum runs in IP order from +0: gfortran -O3 vectorises the products but adds them one
/// by one (addss), so the serial loop here has the object's rounding.
KOKKOS_INLINE_FUNCTION void fd_wave_setup_scalar_prod(const float* v1, const float* v2,
                                                      float& escal, int nx) {
  float e = 0.0f;
  for (int ip = 0; ip < nx; ++ip) {
    e = e + v1[ip] * v2[ip];
  }
  escal = e;
}

}  // namespace ww::wavset
