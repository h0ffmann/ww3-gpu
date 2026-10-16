// kokkos/src/ww_kokkos/srce_flx4.hpp
// W3FLX4: friction velocity, drag coefficient and roughness length from the 10 m wind after
// Hwang (2011), one sea point. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3flx4md.F90:105-231 at NOAA-EMC/WW3 03adda91;
// authors H. L. Tolman, S. Zieger, Q. Liu. Evidence: kokkos/port_report/W3FLX4_KOKKOS.md.
// ⚠ On a device sqrtf and expf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::srce::flx {

namespace flx4_detail {
/// Returned where the Fortran stops in EXTCDE(1) (:199-202): the kernel cannot write the
/// message to NDSE (:200) nor stop the run, the shim must.
inline constexpr int kExtcde = 1;
}  // namespace flx4_detail

/// W3FLX4 for one point. flx4a0 is W3GDATMD FLX4A0 of the current grid (w3gdatmd.F90:2610).
/// Returns 0, or kExtcde with ust, ustd, z0, cd not written.
KOKKOS_INLINE_FUNCTION int w3flx4_point(Real zwnd, Real u10, Real u10d, Real& ust, Real& ustd,
                                        Real& z0, Real& cd, Real flx4a0) {
  // :199. Single-precision literals, as gfortran reads them (.rodata of w3flx4md.o).
  if (Kokkos::fabs(zwnd - 10.f) > 0.01f) return flx4_detail::kExtcde;

  // :210-216. The object keeps U10**2 as U10*U10 and FLX4A0*(...) before *1E-4.
  if (u10 >= 50.33f) {
    ust = 2.026f * Kokkos::sqrt(flx4a0);
    const Real r = ust / u10;
    cd = r * r;
  } else {
    cd = ((flx4a0 * ((8.058f + 0.967f * u10) - 0.016f * (u10 * u10))) * 1E-4f);
    ust = u10 * Kokkos::sqrt(cd);
  }

  // :218-219. -0.4/SQRT(CD) is -(0.4/SQRT(CD)), a divss then a sign flip in the object.
  z0 = zwnd * Kokkos::exp(-(0.4f / Kokkos::sqrt(cd)));
  ustd = u10d;
  return 0;
}

}  // namespace ww::srce::flx
