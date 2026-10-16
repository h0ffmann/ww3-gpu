// kokkos/src/ww_kokkos/uost_compute_reduction_psi.hpp
// COMPUTE_REDUCTION_PSI: the wind-sea reduction factor PSI of the unresolved-obstacles source
// term, a ramp in the wave age from 0 at 0.5 to 1 at 1.5.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3uostmd.F90:688-770 at NOAA-EMC/WW3 03adda91;
// author L. Mentaschi. Evidence: kokkos/port_report/COMPUTE_REDUCTION_PSI_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::uost {

/// COMPUTE_REDUCTION_PSI. dt is a dummy the Fortran never reads; it is kept for the signature.
KOKKOS_INLINE_FUNCTION void compute_reduction_psi(Real u10abs, Real u10dir, Real cgabs,
                                                  Real cgdir, Real dt, Real& psi) {
  constexpr Real tolerance = 0.000001f;
  constexpr Real whthr1 = 0.5f;
  constexpr Real whthr2 = 1.5f;
  // 2*PI and PI/2 as gfortran folds them (:747, :750): one REAL product each, exact.
  constexpr Real tpi = 2.0f * kPi;
  constexpr Real hpi = kPi / 2.0f;
  static_assert(tpi == 0x1.921fb6p+2f && hpi == 0x1.921fb6p+0f);

  Real thdelta = Kokkos::fabs(u10dir - cgdir);
  while (thdelta > kPi) thdelta = thdelta - tpi;
  thdelta = Kokkos::fabs(thdelta);
  Real wa;
  if (hpi - thdelta > tolerance) {
    const Real cp = cgabs * 2.0f;
    // Both compilers rewrite COS(ABS(x)) as cosf(x) on the value before :749's ABS.
    wa = cp / u10abs / Kokkos::cos(thdelta);
  } else {
    wa = 9999999.0f;
  }

  // A NaN wave age falls through to PSI = 1, as in the object.
  if (wa <= whthr1) {
    psi = 0.0f;
  } else if ((wa > whthr1) && (wa < whthr2)) {
    psi = (wa - whthr1) / (whthr2 - whthr1);
  } else {
    psi = 1.0f;
  }
}

}  // namespace ww::uost
