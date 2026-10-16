// kokkos/src/ww_kokkos/wavset_trig_wave_setup_apply_precond.hpp
// TRIG_WAVE_SETUP_APPLY_PRECOND: the preconditioner of the conjugate gradient of the wave setup
// Poisson solver on a triangular (PDLIB) grid, then the halo exchange. Phase 1, translated, not
// regtested.
// WW3 heritage: WW3-noaa/model/src/w3wavset.F90:892-1005 at NOAA-EMC/WW3 03adda91;
// authors M. Dutour-Sikiric, A. Roland. Evidence:
// kokkos/port_report/TRIG_WAVE_SETUP_APPLY_PRECOND_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::wavset {

/// TRIG_WAVE_SETUP_APPLY_PRECOND, arrays 0-based. aspar (PDLIB_NNZ), thein, theout, active,
/// activesec (NPA): Fortran base 1. pdlib_ia (NPA+1), pdlib_ja (PDLIB_NNZ), pdlib_i_diag (NPA)
/// are yowNodepool's PDLIB_IA, PDLIB_JA, PDLIB_I_DIAG: 1-based values. REAL(rkind) is float
/// (PDLIB/yowdatapool.F90:48). exchange(theout) stands for PDLIB_exchange1Dreal (:1004), an MPI
/// halo exchange that is not translated: the caller passes it, or a no-op on a single rank.
template <class Exchange>
KOKKOS_INLINE_FUNCTION void trig_wave_setup_apply_precond(const Real* aspar, const Real* thein,
                                                          Real* theout, const int* active,
                                                          const int* activesec, int npa,
                                                          const int* pdlib_ia,
                                                          const int* pdlib_ja,
                                                          const int* pdlib_i_diag,
                                                          Exchange&& exchange) {
  // :967 `INTEGER :: ThePrecond = 2` is an implicitly SAVEd local that nothing assigns, so only
  // the diagonal scaling (:994-1003) runs; gfortran -O3 keeps only that branch. Branches 0 and 1
  // are translated but are dead in the Fortran object and in this one.
  constexpr int the_precond = 2;
  if constexpr (the_precond == 0) {
    for (int ip = 0; ip < npa; ++ip) theout[ip] = thein[ip];
  }
  if constexpr (the_precond == 1) {
    for (int ip = 0; ip < npa; ++ip) theout[ip] = static_cast<Real>(0);
    for (int ip = 0; ip < npa; ++ip) {
      if (active[ip] == 1) {
        const int j1 = pdlib_i_diag[ip];
        for (int j = pdlib_ia[ip]; j <= pdlib_ia[ip + 1] - 1; ++j) {
          const int jp = pdlib_ja[j - 1];
          if (activesec[jp - 1] == 1) {
            Real ecoeff;
            if (j == j1) {
              ecoeff = 1.0f / aspar[j - 1];
            } else {
              const int j2 = pdlib_i_diag[jp - 1];
              ecoeff = -aspar[j - 1] / (aspar[j1 - 1] * aspar[j2 - 1]);
            }
            theout[ip] = theout[ip] + ecoeff * thein[jp - 1];
          }
        }
      }
    }
  }
  if constexpr (the_precond == 2) {
    for (int ip = 0; ip < npa; ++ip) {
      if (activesec[ip] == 1) {
        const int j = pdlib_i_diag[ip];
        theout[ip] = thein[ip] / aspar[j - 1];
      } else {
        theout[ip] = thein[ip];
      }
    }
  }
  exchange(theout);
}

}  // namespace ww::wavset
