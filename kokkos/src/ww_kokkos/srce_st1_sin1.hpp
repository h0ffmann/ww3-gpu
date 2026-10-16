// kokkos/src/ww_kokkos/srce_st1_sin1.hpp
// W3SIN1: WAM cycle 3 wind input (Snyder et al. 1981, Komen et al. 1984), diagonal D and
// S = D*A, one spectral point. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src1md.F90:255-417 at NOAA-EMC/WW3 03adda91;
// author H. L. Tolman. Evidence: kokkos/port_report/W3SIN1_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"

namespace ww::srce::st1 {

namespace sin1_detail {
/// maxss of w3src1md.o, d the destination: a NaN or a tie gives s.
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }
}  // namespace sin1_detail

/// W3SIN1 for one point; a, k, s, d and the W3GDATMD tables sig2, ecos, esin start at
/// element 1, 0-based here; NSPEC are read (ecos, esin hold NSPEC+NTH, w3gdatmd.F90:2061-2062).
/// sinc1 is W3GDATMD SINC1.
KOKKOS_INLINE_FUNCTION void w3sin1_point(const Real* a, const Real* k, Real ustar, Real usdir,
                                         Real* s, Real* d, const Params& p, Real sinc1,
                                         const Real* sig2, const Real* ecos, const Real* esin) {
  const int nspec = p.nspec;

  // :378-379. The object computes both with one sincosf call; glibc's sincosf returns the
  // bits of cosf and sinf.
  const Real cosu = Kokkos::cos(usdir);
  const Real sinu = Kokkos::sin(usdir);

  // :383-387. 0.035714 is a default REAL literal; the object adds -0.035714f in the
  // vector body and subtracts 0.035714f in the scalar tail, the same bits. MAX(0., x) is
  // maxss with x the destination in both.
  for (int is = 0; is < nspec; ++is) {
    const Real arg =
        ((ustar * (ecos[is] * cosu + esin[is] * sinu)) * k[is]) / sig2[is] -
        static_cast<Real>(0.035714);
    d[is] = (sinc1 * sig2[is]) * sin1_detail::maxss(arg, static_cast<Real>(0.));
  }

  // :389.
  for (int is = 0; is < nspec; ++is) s[is] = d[is] * a[is];
}

}  // namespace ww::srce::st1
