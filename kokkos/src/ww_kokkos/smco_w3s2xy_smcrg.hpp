// kokkos/src/ww_kokkos/smco_w3s2xy_smcrg.hpp
// W3S2XY_SMCRG: area-weighted regridding of a scalar field from the SMC grid onto the regular
// output grid, with the coastline masked by wet coverage. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3smcomd.F90:359-418 at NOAA-EMC/WW3 03adda91;
// author C. Bunney. Evidence: kokkos/port_report/W3S2XY_SMCRG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::smco {

/// Masking of one output cell (ix, iy 1-based, as the Fortran loop counters), :401-412.
/// xy, cov, mapsmc: XY, COV, MAPSMC, all (NXO,NYO) column-major, 0-based here.
KOKKOS_INLINE_FUNCTION void w3s2xy_smcrg_mask_point(int ix, int iy, Real* xy, int nxo,
                                                    const Real* cov, const int* mapsmc,
                                                    Real undef, Real noval) {
  const long ixy = static_cast<long>(ix - 1) + static_cast<long>(nxo) * static_cast<long>(iy - 1);
  if (mapsmc[ixy] == 0) {
    xy[ixy] = undef;
  } else if (cov[ixy] < static_cast<Real>(0.5)) {
    xy[ixy] = noval;
  } else if (cov[ixy] < static_cast<Real>(1.0)) {
    // The Fortran parentheses keep 1/COV then a multiply, never XY/COV (:411).
    xy[ixy] = xy[ixy] * (static_cast<Real>(1.0) / cov[ixy]);
  }
}

/// W3S2XY_SMCRG over the whole grid, serial: XY and COV are scatter sums in ISMC order
/// (:374-396), so the order of the additions into one cell is the Fortran's only on one thread.
/// s: S(:) 1-based in Fortran, contiguous here (the shim packs a strided section).
/// xy: XY(NXO,NYO) column-major, 0-based. nsmc, smcidx: NSMC, SMCIDX(:) (w3smcomd.F90:101,120),
/// smcidx holds 1-based ISEA. xidx, yidx, xspan, yspan, wts: XIDX(:)... (:93-97) indexed by
/// ISEA, 1-based in Fortran, contiguous here; xidx/yidx are 1-based output-grid indices, at
/// least 1 for every ISEA in SMCIDX (:303-313). cov: COV(NXO,NYO), written; mapsmc:
/// MAPSMC(NXO,NYO) (both allocated at ww3_ounf.F90:603). undef: UNDEF (constants.F90:85, a
/// variable); noval: NOVAL (w3smcomd.F90:110).
KOKKOS_INLINE_FUNCTION void w3s2xy_smcrg(const Real* s, Real* xy, int nxo, int nyo, int nsmc,
                                         const int* smcidx, const int* xidx, const int* yidx,
                                         const int* xspan, const int* yspan, const Real* wts,
                                         Real* cov, const int* mapsmc, Real undef, Real noval) {
  const long ncell = static_cast<long>(nxo) * static_cast<long>(nyo);
  for (long k = 0; k < ncell; ++k) cov[k] = static_cast<Real>(0.0);
  for (long k = 0; k < ncell; ++k) xy[k] = static_cast<Real>(0.0);

  for (int ismc = 1; ismc <= nsmc; ++ismc) {
    const int isea = smcidx[ismc - 1];
    const Real sv = s[isea - 1];
    if (sv == undef) continue;
    for (int i = 0; i <= xspan[isea - 1] - 1; ++i) {
      for (int j = 0; j <= yspan[isea - 1] - 1; ++j) {
        const int ix = xidx[isea - 1] + i;
        const int iy = yidx[isea - 1] + j;
        if (ix > nxo || iy > nyo) continue;
        const long ixy =
            static_cast<long>(ix - 1) + static_cast<long>(nxo) * static_cast<long>(iy - 1);
        xy[ixy] = xy[ixy] + sv * wts[isea - 1];
        cov[ixy] = cov[ixy] + wts[isea - 1];
      }
    }
  }

  for (int ix = 1; ix <= nxo; ++ix) {
    for (int iy = 1; iy <= nyo; ++iy) {
      w3s2xy_smcrg_mask_point(ix, iy, xy, nxo, cov, mapsmc, undef, noval);
    }
  }
}

}  // namespace ww::smco
