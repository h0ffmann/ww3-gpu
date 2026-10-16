// kokkos/src/ww_kokkos/srce_flx2.hpp
// W3FLX2: friction velocity, roughness length and drag coefficient after Tolman and
// Chalikov (1996), iterated on the peak phase velocity, one point. Phase 1,
// translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3flx2md.F90:90-230 at NOAA-EMC/WW3 03adda91;
// author H. L. Tolman. Evidence: kokkos/port_report/W3FLX2_KOKKOS.md.
// ⚠ On a device powf, logf, expf and sqrtf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"

namespace ww::srce::flx {

namespace flx2_detail {
inline constexpr Real kGrav = static_cast<Real>(9.806);         // GRAV, constants.F90:62
inline constexpr Real kTpi = static_cast<Real>(2.0) * ww::kPi;  // TPI, constants.F90:73
static_assert(kTpi == 0x1.921fb6p+2f);                          // 0x40c90fdb in w3flx2md.o

// maxss of w3flx2md.o, d the destination: a NaN or a tie gives s.
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }

// glibc's scalar powf/logf/expf on the host, as the object calls them (no libmvec).
KOKKOS_INLINE_FUNCTION Real powf_(Real x, Real y) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::powf(x, y);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::pow(x, y);))
#else
  return Kokkos::pow(x, y);
#endif
}
KOKKOS_INLINE_FUNCTION Real logf_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::logf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::log(x);))
#else
  return Kokkos::log(x);
#endif
}
KOKKOS_INLINE_FUNCTION Real expf_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::expf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::exp(x);))
#else
  return Kokkos::exp(x);
#endif
}
}  // namespace flx2_detail

/// W3FLX2(ZWIND, DEPTH, FP, U, UDIR, UST, USTD, Z0, CD) for one point. Module state:
/// nittin, cinxsi are W3GDATMD NITTIN, CINXSI of the current grid; dsie, n1max, ewn1
/// are W3DISPMD DSIE, N1MAX and EWN1(0:NAR1D), ewn1[i] = EWN1(i), as
/// ww::disp::distab() stores them (disp_distab.hpp). ust is read and written; with
/// nittin <= 0 only ustd is written, z0 and cd keep what they held (the Fortran leaves
/// its INTENT(OUT) Z0, CD undefined).
KOKKOS_INLINE_FUNCTION void w3flx2_point(Real zwind, Real depth, Real fp, Real u, Real udir,
                                         Real& ust, Real& ustd, Real& z0, Real& cd,
                                         int nittin, Real cinxsi, Real dsie, int n1max,
                                         const Real* ewn1) {
  using namespace flx2_detail;
  // :193. MAX(FP, 0.001) is maxss with 0.001 as destination.
  const Real afp = maxss(static_cast<Real>(0.001), fp) * kTpi;
  // :195-197. One quotient SIX/DSIE feeds INT() and R1 (:199).
  const Real sqrth = Kokkos::sqrt(depth);
  const Real six = afp * sqrth;
  const Real x = six / dsie;
  const int i1 = ww::st4::fint(x);
  Real wnp;
  // :198. No lower bound: I1 < 0 (NaN, negative or huge SIX gives INT_MIN) reads
  // EWN1 outside its bounds, as the Fortran does ⚠.
  if (i1 <= n1max) {
    const Real r1 = x - static_cast<Real>(i1);  // :199
    // :200, EWN1(I1+1) up to EWN1(NAR1D).
    wnp = ((static_cast<Real>(1.) - r1) * ewn1[i1] + r1 * ewn1[i1 + 1]) / depth;
  } else {
    wnp = afp * afp / kGrav;  // :202, divss by GRAV
  }
  const Real cp = afp / wnp;  // :207

  // :211-212. MAX(0.01, U) is maxss with U as destination.
  const Real unz = maxss(u, static_cast<Real>(0.01));
  ustd = udir;

  // :214-221. UNZ**2 is unz*unz; ZWIND*GRAV and the product of CINXSI*SQRT(ALPHA)
  // with UNZ**2 are as the object orders them (commuted products, same bits).
  const Real unz2 = unz * unz;
  const Real zg = zwind * kGrav;
  for (int itt = 1; itt <= nittin; ++itt) {
    // :215. MAX(UST, 0.0001): UST is the destination. **1.5 is powf.
    const Real alpha =
        static_cast<Real>(0.57) /
        powf_(cp / maxss(ust, static_cast<Real>(0.0001)), static_cast<Real>(1.5));
    // :216-217. MAX(0., LOG(..)): the LOG is the destination.
    const Real rdch = maxss(logf_(zg / ((cinxsi * Kokkos::sqrt(alpha)) * unz2)),
                            static_cast<Real>(0.));
    // :218. **1.23 is powf.
    const Real cdl = static_cast<Real>(0.001) *
                     (static_cast<Real>(0.021) +
                      static_cast<Real>(10.4) /
                          (powf_(rdch, static_cast<Real>(1.23)) + static_cast<Real>(1.85)));
    cd = cdl;
    // :219-220. One SQRT(CD) for both; -0.4/SQRT(CD) is 0.4/sqrt then a sign flip.
    const Real sqcd = Kokkos::sqrt(cdl);
    ust = sqcd * unz;
    z0 = zwind * expf_(-(static_cast<Real>(0.4) / sqcd));
  }
}

}  // namespace ww::srce::flx
