// kokkos/src/ww_kokkos/wavset_fd_wave_setup_compute_system.hpp
// FD_WAVE_SETUP_COMPUTE_SYSTEM: matrix and right-hand side of the wave setup Poisson problem
// on finite-difference grids, assembled edge by edge. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3wavset.F90:2539-2642 at NOAA-EMC/WW3 03adda91;
// authors M. Dutour-Sikiric, A. Roland.
// Evidence: kokkos/port_report/FD_WAVE_SETUP_COMPUTE_SYSTEM_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "wavset_fd_compute_diff.hpp"

namespace ww::wavset {

/// FD_WAVE_SETUP_COMPUTE_SYSTEM. aspar: ASPAR(PDLIB_NNZ), b: B(NX), fx, fy: FX(NSEA), FY(NSEA),
/// all Fortran base 1, 0-based here. Module state: pdlib_nnz (yowNodepool), nx, nbedge and the
/// arrays fd_compute_diff reads (EDGES, MAPSF, XGRD, YGRD, as there). W3ADATMD DW is not an
/// argument: it feeds only eDep (:2625), which nothing reads, and the object never loads it.
KOKKOS_INLINE_FUNCTION void fd_wave_setup_compute_system(
    Real* aspar, Real* b, const Real* fx, const Real* fy, int pdlib_nnz, int nx, int nbedge,
    const int* edges, std::int64_t ld_edges, const int* mapsf, std::int64_t ld_mapsf,
    const double* xgrd, const double* ygrd, std::int64_t ld_grd) {
  // :2638 reads eFact, which is never set (WW3 defect). gfortran -O3 gives the undefined value
  // the constant +0.0 (.LC0 = .LC1+4 = 0x00000000 in the object), so ASPAR gets escal*0.0f.
  constexpr Real efact = 0.0f;
  for (int i = 0; i < pdlib_nnz; ++i) aspar[i] = 0.0f;
  for (int i = 0; i < nx; ++i) b[i] = 0.0f;
  for (int iedge = 1; iedge <= nbedge; ++iedge) {
    const std::int64_t ie = static_cast<std::int64_t>(iedge) - 1;
    const int isea = edges[ie];
    const int jsea = edges[ie + ld_edges];
    const Real efx = (fx[isea - 1] + fx[jsea - 1]) / 2.0f;
    const Real efy = (fy[isea - 1] + fy[jsea - 1]) / 2.0f;
    for (int i = 1; i <= 2; ++i) {
      const int isearel = edges[ie + (i - 1) * ld_edges];
      Real ugrad1, vgrad1, dist1;
      fd_compute_diff(iedge, isearel, ugrad1, vgrad1, dist1, edges, ld_edges, mapsf, ld_mapsf,
                      xgrd, ygrd, ld_grd);
      Real escal = ugrad1 * efx + vgrad1 * efy;
      // :2632 indexes B(NX) with a sea index, as written.
      b[isearel - 1] = b[isearel - 1] + escal * dist1;
      for (int j = 1; j <= 2; ++j) {
        const int jsearel = edges[ie + (j - 1) * ld_edges];
        Real ugrad, vgrad, dist2;
        fd_compute_diff(iedge, jsearel, ugrad, vgrad, dist2, edges, ld_edges, mapsf, ld_mapsf,
                        xgrd, ygrd, ld_grd);
        escal = ugrad * ugrad1 + vgrad * vgrad1;
        // :2638 accumulates into ASPAR(J), J = 1 or 2, not into a sparse position (WW3 defect).
        aspar[j - 1] = aspar[j - 1] + efact * escal;
      }
    }
  }
}

}  // namespace ww::wavset
