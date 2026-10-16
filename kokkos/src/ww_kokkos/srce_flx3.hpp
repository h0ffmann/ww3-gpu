// kokkos/src/ww_kokkos/srce_flx3.hpp
// W3FLX3: friction velocity, roughness length and drag coefficient after Tolman and
// Chalikov (1996), with a cap on the drag coefficient, one point. Phase 1, translated,
// not regtested.
// WW3 heritage: WW3-noaa/model/src/w3flx3md.F90:95-248 at NOAA-EMC/WW3 03adda91;
// author H. L. Tolman. Evidence: kokkos/port_report/W3FLX3_KOKKOS.md.
// ⚠ On a device sqrtf, powf, logf, expf and tanhf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"

namespace ww::srce::flx {

namespace flx3_detail {
// minss/maxss of w3flx3md.o, d the destination: a NaN or a tie gives s.
KOKKOS_INLINE_FUNCTION Real minss(Real d, Real s) { return (d < s) ? d : s; }
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }

inline constexpr Real kGrav = static_cast<Real>(9.806);  // GRAV constants.F90:62
// TPI = 2*PI (constants.F90:72), folded by gfortran to 0x40c90fdb.
inline constexpr Real kTpi = 0x1.921fb6p+2f;
static_assert(kTpi == static_cast<Real>(2.0) * ww::kPi);
}  // namespace flx3_detail

/// W3FLX3 for one point (w3flx3md.F90:95-248). ust is INOUT: its value on entry starts
/// the iteration. The module state follows the Fortran's USE lists: nittin, cinxsi,
/// cd_max, cap_id of W3GDATMD (0 = MIN cap, otherwise TANH cap, :232-236), then dsie,
/// n1max and ewn1 of W3DISPMD as disp_distab.hpp builds them: ewn1[i] = EWN1(i),
/// i = 0..NAR1D (lower bound 0, no shift).
/// ⚠ With nittin <= 0 the Fortran reads CD, INTENT(OUT), before writing it (:233,
/// :235); this reads the incoming cd the same way.
KOKKOS_INLINE_FUNCTION void w3flx3_point(Real zwind, Real depth, Real fp, Real u, Real udir,
                                         Real& ust, Real& ustd, Real& z0, Real& cd,
                                         int nittin, Real cinxsi, Real cd_max, int cap_id,
                                         Real dsie, int n1max, const Real* ewn1) {
  using flx3_detail::kGrav;
  using flx3_detail::maxss;
  using flx3_detail::minss;
  const Real one = static_cast<Real>(1.);

  // :200-210, the inlined WAVNU1. INT() is cvttss2si.
  const Real afp = flx3_detail::kTpi * maxss(static_cast<Real>(0.001), fp);
  const Real sqrth = Kokkos::sqrt(depth);
  const Real six = afp * sqrth;
  // ⚠ A NaN SIX (FP NaN, DEPTH < 0) or SIX/DSIE >= 2**31 gives I1 = INT_MIN <= N1MAX
  // and the Fortran reads EWN1(INT_MIN) out of bounds; reproduced, not guarded.
  const int i1 = ww::st4::fint(six / dsie);
  Real wnp;
  if (i1 <= n1max) {
    const Real r1 = six / dsie - static_cast<Real>(i1);
    wnp = ((one - r1) * ewn1[i1] + r1 * ewn1[i1 + 1]) / depth;
  } else {
    wnp = afp * afp / kGrav;
  }

  // :214-219
  const Real cp = afp / wnp;
  const Real unz = maxss(u, static_cast<Real>(0.01));
  ustd = udir;

  // :221-228. powf for **1.5 and **1.23, as the object calls them; UNZ**2 is UNZ*UNZ.
  for (int itt = 1; itt <= nittin; ++itt) {
    const Real alpha = static_cast<Real>(0.57) /
                       Kokkos::pow(cp / maxss(ust, static_cast<Real>(0.0001)),
                                   static_cast<Real>(1.5));
    const Real rdch =
        maxss(Kokkos::log((zwind * kGrav) / (cinxsi * Kokkos::sqrt(alpha) * (unz * unz))),
              static_cast<Real>(0.));
    cd = static_cast<Real>(0.001) *
         (static_cast<Real>(0.021) +
          static_cast<Real>(10.4) / (Kokkos::pow(rdch, static_cast<Real>(1.23)) +
                                     static_cast<Real>(1.85)));
    ust = Kokkos::sqrt(cd) * unz;
    z0 = zwind * Kokkos::exp(-(static_cast<Real>(0.4) / Kokkos::sqrt(cd)));
  }

  // :232-236
  if (cap_id == 0) {
    cd = minss(cd_max, cd);
  } else {
    cd = cd_max * Kokkos::tanh(cd / cd_max);
  }

  // :238-239. -(0.4/x), as the object divides then flips the sign (xorps): (-0.4)/x
  // differs in the sign of a NaN.
  ust = Kokkos::sqrt(cd) * unz;
  z0 = zwind * Kokkos::exp(-(static_cast<Real>(0.4) / Kokkos::sqrt(cd)));
}

}  // namespace ww::srce::flx
