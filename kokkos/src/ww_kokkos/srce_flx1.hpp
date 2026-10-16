// kokkos/src/ww_kokkos/srce_flx1.hpp
// W3FLX1: friction velocity, roughness length and drag coefficient from U10 after
// Wu (1980), one point. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3flx1md.F90:88-200 at NOAA-EMC/WW3 03adda91;
// author H. L. Tolman. Evidence: kokkos/port_report/W3FLX1_KOKKOS.md.
// ⚠ On a device expf and sqrtf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::srce::flx {

namespace flx1_detail {
/// Returned where the Fortran stops in EXTCDE(1) (ZWND not 10 m, :178-181): the kernel
/// cannot print or stop the run, the shim must (it writes format 1000 with ZWND).
inline constexpr int kExtcde = 1;
}  // namespace flx1_detail

/// W3FLX1 for one point. Returns 0, or kExtcde with ust, ustd, z0 and cd not written.
KOKKOS_INLINE_FUNCTION int w3flx1_point(Real zwnd, Real u10, Real u10d, Real& ust,
                                        Real& ustd, Real& z0, Real& cd) {
  // :178. Compared in REAL: 10. and 0.01 are default-REAL literals.
  if (Kokkos::fabs(zwnd - static_cast<Real>(10.)) > static_cast<Real>(0.01)) {
    return flx1_detail::kExtcde;
  }

  // :185-188. The object computes SQRT(CD) once (sqrtss) and reuses it for UST;
  // -0.4/SQRT(CD) is 0.4/sqrt then a sign flip, both exact either way.
  const Real cdl = static_cast<Real>(0.001) *
                   (static_cast<Real>(0.8) + static_cast<Real>(0.065) * u10);
  cd = cdl;
  const Real sqcd = Kokkos::sqrt(cdl);
  z0 = zwnd * Kokkos::exp(-(static_cast<Real>(0.4) / sqcd));
  ust = u10 * sqcd;
  ustd = u10d;
  return 0;
}

}  // namespace ww::srce::flx
