// kokkos/src/ww_kokkos/smco_w3s2xy_smcnn_int.hpp
// W3S2XY_SMCNN_INT: fill a regular output grid from the nearest SMC sea point, corrected by
// the cell's local gradient times the distance between cell centres; directions turned from
// radians to degrees nautical. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3smcomd.F90:762-798 at NOAA-EMC/WW3 03adda91;
// author C. Bunney. Evidence: kokkos/port_report/W3S2XY_SMCNN_INT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "smco_w3s2xy_smcnn.hpp"

namespace ww::smco {

/// :778, CVQ(1:NSEA) = S. cvq: CVQ(-9:NSEA), 0-based here (cvq[k] is CVQ(k-9)), NSEA+10
/// elements, caller-provided (the Fortran's automatic array, heap-allocated by gfortran).
/// size_s: SIZE(S); the object copies SIZE(S) elements, not NSEA. cvq[0..9], CVQ(-9:0), are
/// never written: the Fortran leaves them undefined.
KOKKOS_INLINE_FUNCTION void w3s2xy_smcnn_int_cvq(const Real* s, long size_s, Real* cvq) {
  for (long i = 0; i < size_s; ++i) cvq[10 + i] = s[i];
}

/// One output cell (ix, iy 1-based, as the Fortran loop counters), :784-794.
/// s: S(:) 1-based in Fortran, contiguous here (the shim packs a strided section).
/// xy, nnidx, xdist, ydist: XY, NNIDX, XDIST, YDIST, all (NXO,NYO) column-major as
/// READ_SMCINT allocates them (w3smcomd.F90:597), 0-based here; nnidx holds a 1-based ISEA or
/// -1 for land. undef: UNDEF (constants.F90:85, a variable). grdx, grdy: GrdX(NSEA),
/// GrdY(NSEA) as SMCGradn wrote them, 0-based here.
KOKKOS_INLINE_FUNCTION void w3s2xy_smcnn_int_point(int ix, int iy, const Real* s, Real* xy,
                                                   bool dirn, int nxo, const int* nnidx,
                                                   const Real* xdist, const Real* ydist,
                                                   Real undef, const Real* grdx,
                                                   const Real* grdy) {
  using s2xy_smcnn_detail::k360;
  using s2xy_smcnn_detail::k630;
  // RADE folded to the same float at :792 as at :736.
  using s2xy_smcnn_detail::kRade;
  const long ixy = static_cast<long>(ix - 1) + static_cast<long>(nxo) * static_cast<long>(iy - 1);
  const int isea = nnidx[ixy];
  if (isea == -1) {
    xy[ixy] = undef;
  } else {
    // :789, left to right: (S + GrdX*XDIST) + GrdY*YDIST.
    xy[ixy] = s[isea - 1] + grdx[isea - 1] * xdist[ixy] + grdy[isea - 1] * ydist[ixy];
    if (dirn) {
      // MOD of REALs is fprem inline in the object (no libm call); fmod is exact.
      xy[ixy] = Kokkos::fmod(k630 - kRade * xy[ixy], k360);
    }
  }
}

/// W3S2XY_SMCNN_INT, :762-798. Arguments as in w3s2xy_smcnn_int_cvq and
/// w3s2xy_smcnn_int_point; nyo: NYO. cvq (NSEA+10), grdx and grdy (NSEA) are caller-provided
/// work. smcgradn(cvq, grdx, grdy, l0r1) is SMCGradn (w3psmcmd.F90:2341), not ported: the shim
/// binds the Fortran one. It receives cvq at CVQ(-9), as the object passes it (:779): its
/// explicit-shape dummy CVQ(NSEA) is sequence-associated with CVQ(-9:NSEA-10), so SMCGradn
/// reads the ten undefined CVQ(-9:0) and S shifted by ten cells (port report, WW3 defect).
template <class SmcGradn>
inline void w3s2xy_smcnn_int(const Real* s, long size_s, Real* xy, bool dirn, int nxo, int nyo,
                             const int* nnidx, const Real* xdist, const Real* ydist,
                             Real undef, Real* cvq, Real* grdx, Real* grdy,
                             SmcGradn&& smcgradn) {
  w3s2xy_smcnn_int_cvq(s, size_s, cvq);
  smcgradn(cvq, grdx, grdy, 0);
  for (int ix = 1; ix <= nxo; ++ix) {
    for (int iy = 1; iy <= nyo; ++iy) {
      w3s2xy_smcnn_int_point(ix, iy, s, xy, dirn, nxo, nnidx, xdist, ydist, undef, grdx, grdy);
    }
  }
}

}  // namespace ww::smco
