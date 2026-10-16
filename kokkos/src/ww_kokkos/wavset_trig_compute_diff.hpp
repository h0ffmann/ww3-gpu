// kokkos/src/ww_kokkos/wavset_trig_compute_diff.hpp
// TRIG_COMPUTE_DIFF: gradient of the P1 basis function of vertex I1 of triangle IE of an
// unstructured (PDLIB) grid, used to assemble the wave setup system. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3wavset.F90:598-685 at NOAA-EMC/WW3 03adda91;
// authors M. Dutour-Sikiric, A. Roland.
// Evidence: kokkos/port_report/TRIG_COMPUTE_DIFF_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::wavset {

/// ie, i1: the Fortran values, 1-based (element number, vertex 1..3). REAL(rkind) is float
/// (PDLIB/yowdatapool.F90:48). Arrays 0-based here:
///   ine        yowElementpool INE(3,NE), column-major, node numbers 1-based (yowelementpool.F90:56);
///   x, y       yowNodepool x(:), y(:), Fortran base 1 (yownodepool.F90:79);
///   pdlib_tria yowNodepool PDLIB_TRIA(:), Fortran base 1 (yownodepool.F90:80).
/// IP1 = INE(I1,IE) is read and never used (:679); the object drops the load, so does this.
KOKKOS_INLINE_FUNCTION void trig_compute_diff(int ie, int i1, float& ugrad, float& vgrad,
                                              const int* ine, const float* x, const float* y,
                                              const float* pdlib_tria) {
  // POS_TRICK(3,2), :671-676: the two other vertices of I1, counter-clockwise.
  constexpr int pos_trick[6] = {2, 3, 1, 3, 1, 2};
  const int i2 = pos_trick[i1 - 1];
  const int i3 = pos_trick[i1 + 2];
  const int ip2 = ine[(i2 - 1) + 3 * (ie - 1)];
  const int ip3 = ine[(i3 - 1) + 3 * (ie - 1)];
  // :682, 2.0*PDLIB_TRIA(IE); the object adds it to itself (addss), the same bits.
  const float h = 2.0f * pdlib_tria[ie - 1];
  // :683, the minus applies to the quotient (xorps after divss).
  ugrad = -((y[ip3 - 1] - y[ip2 - 1]) / h);
  vgrad = (x[ip3 - 1] - x[ip2 - 1]) / h;
}

}  // namespace ww::wavset
