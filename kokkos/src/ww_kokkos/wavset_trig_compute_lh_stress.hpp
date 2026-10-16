// kokkos/src/ww_kokkos/wavset_trig_compute_lh_stress.hpp
// TRIG_COMPUTE_LH_STRESS: the wave forcing of the setup, minus the divergence of the
// radiation stress (SXX, SXY, SYY over DWAT*GRAV) on the unstructured (PDLIB) grid, and
// the depth at each node. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3wavset.F90:448-584 at NOAA-EMC/WW3 03adda91;
// authors Aron Roland, Mathieu Dutour-Sikiric.
// Evidence: kokkos/port_report/TRIG_COMPUTE_LH_STRESS_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <utility>

#include "wavset_differentiate_xydir.hpp"

namespace ww::wavset {

/// TRIG_COMPUTE_LH_STRESS (:448-584). REAL(rkind) is float (PDLIB/yowdatapool.F90:48).
/// Arrays 0-based here:
///   f_x, f_y, dwnx    F_X(npa), F_Y(npa), DWNX(npa), Fortran base 1;
///   iplg              yowNodepool iplg(npa), values 1-based;
///   mapfs             W3GDATMD MAPFS(NY,NX) from MAPFS(1,1), column-major, leading dimension
///                     mapfs_ld; only row 1 is read, MAPFS(1,IX) = mapfs[(IX-1)*mapfs_ld];
///   sxx, sxy, syy     W3ADATMD SXX(NSEALM), SXY, SYY, Fortran base 1;
///   dw                W3ADATMD DW(0:NSEA), Fortran base 0, so DW(ISEA) = dw[isea];
///   sxx_p, sxy_p, syy_p, u_x1, u_y1, u_x2, u_y2
///                     caller-provided work arrays of npa floats, the automatic arrays
///                     of :526-528.
/// DIFFERENTIATE_XYDIR_MAPSTA, with its module state, WEI and its MPI exchange, is the
/// callable passed on to differentiate_xydir (wavset_differentiate_xydir.hpp).
/// SXXmethod is 1 (:530), so eSXX/eSXY/eSYY are always set. W3_DEBUGSTP (:553-582) only
/// writes and is not translated.
template <class DifferentiateXydirMapsta>
inline void trig_compute_lh_stress(float* f_x, float* f_y, float* dwnx, int npa, int nseal,
                                   const int* iplg, const int* mapfs, int mapfs_ld,
                                   const float* sxx, const float* sxy, const float* syy,
                                   const float* dw, float* sxx_p, float* sxy_p, float* syy_p,
                                   float* u_x1, float* u_y1, float* u_x2, float* u_y2,
                                   DifferentiateXydirMapsta&& differentiate_xydir_mapsta) {
  // DWAT*GRAV (constants.F90:62-63) folded by gfortran to 9806.0 (GIMPLE `/ 9.806e+3`).
  constexpr float dwat_grav = 0x1.327p+13f;
  static_assert(dwat_grav == 1000.0f * 9.806f, "DWAT*GRAV in REAL");
  for (int ip = 0; ip < npa; ++ip) sxx_p[ip] = 0.0f;
  for (int ip = 0; ip < npa; ++ip) sxy_p[ip] = 0.0f;
  for (int ip = 0; ip < npa; ++ip) syy_p[ip] = 0.0f;
  for (int ip = 0; ip < npa; ++ip) dwnx[ip] = 0.0f;
  for (int jsea = 1; jsea <= nseal; ++jsea) {
    const int ip = jsea;
    const int ix = iplg[ip - 1];
    const int isea = mapfs[static_cast<std::ptrdiff_t>(ix - 1) * mapfs_ld];
    const float esxx = sxx[jsea - 1] / dwat_grav;
    const float esxy = sxy[jsea - 1] / dwat_grav;
    const float esyy = syy[jsea - 1] / dwat_grav;
    // DBLE(e) assigned to REAL(rkind) (:547-549): the round trip is exact and the object drops it.
    sxx_p[ip - 1] = esxx;
    sxy_p[ip - 1] = esxy;
    syy_p[ip - 1] = esyy;
    dwnx[ip - 1] = dw[isea];
  }
  differentiate_xydir(sxx_p, u_x1, u_y1, differentiate_xydir_mapsta);
  differentiate_xydir(sxy_p, u_x2, u_y2, differentiate_xydir_mapsta);
  for (int ip = 0; ip < npa; ++ip) f_x[ip] = -u_x1[ip] - u_y2[ip];
  differentiate_xydir(syy_p, u_x1, u_y1, differentiate_xydir_mapsta);
  for (int ip = 0; ip < npa; ++ip) f_y[ip] = -u_y1[ip] - u_x2[ip];
}

}  // namespace ww::wavset
