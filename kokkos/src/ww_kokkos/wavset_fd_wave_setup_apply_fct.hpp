// kokkos/src/ww_kokkos/wavset_fd_wave_setup_apply_fct.hpp
// FD_WAVE_SETUP_APPLY_FCT: the CSR matrix-vector product TheOut = ASPAR * TheIn of the
// wave setup solver on a finite-difference (PDLIB) grid. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3wavset.F90:1945-2026 at NOAA-EMC/WW3 03adda91;
// authors M. Dutour-Sikiric, A. Roland. Evidence:
// kokkos/port_report/FD_WAVE_SETUP_APPLY_FCT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::wavset {

/// FD_WAVE_SETUP_APPLY_FCT, arrays 0-based. aspar (NNZ), thein, theout (NSEA): Fortran base 1.
/// pdlib_ia (NSEA+1) and pdlib_ja (NNZ) are yowNodepool's PDLIB_IA, PDLIB_JA: 1-based values.
/// REAL(rkind) is float (PDLIB/yowdatapool.F90:48).
KOKKOS_INLINE_FUNCTION void fd_wave_setup_apply_fct(const Real* aspar, const Real* thein,
                                                    Real* theout, int nsea,
                                                    const int* pdlib_ia, const int* pdlib_ja) {
  // :2017; the object zeroes only when NSEA > 0, which is the same set of stores.
  for (int ip = 0; ip < nsea; ++ip) theout[ip] = static_cast<Real>(0);
  // :2018-2024; the sum per IP runs in J order, scalar in the object.
  for (int ip = 0; ip < nsea; ++ip) {
    for (int j = pdlib_ia[ip]; j <= pdlib_ia[ip + 1] - 1; ++j) {
      const int jp = pdlib_ja[j - 1];
      const Real ecoeff = aspar[j - 1];
      theout[ip] = theout[ip] + ecoeff * thein[jp - 1];
    }
  }
}

}  // namespace ww::wavset
