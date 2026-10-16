// kokkos/src/ww_kokkos/srce_st1_sds1.hpp
// W3SDS1: WAM cycle 3 whitecapping dissipation, D = SDSC1*FMEAN*WNMEAN**3*EMEAN**2*K,
// S = D*A, one spectral point. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src1md.F90:432-586 at NOAA-EMC/WW3 03adda91;
// author H. L. Tolman. Evidence: kokkos/port_report/W3SDS1_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"

namespace ww::srce::st1 {

/// W3SDS1 for one point; a, k, s, d are NSPEC long, 0-based. sdsc1 is W3GDATMD SDSC1.
KOKKOS_INLINE_FUNCTION void w3sds1_point(const Real* a, const Real* k, Real emean, Real fmean,
                                         Real wnmean, Real* s, Real* d, const Params& p,
                                         Real sdsc1) {
  const int nspec = p.nspec;

  // :549. -O3 groups the product as ((SDSC1*FMEAN)*WNMEAN**3)*EMEAN**2 and expands
  // WNMEAN**3 as WNMEAN*(WNMEAN*WNMEAN) (powmult in the GIMPLE).
  const Real wn2 = wnmean * wnmean;
  const Real wn3 = wnmean * wn2;
  const Real e2 = emean * emean;
  const Real factor = ((sdsc1 * fmean) * wn3) * e2;

  // :557.
  for (int is = 0; is < nspec; ++is) d[is] = factor * k[is];
  // :558.
  for (int is = 0; is < nspec; ++is) s[is] = d[is] * a[is];
}

}  // namespace ww::srce::st1
