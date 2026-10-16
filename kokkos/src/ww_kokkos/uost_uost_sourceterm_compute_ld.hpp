// kokkos/src/ww_kokkos/uost_uost_sourceterm_compute_ld.hpp
// UOST_SOURCETERM_COMPUTE_LD: the local-dissipation part of the unresolved-obstacles source
// term at one sea point, from the transparencies ALPHA, BETA of the cell's subgrid obstacles.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3uostmd.F90:793-898 at NOAA-EMC/WW3 03adda91;
// author L. Mentaschi. Evidence: kokkos/port_report/UOST_SOURCETERM_COMPUTE_LD_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "uost_compute_reduction_psi.hpp"

namespace ww::uost {

/// UOST_SOURCETERM_COMPUTE_LD for one point. spec, s, d hold NSPEC values and cg NK, 0-based
/// (Fortran base 1); ix, iy are the Fortran's 1-based grid indices.
/// Module state: nspec, nk, nth, th are THIS%SGD%NSPEC, NK, NTH, TH (NTH values, 0-based);
/// nx, ny are THIS%GRD%NX, NY; the grid arrays are THIS%GRD's, contiguous, column-major,
/// Fortran base 1 in every dimension (allocated at w3uostmd.F90:187-195):
///   lcl_obstructed  UOST_LCL_OBSTRUCTED(NX, NY), LOGICAL(4), nonzero is .TRUE.
///   localalpha, localbeta  UOSTLOCALALPHA, UOSTLOCALBETA(NX, NY, NK, NTH), INTEGER*1
///   cellsize  UOSTCELLSIZE(NX, NY, NTH), REAL*4
/// abmultfactor, cellsizefactor, localfactor are THIS%GRD%UOSTABMULTFACTOR,
/// UOSTCELLSIZEFACTOR, UOSTLOCALFACTOR; gammaup is THIS%GAMMAUP.
KOKKOS_INLINE_FUNCTION void uost_sourceterm_compute_ld(
    int ix, int iy, const Real* spec, const Real* cg, Real dt, Real u10abs, Real u10dir,
    Real* s, Real* d, int nspec, int nk, int nth, const Real* th, int nx, int ny,
    const int* lcl_obstructed, const std::int8_t* localalpha, const std::int8_t* localbeta,
    const Real* cellsize_arr, Real abmultfactor, Real cellsizefactor, Real localfactor,
    Real gammaup) {
  // :853-854. INTENT(OUT): written before the early return.
  for (int i = 0; i < nspec; ++i) s[i] = 0.0f;
  for (int i = 0; i < nspec; ++i) d[i] = 0.0f;

  const std::int64_t i0 = static_cast<std::int64_t>(ix) - 1;
  const std::int64_t j0 = static_cast<std::int64_t>(iy) - 1;
  const std::int64_t nx64 = nx;
  const std::int64_t ny64 = ny;
  const std::int64_t nk64 = nk;
  if (lcl_obstructed[i0 + nx64 * j0] == 0) return;  // :856-857

  for (int ik = 1; ik <= nk; ++ik) {
    const Real cgi = cg[ik - 1];
    for (int ith = 1; ith <= nth; ++ith) {
      const std::int64_t a4 =
          i0 + nx64 * (j0 + ny64 * ((ik - 1) + nk64 * static_cast<std::int64_t>(ith - 1)));

      // :867-870. MIN, MAX as the object's minss/maxss: a NaN product gives 1, then 1.
      Real alpha = static_cast<Real>(localalpha[a4]) / abmultfactor;
      Real m = alpha * localfactor;
      m = (m < 1.0f) ? m : 1.0f;
      alpha = (m > 0.0f) ? m : 0.0f;
      Real beta = static_cast<Real>(localbeta[a4]) / abmultfactor;
      m = beta * localfactor;
      m = (m < 1.0f) ? m : 1.0f;
      beta = (m > 0.0f) ? m : 0.0f;

      if (alpha == 1.0f) continue;  // :873

      // :876. g++ emits UOSTCELLSIZEFACTOR as the first operand of this product, gfortran the
      // array element; UOSTCELLSIZEFACTOR is never assigned (w3gdatmd.F90:787), so no NaN
      // payload can depend on the order.
      const Real cellsize =
          cellsize_arr[i0 + nx64 * (j0 + ny64 * static_cast<std::int64_t>(ith - 1))] *
          cellsizefactor;

      const int isp = ith + (ik - 1) * nth;
      const Real speci = spec[isp - 1];

      Real psi;
      compute_reduction_psi(u10abs, u10dir, cg[ik - 1], th[ith - 1], dt, psi);  // :882

      // :884-891. The object computes CGI/CELLSIZE once and multiplies (1-BETA), then GAMMAUP,
      // into it in this operand order.
      const Real q = cgi / cellsize;
      Real sfc;
      if (beta > 0.09f) {
        sfc = -((1.0f - beta) * q / beta);
      } else {
        sfc = -(q * gammaup);
      }

      // :893-894. Operand order as the object's mulss: SPECI first, then SFC first.
      s[isp - 1] = speci * sfc * psi;
      d[isp - 1] = sfc * psi;
    }
  }
}

}  // namespace ww::uost
