// kokkos/src/ww_kokkos/uost_uost_sourceterm_compute_se.hpp
// UOST_SOURCETERM_COMPUTE_SE: the shadow-effect part of the unresolved-obstacles source term at
// one sea point, from the shadow transparencies ALPHASH, BETASH of the upstream obstacles.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3uostmd.F90:920-1034 at NOAA-EMC/WW3 03adda91;
// author L. Mentaschi. Evidence: kokkos/port_report/UOST_SOURCETERM_COMPUTE_SE_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "uost_compute_reduction_psi.hpp"

namespace ww::uost {

/// UOST_SOURCETERM_COMPUTE_SE for one point. spec, s, d hold NSPEC values and cg NK, 0-based
/// (Fortran base 1); ix, iy are the Fortran's 1-based grid indices.
/// Module state: nspec, nk, nth, th are THIS%SGD%NSPEC, NK, NTH, TH (NTH values, 0-based);
/// nx, ny are THIS%GRD%NX, NY; the grid arrays are THIS%GRD's, contiguous, column-major,
/// Fortran base 1 in every dimension (allocated at w3uostmd.F90:189-200):
///   shd_obstructed  UOST_SHD_OBSTRUCTED(NX, NY), LOGICAL(4), nonzero is .TRUE.
///   shadowalpha, shadowbeta  UOSTSHADOWALPHA, UOSTSHADOWBETA(NX, NY, NK, NTH), INTEGER*1
///   cellsize  UOSTCELLSIZE(NX, NY, NTH), REAL*4
/// abmultfactor, cellsizefactor, shadowfactor are THIS%GRD%UOSTABMULTFACTOR,
/// UOSTCELLSIZEFACTOR, UOSTSHADOWFACTOR; gammadown is THIS%GAMMADOWN.
KOKKOS_INLINE_FUNCTION void uost_sourceterm_compute_se(
    int ix, int iy, const Real* spec, const Real* cg, Real dt, Real u10abs, Real u10dir,
    Real* s, Real* d, int nspec, int nk, int nth, const Real* th, int nx, int ny,
    const int* shd_obstructed, const std::int8_t* shadowalpha, const std::int8_t* shadowbeta,
    const Real* cellsize_arr, Real abmultfactor, Real cellsizefactor, Real shadowfactor,
    Real gammadown) {
  // :983-984. INTENT(OUT): written before the early returns.
  for (int i = 0; i < nspec; ++i) s[i] = 0.0f;
  for (int i = 0; i < nspec; ++i) d[i] = 0.0f;

  if ((ix == 1) || (ix == nx) || (iy == 1) || (iy == ny)) return;  // :991

  const std::int64_t i0 = static_cast<std::int64_t>(ix) - 1;
  const std::int64_t j0 = static_cast<std::int64_t>(iy) - 1;
  const std::int64_t nx64 = nx;
  const std::int64_t ny64 = ny;
  const std::int64_t nk64 = nk;
  if (shd_obstructed[i0 + nx64 * j0] == 0) return;  // :993-994

  for (int ik = 1; ik <= nk; ++ik) {
    for (int ith = 1; ith <= nth; ++ith) {
      const std::int64_t a4 =
          i0 + nx64 * (j0 + ny64 * ((ik - 1) + nk64 * static_cast<std::int64_t>(ith - 1)));

      // :1000-1003. MIN, MAX as the object's minss/maxss: a NaN product gives 1, then 1.
      Real alphash = static_cast<Real>(shadowalpha[a4]) / abmultfactor;
      Real m = alphash * shadowfactor;
      m = (m < 1.0f) ? m : 1.0f;
      alphash = (m > 0.0f) ? m : 0.0f;

      if (alphash == 1.0f) continue;  // :1005

      // :1008
      const Real cellsize =
          cellsize_arr[i0 + nx64 * (j0 + ny64 * static_cast<std::int64_t>(ith - 1))] *
          cellsizefactor;
      const Real cgi = cg[ik - 1];
      const Real gg = cgi / cellsize;  // :1012

      // :1014-1020. BETASH (:1002-1003) is read only on this branch, as in the object.
      Real gammma;
      if (alphash > 0.2f) {
        Real betash = static_cast<Real>(shadowbeta[a4]) / abmultfactor;
        m = betash * shadowfactor;
        m = (m < 1.0f) ? m : 1.0f;
        betash = (m > 0.0f) ? m : 0.0f;
        gammma = betash / alphash - 1.0f;
      } else {
        gammma = gammadown;
      }

      // :1028-1029, read before the PSI call: only then does g++ keep SPECI as the first
      // operand of :1030's product, as the object does (a NaN payload when both are NaN).
      const int isp = ith + (ik - 1) * nth;
      const Real speci = spec[isp - 1];

      Real psi;
      compute_reduction_psi(u10abs, u10dir, cg[ik - 1], th[ith - 1], dt, psi);  // :1024

      // :1026. The object multiplies GAMMMA * GG, g++ GG * GAMMMA whatever the source order;
      // GAMMMA is never NaN (BETASH/ALPHASH with ALPHASH in (0.2, 1), or GAMMADOWN = 20,
      // w3uostmd.F90:78, assigned nowhere), so no NaN payload can depend on it.
      const Real sfc = -(gammma * gg);

      // :1030-1031. Operand order as the object's mulss: SPECI first, then SFC first.
      s[isp - 1] = speci * sfc * psi;
      d[isp - 1] = sfc * psi;
    }
  }
}

}  // namespace ww::uost
