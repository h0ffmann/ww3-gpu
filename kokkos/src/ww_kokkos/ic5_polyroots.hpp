// kokkos/src/ww_kokkos/ic5_polyroots.hpp
// POLYROOTS of W3SIC5MD: roots of a REAL polynomial as the eigenvalues of its balanced companion
// (Hessenberg) matrix (Numerical Recipes' zrhqr).
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic5md.F90:1435-1586 at NOAA-EMC/WW3 03adda91;
// author Q. Liu (module authors Q. Liu, E. Rogers).
// Evidence: kokkos/port_report/POLYROOTS_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "ic5_balancing_matrix.hpp"

namespace ww::ic5 {

/// POLYROOTS(NPC, PCVEC, RTRL, RTIM), :1435-1586. `pcvec` is PCVEC(NPC) = [c_n, ..., c_0];
/// `rtrl`, `rtim` are RTRL(NPC-1), RTIM(NPC-1); all Fortran base 1, here 0-based.
/// `hess` is the caller-provided work array for the automatic HESS(NPC-1, NPC-1), column-major
/// with leading dimension NPC-1. EIG_HQR is not translated: `eig_hqr(nmat, hmat, eigr, eigi)`
/// stands for EIG_HQR(NMAT, HMAT, EIGR, EIGI) of :1111.
/// Returns 0, or 2 where the Fortran writes format 1001 on NAPERR and calls EXTCDE(2)
/// (:1563-1566); the shim does the WRITE and the stop. HESS is not touched on that path.
template <class EigHqr>
KOKKOS_INLINE_FUNCTION int polyroots(int npc, const float* pcvec, float* rtrl, float* rtim,
                                     float* hess, EigHqr&& eig_hqr) {
  // :1563, ERRTOL = 1.E-12 is a default REAL PARAMETER (:144).
  constexpr float errtol = 1.e-12f;
  if (Kokkos::fabs(pcvec[0]) < errtol) return 2;
  const int n = npc - 1;
  const long ld = n > 0 ? n : 0;
  // :1569
  for (long k = 0; k < ld * ld; ++k) hess[k] = 0.0f;
  // :1570, -1 * PCVEC(2:) / PCVEC(1): the object divides, then flips the sign (xorps).
  for (int j = 0; j < n; ++j) hess[j * ld] = -(pcvec[j + 1] / pcvec[0]);
  // :1571-1573
  for (int j = 1; j <= npc - 2; ++j) hess[j + (j - 1) * ld] = 1.0f;
  // :1576, :1578
  balancing_matrix(n, hess);
  eig_hqr(n, hess, rtrl, rtim);
  return 0;
}

}  // namespace ww::ic5
