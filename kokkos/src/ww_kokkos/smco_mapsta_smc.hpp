// kokkos/src/ww_kokkos/smco_mapsta_smc.hpp
// MAPSTA_SMC: wet coverage of each regular output cell by SMC sea points, and the land/sea mask
// MAPSMC it gives for ww3_ounf's regridded output. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3smcomd.F90:518-568 at NOAA-EMC/WW3 03adda91;
// author C. Bunney. Evidence: kokkos/port_report/MAPSTA_SMC_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::smco {

/// MAPSTA_SMC over the whole grid, serial: COV is a scatter sum in ISEA order (:529-552), so the
/// order of the additions into one cell is the Fortran's only on one thread.
/// nsea: NSEA. mapsf: MAPSF(MSEA,3) column-major, 0-based here, ld_mapsf its leading dimension
/// (MSEA, w3gdatmd.F90:1736). mapsta: MAPSTA(MY,MX) column-major, ld_mapsta = MY.
/// xidx, yidx, xspan, yspan, wts: XIDX(NSEA)... (w3smcomd.F90:93-97), 1-based in Fortran,
/// contiguous here; xidx/yidx hold 1-based output-grid indices or -1.
/// cov: COV(NXO,NYO), mapsmc: MAPSMC(NXO,NYO) (allocated at ww3_ounf.F90:603), column-major,
/// both written.
KOKKOS_INLINE_FUNCTION void mapsta_smc(int nsea, const int* mapsf, long ld_mapsf,
                                       const int* mapsta, long ld_mapsta, int nxo, int nyo,
                                       const int* xidx, const int* yidx, const int* xspan,
                                       const int* yspan, const Real* wts, Real* cov,
                                       int* mapsmc) {
  // COV(:,:) and MAPSMC(:,:) span the allocated extents, each clamped at 0 as gfortran's
  // descriptor does (ww3_ounf.F90:603): a negative NXO and NYO must not give a positive count.
  const long ncell = static_cast<long>(nxo > 0 ? nxo : 0) * static_cast<long>(nyo > 0 ? nyo : 0);
  for (long k = 0; k < ncell; ++k) cov[k] = static_cast<Real>(0.0);
  for (long k = 0; k < ncell; ++k) mapsmc[k] = 0;

  for (int isea = 1; isea <= nsea; ++isea) {
    const int imx = mapsf[isea - 1];
    const int imy = mapsf[(isea - 1) + ld_mapsf];

    if (xidx[isea - 1] == -1) continue;

    for (int i = 0; i <= xspan[isea - 1] - 1; ++i) {
      for (int j = 0; j <= yspan[isea - 1] - 1; ++j) {
        const int ix = xidx[isea - 1] + i;
        const int iy = yidx[isea - 1] + j;

        if (ix > nxo || iy > nyo) continue;

        if (mapsta[static_cast<long>(imy - 1) + ld_mapsta * static_cast<long>(imx - 1)] != 0) {
          const long c = static_cast<long>(ix - 1) + static_cast<long>(nxo) * static_cast<long>(iy - 1);
          cov[c] = cov[c] + wts[isea - 1];
        }
      }
    }
  }

  // A NaN coverage fails .LT. and gives 1, as the object's setbe after comiss does.
  for (int ix = 1; ix <= nxo; ++ix) {
    for (int iy = 1; iy <= nyo; ++iy) {
      const long c = static_cast<long>(ix - 1) + static_cast<long>(nxo) * static_cast<long>(iy - 1);
      if (cov[c] < static_cast<Real>(0.5)) {
        mapsmc[c] = 0;
      } else {
        mapsmc[c] = 1;
      }
    }
  }
}

}  // namespace ww::smco
