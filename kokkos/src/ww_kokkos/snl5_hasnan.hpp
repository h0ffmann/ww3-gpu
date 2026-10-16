// kokkos/src/ww_kokkos/snl5_hasnan.hpp
// HasNaN of W3SNL5MD: true when a REAL ARR2D(NK,NTH) holds a NaN or an infinity.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3snl5md.F90:884-914 at NOAA-EMC/WW3 03adda91;
// author Q. Liu. Evidence: kokkos/port_report/HASNAN_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::snl5 {

namespace hasnan_detail {
/// HUGE(1.0) for default REAL: FLT_MAX, the literal the object compares with.
inline constexpr Real kHuge = 0x1.fffffep+127f;
}  // namespace hasnan_detail

/// HasNaN(NK, NTH, ARR2D), :907-910. arr2d is ARR2D(1:NK,1:NTH) in Fortran (column-major)
/// order, 0-based: element (ik,ith) at arr2d[(ith-1)*max(nk,0) + ik-1].
/// A +-Inf also gives true: the test is a range check, not isnan.
KOKKOS_INLINE_FUNCTION bool hasnan(int nk, int nth, const Real* arr2d) {
  const Real huge = hasnan_detail::kHuge;
  // Column stride MAX(NK,0), as the object computes it; an empty array gives ALL = .TRUE.
  const long stride = nk > 0 ? static_cast<long>(nk) : 0L;

  // First ALL, scanned to its first false (comiss: a NaN fails >=).
  bool all_ge = true;
  for (int ith = 0; ith < nth && all_ge; ++ith) {
    for (int ik = 0; ik < nk; ++ik) {
      if (!(arr2d[ith * stride + ik] >= -huge)) {
        all_ge = false;
        break;
      }
    }
  }
  // The object evaluates the second ALL whatever the first gave (no short circuit).
  bool all_le = true;
  for (int ith = 0; ith < nth && all_le; ++ith) {
    for (int ik = 0; ik < nk; ++ik) {
      if (!(arr2d[ith * stride + ik] <= huge)) {
        all_le = false;
        break;
      }
    }
  }
  return !(all_ge && all_le);
}

}  // namespace ww::snl5
