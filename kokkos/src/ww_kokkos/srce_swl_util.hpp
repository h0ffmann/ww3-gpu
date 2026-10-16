// kokkos/src/ww_kokkos/srce_swl_util.hpp
// IRANGE of W3SWLDMD: the integer sequence X0, X0+DX, ... used as an index vector
// (IKN = IRANGE(1,NSPEC,NTH), the first direction of each wavenumber).
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3swldmd.F90:456-486 at NOAA-EMC/WW3 03adda91;
// authors H. L. Tolman, S. Zieger. Evidence: kokkos/port_report/W3SWLDMD_IRANGE_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"  // fint

namespace ww::srce::swl {

/// IRANGE(X0,X1,DX). The Fortran returns an ALLOCATABLE IX(1:N); here the caller
/// owns the storage. Returns SIZE(IX), that is max(N,0) with
/// N = INT(REAL(X1-X0)/REAL(DX))+1 (:480). When ix is not null, writes
/// ix[i-1] = X0+(i-1)*DX for i = 1..N (:482-484), so ix must hold the returned
/// count: call once with ix = nullptr to size it. Values are the Fortran's
/// (1-based where X0 = 1). W3SWL4 (:192) and W3SWL6 (:371) call it with
/// (1, NSPEC, NTH) once (SAVE) and stop when the count is not NK.
KOKKOS_INLINE_FUNCTION int irange(int x0, int x1, int dx, int* ix) {
  // :480. N is computed in REAL: int->float rounding, a true division, and
  // cvttss2si (INT_MIN when DX = 0 or out of range, then N <= 0, empty IX).
  const int n = ww::st4::fint(static_cast<Real>(x1 - x0) / static_cast<Real>(dx)) + 1;
  if (ix != nullptr) {
    for (int i = 1; i <= n; ++i) ix[i - 1] = x0 + (i - 1) * dx;
  }
  return n > 0 ? n : 0;
}

}  // namespace ww::srce::swl
