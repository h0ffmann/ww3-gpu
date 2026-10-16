// kokkos/src/ww_kokkos/smco_w3s2xy_smcrg_dir.hpp
// W3S2XY_SMCRG_DIR: area-weighted regridding of a directional field from the SMC grid to a
// regular output grid through its cos/sin components, result in degrees nautical.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3smcomd.F90:438-510 at NOAA-EMC/WW3 03adda91;
// author C. Bunney. Evidence: kokkos/port_report/W3S2XY_SMCRG_DIR_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "smco_w3s2xy_smcnn.hpp"

namespace ww::smco {

/// W3S2XY_SMCRG_DIR over the whole output grid, serial in the Fortran loop order.
/// s: S(:) 1-based in Fortran, contiguous here (the shim packs a strided section).
/// xy: XY(NXO,NYO), column-major, 0-based here.
/// Module state (w3smcomd.F90:83-113): nsmc NSMC; smcidx SMCIDX(NSMC), holding 1-based ISEA;
/// xidx, yidx, xspan, yspan, wts: XIDX, YIDX, XSPAN, YSPAN, WTS indexed by ISEA (1-based in
/// Fortran, 0-based here); mapsmc MAPSMC(NXO,NYO); undef UNDEF (constants.F90:85, a variable);
/// noval NOVAL. cov: COV, written; allocated (NXO,NYO) at ww3_ounf.F90:603, which this
/// signature assumes. aux1, aux2: caller-provided work arrays of NXO*NYO, the ALLOCATE of :452.
KOKKOS_INLINE_FUNCTION void w3s2xy_smcrg_dir(const Real* s, Real* xy, int nxo, int nyo, int nsmc,
                                             const int* smcidx, const int* xidx, const int* yidx,
                                             const int* xspan, const int* yspan, const Real* wts,
                                             const int* mapsmc, Real undef, Real noval, Real* cov,
                                             Real* aux1, Real* aux2) {
  using namespace s2xy_smcnn_detail;
  const long n = static_cast<long>(nxo) * static_cast<long>(nyo);
  for (long k = 0; k < n; ++k) cov[k] = static_cast<Real>(0.0);
  for (long k = 0; k < n; ++k) xy[k] = static_cast<Real>(0.0);
  for (long k = 0; k < n; ++k) aux1[k] = static_cast<Real>(0.0);
  for (long k = 0; k < n; ++k) aux2[k] = static_cast<Real>(0.0);

  for (int ismc = 1; ismc <= nsmc; ++ismc) {
    const int isea = smcidx[ismc - 1];
    const Real sv = s[isea - 1];
    if (sv == undef) continue;
    // gfortran merges COS and SIN into one sincosf call; g++ does the same (port report).
    const Real coss = Kokkos::cos(sv);
    const Real sins = Kokkos::sin(sv);
    const Real w = wts[isea - 1];
    for (int i = 0; i <= xspan[isea - 1] - 1; ++i) {
      for (int j = 0; j <= yspan[isea - 1] - 1; ++j) {
        const int ix = xidx[isea - 1] + i;
        const int iy = yidx[isea - 1] + j;
        if (ix > nxo || iy > nyo) continue;
        const long ixy =
            static_cast<long>(ix - 1) + static_cast<long>(nxo) * static_cast<long>(iy - 1);
        // gfortran's mulss takes WTS as first operand, g++ takes COSS whatever the source order:
        // only the payload of a NaN times a NaN can differ (port report).
        aux1[ixy] = aux1[ixy] + coss * w;
        aux2[ixy] = aux2[ixy] + sins * w;
        cov[ixy] = cov[ixy] + w;
      }
    }
  }

  // :487-506; the COV < 1 and the ELSE branches are the same statements (gfortran merges them).
  for (int ix = 1; ix <= nxo; ++ix) {
    for (int iy = 1; iy <= nyo; ++iy) {
      const long ixy =
          static_cast<long>(ix - 1) + static_cast<long>(nxo) * static_cast<long>(iy - 1);
      if (mapsmc[ixy] == 0) {
        xy[ixy] = undef;
      } else if (cov[ixy] < static_cast<Real>(0.5)) {
        xy[ixy] = noval;
      } else if (cov[ixy] < static_cast<Real>(1.0)) {
        xy[ixy] = Kokkos::atan2(aux2[ixy], aux1[ixy]);
        xy[ixy] = Kokkos::fmod(k630 - kRade * xy[ixy], k360);
      } else {
        xy[ixy] = Kokkos::atan2(aux2[ixy], aux1[ixy]);
        xy[ixy] = Kokkos::fmod(k630 - kRade * xy[ixy], k360);
      }
    }
  }
}

}  // namespace ww::smco
