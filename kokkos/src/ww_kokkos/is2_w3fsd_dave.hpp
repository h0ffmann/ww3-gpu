// kokkos/src/ww_kokkos/is2_w3fsd_dave.hpp
// W3FSD_DAVE of W3SIS2MD: mean floe diameter of a truncated power-law floe size
// distribution, from the minimum and maximum floe diameters and the fragility.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sis2md.F90:1317-1405 at NOAA-EMC/WW3 03adda91;
// authors C. Sevigny, F. Ardhuin. Evidence: kokkos/port_report/W3FSD_DAVE_KOKKOS.md.
// ⚠ On a device logf and powf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>
#include <cmath>

namespace ww::is2 {

namespace w3fsd_dave_detail {
// 1.01 in REAL, :1390.
inline constexpr float kMinFactor = 0x1.028f5cp+0f;
static_assert(kMinFactor == 1.01f);
// LOG(2.) folded by gfortran to the REAL nearest ln 2 (GIMPLE 6.93147182464599609375e-1), :1391.
inline constexpr float kLog2 = 0x1.62e43p-1f;
static_assert(kLog2 == static_cast<float>(0.693147180559945309417232121458176568));
}  // namespace w3fsd_dave_detail

/// W3FSD_DAVE(ICEDMIN, ICEDMAX, FRAGILITY), :1317-1405.
/// kIcedmaxFirst picks the MAX operand order of :1390 per call site, as the object has it:
/// false (default) is the standalone function and its copy inlined in W3SIS2 (:850), which
/// return ICEDMAX when the comparison is unordered or equal; true is the copy inlined in
/// INSIS2 (:582), which returns ICEDMIN*1.01 then. They differ when either operand is NaN or
/// on a tie of +0 and -0; at INSIS2's site ICEDMAX = REAL(I) >= 1, so only a NaN ICEDMIN.
template <bool kIcedmaxFirst = false>
KOKKOS_INLINE_FUNCTION float w3fsd_dave(float icedmin, float icedmax, float fragility) {
  using namespace w3fsd_dave_detail;
  const float scaled = icedmin * kMinFactor;
  float icedmaxl;
  if constexpr (kIcedmaxFirst) {
    icedmaxl = icedmax > scaled ? icedmax : scaled;  // maxss scaled, icedmax
  } else {
    icedmaxl = scaled > icedmax ? scaled : icedmax;  // maxss icedmax, scaled
  }
  if (icedmin == 0.0f) return icedmaxl;  // :1392, tests ICEDMIN, not ICEDMAXL
  const float gam = std::log(fragility) / kLog2 + 2.0f;  // :1391, logf
  // :1395-1396. -GAM+1 is emitted as 1-GAM; the four powers are powf calls.
  const float e1 = 1.0f - gam;
  const float e0 = -gam;
  const float coef = gam / (gam - 1.0f);
  const float num = std::pow(icedmaxl, e1) - std::pow(icedmin, e1);
  const float den = std::pow(icedmaxl, e0) - std::pow(icedmin, e0);
  const float q = num / den;
  return q * coef;  // the object multiplies with the quotient as first source
}

}  // namespace ww::is2
