// kokkos/src/ww_kokkos/srce_ic1.hpp
// W3SIC1: wave attenuation by sea ice with a decay rate uniform in frequency,
// D = -2*Cg*k_i, one spectral point. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic1md.F90:93-369 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, S. Zieger. Evidence: kokkos/port_report/W3SIC1_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"

namespace ww::srce {

namespace ic1_detail {
/// Returned where the Fortran stops in EXTCDE(2) (INFLAGS2(-7) false, :299-302): the
/// kernel cannot stop the run, the shim must.
inline constexpr int kExtcde = 2;
}  // namespace ic1_detail

/// W3SIC1 for one point, arrays 0-based; mapwn values are 1-based IK; d1d (NK) is work.
/// Returns 0, or kExtcde with d = 0 (:289) and s not written.
KOKKOS_INLINE_FUNCTION int w3sic1_point(const Params& p, Real icep1, bool inflag_icep1,
                                        const int* mapwn, const Real* a, const Real* cg,
                                        Real* s, Real* d, Real* d1d) {
  const int nk = p.nk;
  const int nspec = p.nspec;
  const Real zero = static_cast<Real>(0.);

  // :289.
  for (int is = 0; is < nspec; ++is) d[is] = zero;

  // :299-302.
  if (!inflag_icep1) return ic1_detail::kExtcde;

  // :304.
  const Real icecoef1 = icep1;

  if (icecoef1 == zero) {
    // :310.
    for (int is = 0; is < nspec; ++is) d[is] = zero;
  } else {
    // :324-330. WN_I = ICECOEF1.
    const Real wn_i = icecoef1;
    for (int ik = 1; ik <= nk; ++ik) {
      d1d[ik - 1] = -((static_cast<Real>(2.) * cg[ik - 1]) * wn_i);
    }
    // :334-336. MAPWN holds 1-based IK.
    for (int ikth = 0; ikth < nspec; ++ikth) d[ikth] = d1d[mapwn[ikth] - 1];
  }

  // :340.
  for (int is = 0; is < nspec; ++is) s[is] = d[is] * a[is];
  return 0;
}

}  // namespace ww::srce
