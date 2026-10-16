// kokkos/src/ww_kokkos/wavset_fd_wave_setup_apply_precond.hpp
// FD_WAVE_SETUP_APPLY_PRECOND: the preconditioner of the conjugate gradient of the wave setup
// Poisson solver on a finite-difference (PDLIB) grid. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3wavset.F90:2039-2139 at NOAA-EMC/WW3 03adda91;
// authors M. Dutour-Sikiric, A. Roland. Evidence:
// kokkos/port_report/FD_WAVE_SETUP_APPLY_PRECOND_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::wavset {

/// FD_WAVE_SETUP_APPLY_PRECOND, arrays 0-based. aspar (NNZ), thein, theout (NSEA): Fortran
/// base 1. pdlib_ia (NSEA+1), pdlib_ja (NNZ), pdlib_i_diag (NSEA) are yowNodepool's PDLIB_IA,
/// PDLIB_JA, PDLIB_I_DIAG: 1-based values. REAL(rkind) is float (PDLIB/yowdatapool.F90:48).
KOKKOS_INLINE_FUNCTION void fd_wave_setup_apply_precond(const Real* aspar, const Real* thein,
                                                        Real* theout, int nsea,
                                                        const int* pdlib_ia, const int* pdlib_ja,
                                                        const int* pdlib_i_diag) {
  // :2109 `INTEGER :: ThePrecond = 0` is an implicitly SAVEd local that nothing assigns, so only
  // the identity branch ever runs; gfortran -O3 keeps only that branch (a memcpy of NSEA reals).
  // Branches 1 and 2 are translated but are dead in the Fortran object and in this one.
  constexpr int the_precond = 0;
  if constexpr (the_precond == 0) {
    for (int ip = 0; ip < nsea; ++ip) theout[ip] = thein[ip];
  }
  if constexpr (the_precond == 1) {
    for (int ip = 0; ip < nsea; ++ip) theout[ip] = static_cast<Real>(0);
    for (int ip = 0; ip < nsea; ++ip) {
      const int j1 = pdlib_i_diag[ip];
      for (int j = pdlib_ia[ip]; j <= pdlib_ia[ip + 1] - 1; ++j) {
        const int jp = pdlib_ja[j - 1];
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
  if constexpr (the_precond == 2) {
    for (int ip = 0; ip < nsea; ++ip) {
      const int j = pdlib_i_diag[ip];
      theout[ip] = thein[ip] / aspar[j - 1];
    }
  }
}

}  // namespace ww::wavset
