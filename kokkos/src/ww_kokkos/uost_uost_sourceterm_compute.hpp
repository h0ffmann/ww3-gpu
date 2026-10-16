// kokkos/src/ww_kokkos/uost_uost_sourceterm_compute.hpp
// UOST_SOURCETERM_COMPUTE: the unresolved-obstacles source term of one sea point, the sum of
// the local dissipation and the shadow effect.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3uostmd.F90:1056-1128 at NOAA-EMC/WW3 03adda91;
// author L. Mentaschi. Evidence: kokkos/port_report/UOST_SOURCETERM_COMPUTE_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

// g++ swaps the operands of these adds in its vector body (S_SE + S_LD), gfortran does not;
// the bits differ when both are NaN (x86 returns the first operand's NaN). The scalar loop keeps
// the Fortran's order.
#if defined(__GNUC__) && !defined(__clang__) && !defined(__CUDA_ARCH__) && __GNUC__ >= 14
#define WW_UOST_NOVECTOR _Pragma("GCC novector")
#else
#define WW_UOST_NOVECTOR
#endif

namespace ww::uost {

/// UOST_SOURCETERM_COMPUTE for one point. spec, s, d, s_ld, s_se, d_ld, d_se hold NSPEC values
/// and cg NK, 0-based (Fortran base 1); ix, iy are passed through unchanged as the Fortran's
/// 1-based grid indices.
/// nspec is THIS%SGD%NSPEC, uostenabled THIS%GRD%UOSTENABLED.
/// s_ld, s_se, d_ld, d_se are the Fortran's automatic arrays (:1105-1106), which the object
/// mallocs on every call; here the caller provides them.
/// compute_ld and compute_se stand for THIS%COMPUTE_LD (:1123) and THIS%COMPUTE_SE (:1125),
/// called as f(ix, iy, spec, cg, dt, u10abs, u10dir, s, d). The object calls them through the
/// vtable (slots +0x50, +0x48); SRCTRM is only allocated with its declared type
/// (w3uostmd.F90:274), so they are always UOST_SOURCETERM_COMPUTE_LD (:793-898) and
/// UOST_SOURCETERM_COMPUTE_SE (:920-1034), not translated yet: the caller passes them.
template <class ComputeLd, class ComputeSe>
KOKKOS_INLINE_FUNCTION void uost_sourceterm_compute(int ix, int iy, const Real* spec,
                                                    const Real* cg, Real dt, Real u10abs,
                                                    Real u10dir, Real* s, Real* d, int nspec,
                                                    bool uostenabled, Real* s_ld, Real* s_se,
                                                    Real* d_ld, Real* d_se,
                                                    ComputeLd&& compute_ld,
                                                    ComputeSe&& compute_se) {
  // D is INTENT(OUT) and left unwritten on this path (:1114-1117).
  if (!uostenabled) {
    for (int i = 0; i < nspec; ++i) s[i] = 0.0f;
    return;
  }

  for (int i = 0; i < nspec; ++i) s_ld[i] = 0.0f;
  for (int i = 0; i < nspec; ++i) s_se[i] = 0.0f;
  compute_ld(ix, iy, spec, cg, dt, u10abs, u10dir, s_ld, d_ld);
  compute_se(ix, iy, spec, cg, dt, u10abs, u10dir, s_se, d_se);
  WW_UOST_NOVECTOR
  for (int i = 0; i < nspec; ++i) s[i] = s_ld[i] + s_se[i];
  WW_UOST_NOVECTOR
  for (int i = 0; i < nspec; ++i) d[i] = d_ld[i] + d_se[i];
}

}  // namespace ww::uost
