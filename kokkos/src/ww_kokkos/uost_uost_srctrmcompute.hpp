// kokkos/src/ww_kokkos/uost_uost_srctrmcompute.hpp
// UOST_SRCTRMCOMPUTE: the unresolved-obstacles source term of one sea point, the public entry
// W3SRCE calls; it forwards to the COMPUTE method of the module object SRCTRM.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3uostmd.F90:298-343 at NOAA-EMC/WW3 03adda91;
// author L. Mentaschi. Evidence: kokkos/port_report/UOST_SRCTRMCOMPUTE_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::uost {

/// UOST_SRCTRMCOMPUTE for one point. spec, s, d hold NSPEC values and cg NK, 0-based (Fortran
/// base 1); ix, iy are passed through unchanged as the Fortran's 1-based grid indices.
/// `compute` stands for SRCTRM%COMPUTE (:342), called as compute(ix, iy, spec, cg, dt,
/// u10abs, u10dir, s, d). SRCTRM is only ever allocated with its declared type (:274), so the
/// object's vtable call (slot +0x38) always lands in UOST_SOURCETERM_COMPUTE (:1056-1128),
/// which is not translated here: the caller passes it.
template <class Compute>
KOKKOS_INLINE_FUNCTION void uost_srctrmcompute(int ix, int iy, const Real* spec, const Real* cg,
                                               Real dt, Real u10abs, Real u10dir, Real* s,
                                               Real* d, Compute&& compute) {
  compute(ix, iy, spec, cg, dt, u10abs, u10dir, s, d);
}

}  // namespace ww::uost
