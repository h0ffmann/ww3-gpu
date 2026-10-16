// kokkos/src/ww_kokkos/srce_sign_vsd_semi_implicit_ww3.hpp
// SIGN_VSD_SEMI_IMPLICIT_WW3: clip the diagonal source term to VD <= 0, one spectral
// point. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3srcemd.F90:2559-2633 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, M. Dutour-Sikiric. Evidence:
// kokkos/port_report/SIGN_VSD_SEMI_IMPLICIT_WW3_KOKKOS.md.
// Its only calls (:1721, :1723) sit under IF (.not. LSLOC), and LSLOC is a PARAMETER
// .true. (w3parall.F90:89): no build of WW3 ever runs it.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::srce {

namespace sign_vsd_detail {
// MIN(0., VD) as the object computes it. The build uses two operand orders, which
// differ only for a NaN or a -0. VD: min_vd_first is minps/minss with VD as the
// destination (NaN or -0 give +0); min_zero_first has 0. as the destination (NaN or
// -0 give VD back).
KOKKOS_INLINE_FUNCTION Real min_vd_first(Real v) {
  return v < static_cast<Real>(0.) ? v : static_cast<Real>(0.);
}
KOKKOS_INLINE_FUNCTION Real min_zero_first(Real v) {
  return static_cast<Real>(0.) < v ? static_cast<Real>(0.) : v;
}
}  // namespace sign_vsd_detail

/// SIGN_VSD_SEMI_IMPLICIT_WW3, arrays 0-based (Fortran SPEC, VS, VD are 1:NSPEC).
/// SPEC and VS are not read or written, as in the Fortran.
KOKKOS_INLINE_FUNCTION void sign_vsd_semi_implicit_ww3(const Real* spec, Real* vs, Real* vd,
                                                       int nspec) {
  using sign_vsd_detail::min_vd_first;
  using sign_vsd_detail::min_zero_first;
  // :2630-2632. The per-index operand order follows w3srcemd.o: a scalar loop when
  // NSPEC <= 3 (or NSPEC = 2**31-1), else a 4-wide minps body and an unrolled tail
  // whose third element swaps the operands.
  if (nspec <= 0) return;
  const unsigned un = static_cast<unsigned>(nspec);
  if (un - 1u <= 2u || nspec == 2147483647) {
    for (int is = 0; is < nspec; ++is) vd[is] = min_zero_first(vd[is]);
    return;
  }
  const int n4 = static_cast<int>((un >> 2) * 4u);
  for (int is = 0; is < n4; ++is) vd[is] = min_vd_first(vd[is]);
  if (n4 == nspec) return;
  vd[n4] = min_vd_first(vd[n4]);
  if (nspec < n4 + 2) return;
  vd[n4 + 1] = min_vd_first(vd[n4 + 1]);
  if (nspec < n4 + 3) return;
  vd[n4 + 2] = min_zero_first(vd[n4 + 2]);
}

}  // namespace ww::srce
