// kokkos/src/ww_kokkos/snl4_cgfnc.hpp
// cgfnc of W3SNL4MD: linear group velocity Cg = 0.5 c (1 + kd (1 - tanh(kd)**2) / tanh(kd))
// from frequency, depth and phase speed. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3snl4md.F90:5396-5469 at NOAA-EMC/WW3 03adda91;
// authors B. Toulany, M. Casey, W. Perrie. Evidence: kokkos/port_report/CGFNC_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cmath>

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::snl4 {

namespace cgfnc_detail {
// glibc's scalar tanhf on the host, as the object calls it (tanhf@PLT, no libmvec variant).
// ⚠ On a device Kokkos::tanh (CUDA libdevice) is not shown to return glibc's bits.
KOKKOS_INLINE_FUNCTION Real tanh_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::tanhf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::tanh(x);))
#else
  return Kokkos::tanh(x);
#endif
}
}  // namespace cgfnc_detail

/// cgfnc(f, dep, cvel), :5396-5469: f in Hz, dep in m, cvel in m/s, Cg in m/s.
/// twopi is the module variable of w3snl4md (:142), set to TPI in W3SNL4 (:872).
/// tkd = 0 (wkd = 0) divides by zero as the Fortran does; no guard.
KOKKOS_INLINE_FUNCTION Real cgfnc(Real f, Real dep, Real cvel, Real twopi) {
  // :5461. ((twopi*f)*dep)/cvel; the object multiplies f by twopi (commutative, same bits).
  const Real wkd = twopi * f * dep / cvel;
  // :5462.
  const Real tkd = cgfnc_detail::tanh_(wkd);
  // :5463. tkd**2 is tkd*tkd; the object forms (cvel*0.5) and ((1-tkd*tkd)*wkd)/tkd + 1.
  return static_cast<Real>(0.5) * cvel *
         (static_cast<Real>(1.) + wkd * (static_cast<Real>(1.) - tkd * tkd) / tkd);
}

}  // namespace ww::snl4
