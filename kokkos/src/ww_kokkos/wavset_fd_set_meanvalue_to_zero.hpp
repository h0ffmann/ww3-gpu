// kokkos/src/ww_kokkos/wavset_fd_set_meanvalue_to_zero.hpp
// FD_SET_MEANVALUE_TO_ZERO: subtract the SI-weighted mean from an NX field, so the wave setup
// on finite-difference grids has zero area-weighted mean. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3wavset.F90:2863-2943 at NOAA-EMC/WW3 03adda91;
// authors M. Dutour-Sikiric, A. Roland.
// Evidence: kokkos/port_report/FD_SET_MEANVALUE_TO_ZERO_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::wavset {

/// thevar: Fortran TheVar(NX), REAL(rkind) with rkind = 4 (yowdatapool.F90:48), 1-based there,
/// 0-based here. nx, si: W3GDATMD NX and SI(:), REAL(8), 1-based there, 0-based here.
/// Both sums are REAL(4) accumulators updated in REAL(8) and rounded back each step
/// (:2936-2937), in IP order; the object keeps that loop scalar. NX <= 0 leaves thevar
/// untouched (the 0/0 mean is never used).
KOKKOS_INLINE_FUNCTION void fd_set_meanvalue_to_zero(float* thevar, int nx, const double* si) {
  float sum_si_var = 0.0f;
  float sum_si = 0.0f;
  for (int ip = 0; ip < nx; ++ip) {
    // :2936. The object multiplies with TheVar as the first operand (mulsd SI into TheVar).
    sum_si_var = static_cast<float>(static_cast<double>(sum_si_var) +
                                    static_cast<double>(thevar[ip]) * si[ip]);
    // :2937.
    sum_si = static_cast<float>(static_cast<double>(sum_si) + si[ip]);
  }
  // :2939.
  const float themean = sum_si_var / sum_si;
  // :2940-2942.
  for (int ip = 0; ip < nx; ++ip) {
    thevar[ip] = thevar[ip] - themean;
  }
}

}  // namespace ww::wavset
