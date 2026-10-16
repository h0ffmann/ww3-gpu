// kokkos/src/ww_kokkos/srce_st3_sds3.hpp
// W3SDS3: WAM4/WAM4+ whitecapping dissipation (ST3), source term and diagonal
// term, one spectral point. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src3md.F90:1253-1433 at NOAA-EMC/WW3 03adda91;
// authors F. Ardhuin, H. L. Tolman. Evidence: kokkos/port_report/W3SDS3_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"

namespace ww::srce::st3 {

namespace sds3_detail {
// maxss of w3src3md.o, d the destination: a NaN WNMEAN or a tie gives s (:1382).
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }
}  // namespace sds3_detail

/// W3SDS3 for one point, arrays 0-based: a, s, d have NSPEC elements, k and cg NK.
/// cg, ustar, usdir, depth, ix, iy are read by no statement of the Fortran (STAB3 does
/// not change it). ssdsc1, ddelta1, ddelta2 are W3GDATMD's for the current grid.
KOKKOS_INLINE_FUNCTION void w3sds3_point(const Real* a, const Real* k, const Real* cg,
                                         Real emean, Real fmean, Real wnmean, Real ustar,
                                         Real usdir, Real depth, Real* s, Real* d, int ix,
                                         int iy, const Params& p, Real ssdsc1, Real ddelta1,
                                         Real ddelta2) {
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;

  // :1378.
  for (int is = 0; is < nspec; ++is) d[is] = static_cast<Real>(0.);

  // :1382-1384. TPI is constants.F90's 2*REAL(PI), as the object's constant.
  const Real wnmean2 = sds3_detail::maxss(static_cast<Real>(1.e-10), wnmean);
  const Real alfamean = (wnmean * wnmean) * emean;
  const Real factor = ((ssdsc1 * kTpi) * fmean) * (alfamean * alfamean);

  // :1394-1403.
  for (int ik = 1; ik <= nk; ++ik) {
    const Real kk = k[ik - 1];
    const Real q = kk / wnmean2;
    const Real factor2 = factor * ((ddelta1 * kk) / wnmean2 + ddelta2 * (q * q));
    for (int ith = 1; ith <= nth; ++ith) {
      const int is = ith + (ik - 1) * nth;
      d[is - 1] = factor2;
    }
  }

  // :1405.
  for (int is = 0; is < nspec; ++is) s[is] = d[is] * a[is];
}

}  // namespace ww::srce::st3
