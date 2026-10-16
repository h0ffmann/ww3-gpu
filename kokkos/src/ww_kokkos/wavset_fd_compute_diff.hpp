// kokkos/src/ww_kokkos/wavset_fd_compute_diff.hpp
// FD_COMPUTE_DIFF: length and signed unit vector of one edge of the finite-difference
// wave-setup graph, seen from one of its two sea points. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3wavset.F90:2433-2525 at NOAA-EMC/WW3 03adda91;
// authors M. Dutour-Sikiric, A. Roland. Evidence: kokkos/port_report/FD_COMPUTE_DIFF_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::wavset {

/// FD_COMPUTE_DIFF. iedge and isea are 1-based as in the Fortran; the arrays are
/// column-major, Fortran base 1, passed 0-based with their leading dimension:
/// edges(NBEDGE,2) (ld_edges = NBEDGE), mapsf(NSEA,3) (ld_mapsf = NSEA),
/// xgrd/ygrd(NY,NX) in DOUBLE PRECISION (ld_grd = NY).
KOKKOS_INLINE_FUNCTION void fd_compute_diff(int iedge, int isea, Real& ugrad, Real& vgrad,
                                            Real& dist, const int* edges, std::int64_t ld_edges,
                                            const int* mapsf, std::int64_t ld_mapsf,
                                            const double* xgrd, const double* ygrd,
                                            std::int64_t ld_grd) {
  const std::int64_t ie = static_cast<std::int64_t>(iedge) - 1;
  // :2509-2514.
  const int isea1 = edges[ie];
  const int isea2 = edges[ie + ld_edges];
  const std::int64_t s1 = static_cast<std::int64_t>(isea1) - 1;
  const std::int64_t s2 = static_cast<std::int64_t>(isea2) - 1;
  const int ix1 = mapsf[s1];
  const int iy1 = mapsf[s1 + ld_mapsf];
  const int ix2 = mapsf[s2];
  const int iy2 = mapsf[s2 + ld_mapsf];
  // :2515-2516 index XGRD(IX,IY) although XGRD is allocated (NY,NX) (w3gdatmd.F90:1727):
  // the first subscript is IX on a leading dimension of NY, reproduced as written.
  const std::int64_t p1 = (static_cast<std::int64_t>(ix1) - 1) +
                          (static_cast<std::int64_t>(iy1) - 1) * ld_grd;
  const std::int64_t p2 = (static_cast<std::int64_t>(ix2) - 1) +
                          (static_cast<std::int64_t>(iy2) - 1) * ld_grd;
  const Real deltax = static_cast<Real>(xgrd[p1] - xgrd[p2]);
  const Real deltay = static_cast<Real>(ygrd[p1] - ygrd[p2]);
  // :2517.
  dist = Kokkos::sqrt(deltax * deltax + deltay * deltay);
  // :2518-2524. -deltaX/dist is -(deltaX/dist) in Fortran; the object negates the quotient,
  // which differs from (-deltaX)/dist only in the sign of the NaN from 0/0.
  const Real u = deltax / dist;
  const Real v = deltay / dist;
  if (isea == isea1) {
    ugrad = u;
    vgrad = v;
  } else {
    ugrad = -u;
    vgrad = -v;
  }
}

}  // namespace ww::wavset
