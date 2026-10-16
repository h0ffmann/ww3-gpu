// kokkos/src/ww_kokkos/smco_w3s2xy_smcnn.hpp
// W3S2XY_SMCNN: fill a regular output grid from the SMC sea point nearest to each cell,
// directions turned from radians to degrees nautical. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3smcomd.F90:711-743 at NOAA-EMC/WW3 03adda91;
// author C. Bunney. Evidence: kokkos/port_report/W3S2XY_SMCNN_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::smco {

namespace s2xy_smcnn_detail {
// RADE = 180. / PI (constants.F90:77), folded by gfortran to this float (w3smcomd.F90:736).
inline constexpr Real kRade = 0x1.ca5dc0p+5f;
static_assert(kRade == static_cast<Real>(180.) / kPi);
inline constexpr Real k630 = static_cast<Real>(630.);
inline constexpr Real k360 = static_cast<Real>(360.0);
}  // namespace s2xy_smcnn_detail

/// One output cell (ix, iy 1-based, as the Fortran loop counters), :724-739.
/// s: S(:) 1-based in Fortran, contiguous here (the shim packs a strided section).
/// xy: XY(NXO,NYO), nnidx: NNIDX(NXO,NYO), both column-major, 0-based here; nnidx holds a
/// 1-based ISEA or -1 for land. undef: UNDEF (constants.F90:85, a variable, not a parameter);
/// noval: NOVAL (w3smcomd.F90:110).
KOKKOS_INLINE_FUNCTION void w3s2xy_smcnn_point(int ix, int iy, const Real* s, Real* xy,
                                               bool dirn, int nxo, const int* nnidx,
                                               Real undef, Real noval) {
  using namespace s2xy_smcnn_detail;
  const long ixy = static_cast<long>(ix - 1) + static_cast<long>(nxo) * static_cast<long>(iy - 1);
  const int isea = nnidx[ixy];
  if (isea == -1) {
    xy[ixy] = undef;
  } else {
    const Real sv = s[isea - 1];
    if (sv == undef) {
      xy[ixy] = noval;
    } else {
      xy[ixy] = sv;
      if (dirn) {
        // MOD of REALs is fprem inline in the object (no libm call); fmod is exact, so any
        // correct fmodf gives the same bits.
        xy[ixy] = Kokkos::fmod(k630 - kRade * xy[ixy], k360);
      }
    }
  }
}

/// W3S2XY_SMCNN over the whole output grid, in the Fortran loop order (IX outer, IY inner,
/// :722-741). Arguments as in w3s2xy_smcnn_point; nyo: NYO (w3smcomd.F90:84).
KOKKOS_INLINE_FUNCTION void w3s2xy_smcnn(const Real* s, Real* xy, bool dirn, int nxo, int nyo,
                                         const int* nnidx, Real undef, Real noval) {
  for (int ix = 1; ix <= nxo; ++ix) {
    for (int iy = 1; iy <= nyo; ++iy) {
      w3s2xy_smcnn_point(ix, iy, s, xy, dirn, nxo, nnidx, undef, noval);
    }
  }
}

}  // namespace ww::smco
