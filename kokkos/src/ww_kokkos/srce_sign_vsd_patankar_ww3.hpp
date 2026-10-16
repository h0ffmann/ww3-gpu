// kokkos/src/ww_kokkos/srce_sign_vsd_patankar_ww3.hpp
// SIGN_VSD_PATANKAR_WW3: clip the implicit source terms Patankar style, VD <= 0 and VS >= 0,
// for one spectrum (PDLIB implicit scheme). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3srcemd.F90:2647-2722 at NOAA-EMC/WW3 03adda91;
// authors A. Roland, M. Dutour-Sikiric. Evidence: kokkos/port_report/SIGN_VSD_PATANKAR_WW3_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::srce {

/// SIGN_VSD_PATANKAR_WW3, arrays 0-based (Fortran 1-based, length NSPEC); spec is unused, as
/// in the Fortran. nspec is W3GDATMD's NSPEC.
///
/// Trap: gfortran -O3 lowers MIN(0.,VD) and MAX(0.,VS) (:2719-2720) to minss/maxss, whose result
/// for a NaN or a signed zero depends on the operand order, and the order differs between the
/// object's paths. Each ternary below is one instruction of the build_lab object, written so a NaN
/// or a -0 gives the same bits:
///   NSPEC == 1 or 2147483647: the scalar loop, 0 is the destination of minss and of maxss;
///   otherwise elements 0..NSPEC-2 and, when NSPEC is even, the last one go through minps/maxps
///   with the value as destination; when NSPEC is odd the last one goes through the scalar tail,
///   minss with the value as destination, maxss with 0 as destination.
KOKKOS_INLINE_FUNCTION void sign_vsd_patankar_ww3(const Real* spec, Real* vs, Real* vd, int nspec) {
  (void)spec;
  const Real zero = static_cast<Real>(0.);
  if (nspec <= 0) return;

  if (nspec == 1 || nspec == 2147483647) {
    for (int is = 0; is < nspec; ++is) {
      vd[is] = (zero < vd[is]) ? zero : vd[is];
      vs[is] = (zero > vs[is]) ? zero : vs[is];
    }
    return;
  }

  const int nvec = (nspec % 2 == 0) ? nspec : nspec - 1;
  for (int is = 0; is < nvec; ++is) {
    vd[is] = (vd[is] < zero) ? vd[is] : zero;
    vs[is] = (vs[is] > zero) ? vs[is] : zero;
  }
  if (nvec < nspec) {
    const int is = nspec - 1;
    vd[is] = (vd[is] < zero) ? vd[is] : zero;
    vs[is] = (zero > vs[is]) ? zero : vs[is];
  }
}

}  // namespace ww::srce
