// kokkos/src/ww_kokkos/srce_flx5.hpp
// W3FLX5: friction velocity, drag coefficient, roughness length and Charnock coefficient from
// the atmospheric model's total stress, one sea point. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3flx5md.F90:118-260 at NOAA-EMC/WW3 03adda91;
// authors N. G. Valiente, J. Edward, A. Saulter. Evidence: kokkos/port_report/W3FLX5_KOKKOS.md.
// ⚠ On a device sqrtf and expf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::srce::flx {

namespace flx5_detail {
/// Returned where the Fortran stops in EXTCDE(1) (:228-231): the kernel cannot write the
/// message to NDSE (:229) nor stop the run, the shim must.
inline constexpr int kExtcde = 1;

/// 0.11*NU_AIR of :243, folded by gfortran (.rodata 0x35ceb1ff); NU_AIR = 1.4E-5,
/// constants.F90:65.
inline constexpr Real kNu011 = 0x1.9d63fep-20f;
static_assert(kNu011 == 0.11f * static_cast<Real>(1.4E-5), "0.11*NU_AIR, 0x35ceb1ff");
}  // namespace flx5_detail

/// W3FLX5 for one point. U10D is read by no statement of the Fortran (:226-249) and is kept for
/// the argument order. Returns 0, or kExtcde with no output written.
KOKKOS_INLINE_FUNCTION int w3flx5_point(Real zwnd, Real u10, Real u10d, Real taua, Real tauadir,
                                        Real rhoair, Real& ust, Real& ustd, Real& z0, Real& cd,
                                        Real& charn) {
  // :228. A NaN ZWND passes, as comiss/jbe in the object.
  if (Kokkos::fabs(zwnd - 10.f) > 0.01f) return flx5_detail::kExtcde;

  // MAX/MIN below follow maxss/minss of the object, register operand first: a NaN in it
  // yields the constant.
  // :236.
  const Real s = Kokkos::sqrt(taua / rhoair);
  const Real ust_ = s > 1E-4f ? s : 1E-4f;
  ust = ust_;
  // :237.
  const Real unz = u10 > 0.01f ? u10 : 0.01f;
  // :238. (UST/UNZ)**2 is one product in the object.
  const Real r = ust_ / unz;
  cd = r * r;
  // :239.
  ustd = tauadir;
  // :240.
  const Real q = unz / ust_;
  const Real sqrtcdm1 = q < 100.f ? q : 100.f;
  // :241. KAPPA = 0.40 (constants.F90:70); -KAPPA*X is -(X*KAPPA), a mulss then a sign flip.
  const Real z0_ = zwnd * Kokkos::exp(-(sqrtcdm1 * 0.4f));
  z0 = z0_;
  // :242-249. GRAV = 9.806 (constants.F90:62); UST**2 is UST*UST.
  Real c;
  if (unz > 2.5f) {
    c = ((z0_ - flx5_detail::kNu011 / ust_) * 9.806f) / (ust_ * ust_);
    c = c > 0.0095f ? c : 0.0095f;
    c = c < 0.035f ? c : 0.035f;
  } else {
    c = 0.0095f;
  }
  charn = c;
  return 0;
}

}  // namespace ww::srce::flx
