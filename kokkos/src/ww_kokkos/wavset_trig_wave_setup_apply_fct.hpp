// kokkos/src/ww_kokkos/wavset_trig_wave_setup_apply_fct.hpp
// TRIG_WAVE_SETUP_APPLY_FCT: the CSR matrix-vector product TheOut = ASPAR * TheIn of the
// wave setup solver on a triangular (PDLIB) grid, rows with ACTIVESEC = 1 only, then the
// halo exchange. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3wavset.F90:1020-1107 at NOAA-EMC/WW3 03adda91;
// authors M. Dutour-Sikiric, A. Roland. Evidence:
// kokkos/port_report/TRIG_WAVE_SETUP_APPLY_FCT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::wavset {

/// TRIG_WAVE_SETUP_APPLY_FCT, arrays 0-based. aspar (PDLIB_NNZ), thein, theout, active,
/// activesec (NPA): Fortran base 1. pdlib_ia (NPA+1) and pdlib_ja (PDLIB_NNZ) are
/// yowNodepool's PDLIB_IA, PDLIB_JA: 1-based values. REAL(rkind) is float
/// (PDLIB/yowdatapool.F90:48). ACTIVE is a dummy the Fortran never reads.
/// exchange(theout) stands for PDLIB_exchange1Dreal (:1106), an MPI halo exchange that is not
/// translated: the caller passes it, or a no-op on a single rank.
template <class Exchange>
KOKKOS_INLINE_FUNCTION void trig_wave_setup_apply_fct(const Real* aspar, const Real* thein,
                                                      Real* theout, const int* active,
                                                      const int* activesec, int npa,
                                                      const int* pdlib_ia, const int* pdlib_ja,
                                                      Exchange&& exchange) {
  // :1096; the object zeroes (memset) only when NPA > 0, the same set of stores.
  for (int ip = 0; ip < npa; ++ip) theout[ip] = static_cast<Real>(0);
  // :1097-1105; the sum per IP runs in J order, scalar in the object.
  for (int ip = 0; ip < npa; ++ip) {
    if (activesec[ip] == 1) {
      for (int j = pdlib_ia[ip]; j <= pdlib_ia[ip + 1] - 1; ++j) {
        const int jp = pdlib_ja[j - 1];
        const Real ecoeff = aspar[j - 1];
        theout[ip] = theout[ip] + ecoeff * thein[jp - 1];
      }
    }
  }
  exchange(theout);
}

}  // namespace ww::wavset
