// kokkos/src/ww_kokkos/gig1_df1f2theta.hpp
// Df1f2theta: second-order coupling coefficient of the surface elevation between two wave
// components of radian frequencies s1, s2 at relative angle theta (Okihiro et al. 1992).
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3gig1md.F90:60-142 at NOAA-EMC/WW3 03adda91;
// authors A. Rawat and F. Ardhuin (module), adapted from Matlab by A. Rawat (2012).
// Evidence: kokkos/port_report/DF1F2THETA_KOKKOS.md.
// ⚠ On a device cosf and tanhf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::gig1 {

namespace df1f2theta_detail {
inline constexpr Real kGrav = static_cast<Real>(9.806);  // GRAV, constants.F90:62
// g2 = GRAV**2 (:124), folded by gfortran to .LC0 of w3gig1md.o, 0x42c050b5.
inline constexpr Real kG2 = 0x1.80a16ap+6f;
static_assert(kG2 == kGrav * kGrav);
}  // namespace df1f2theta_detail

/// Df1f2theta(s1,s2,WN1,WN2,theta,DEPTH), all REAL scalars; WN1, WN2 are only read.
/// The expressions follow the GIMPLE of w3gig1md.o: gfortran turns C1+C1b into C1b minus
/// the unnegated quotient (:130, :139), so C1 never exists as a value.
KOKKOS_INLINE_FUNCTION Real df1f2theta(Real s1, Real s2, Real wn1, Real wn2, Real theta,
                                       Real depth) {
  using df1f2theta_detail::kG2;
  using df1f2theta_detail::kGrav;
  const Real k1 = wn1;
  const Real k2 = wn2;
  const Real co = Kokkos::cos(theta);  // cosf
  const Real s3 = s1 + s2;
  const Real k1sq = k1 * k1;
  const Real k2sq = k2 * k2;
  const Real k3 = Kokkos::sqrt((k1sq + k2sq) + ((static_cast<Real>(2) * k1) * k2) * co);  // :126
  const Real sk2 = (kGrav * k3) * Kokkos::tanh(k3 * depth);  // :128, tanhf

  const Real kkco = (k1 * k2) * co;
  const Real s12 = s1 * s2;
  const Real c1q = kkco / s12;  // -C1 (:130)
  const Real s3sq = s3 * s3;
  const Real c1b = (s3sq - s12) / kG2;     // :131
  const Real c3 = (s3sq - sk2) * s1 * s2;  // :133
  // :137; s**3 is s*(s*s) in the object.
  const Real c4 = s3 * (kkco - (s12 * s12) / kG2) +
                  static_cast<Real>(0.5) *
                      ((s1 * k2sq + s2 * k1sq) - (s12 * (s2 * (s2 * s2) + s1 * (s1 * s1))) / kG2);
  // :139, C2 = s3 (:132).
  return kGrav * (static_cast<Real>(0.5) * (c1b - c1q) + (s3 * c4) / c3);
}

}  // namespace ww::gig1
