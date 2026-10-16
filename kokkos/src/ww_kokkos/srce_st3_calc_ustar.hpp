// kokkos/src/ww_kokkos/srce_st3_calc_ustar.hpp
// CALC_USTAR of W3SRC3MD (ST3, WAM4+): friction velocity, roughness length and
// Charnock coefficient by bilinear interpolation in the TAUT table. Phase 1, translated,
// not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src3md.F90:1135-1227 at NOAA-EMC/WW3 03adda91 (module
// constants :95-101); author F. Ardhuin. Evidence:
// kokkos/port_report/W3SRC3MD_CALC_USTAR_KOKKOS.md.
// Not the ST4 routine of st4_calc_ustar.hpp: ST3's has no Newton branch and no CAPCHNK.
// ⚠ On a device expf and sqrtf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"

namespace ww::srce::st3 {

namespace calc_ustar_detail {
inline constexpr int kItauMax = 200;                        // ITAUMAX :95
inline constexpr int kJuMax = 200;                          // JUMAX   :95
inline constexpr Real kTauwMax = static_cast<Real>(2.2361); // TAUWMAX :101
inline constexpr Real kKappa = static_cast<Real>(0.40);     // KAPPA, constants.F90:70

// minss/maxss of w3src3md.o, d the destination: a NaN or a tie gives s. Every site of
// calc_ustar has the variable as d and the constant as s, so WINDSPEED/USTAR = 0/0
// (WINDSPEED = 0 reads TAUT(:,0) = 0) gives SQRTCDM1 = 100 (:1218).
KOKKOS_INLINE_FUNCTION Real minss(Real d, Real s) { return (d < s) ? d : s; }
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }
// INT(): cvttss2si, 0x80000000 for NaN or out of range; then MIN(_,ITAUMAX-1) by cmovg.
KOKKOS_INLINE_FUNCTION int fint(Real x) {
  return (x > static_cast<Real>(-2147483904.) && x < static_cast<Real>(2147483648.))
             ? static_cast<int>(x)
             : static_cast<int>(0x80000000u);
}
KOKKOS_INLINE_FUNCTION int imin(int a, int b) { return (a > b) ? b : a; }
}  // namespace calc_ustar_detail

/// CALC_USTAR(WINDSPEED, TAUW, USTAR, Z0, CHARN) for one point. Called by W3SPR3
/// (w3src3md.F90:325).
///
/// windspeed, tauw: 10 m wind speed (m/s) and wave-supported stress (m2/s2).
/// ustar, z0, charn: written, as the INTENT(OUT) dummies.
/// taut: W3SRC3MD's TAUT(0:ITAUMAX,0:JUMAX), 201 x 201, Fortran column-major, so
///   TAUT(i,j) is taut[i + 201*j]; built by TABU_STRESS (srce_st3_tabu_stress.hpp).
/// deltauw, delu: W3SRC3MD's DELTAUW and DELU, set by TABU_STRESS (:906-907).
/// zzwnd, aalpha: W3GDATMD's ZZWND and AALPHA of the current grid.
///
/// ⚠ A WINDSPEED <= -DELU, or NaN, gives J < 0 and reads TAUT out of bounds, as the
/// Fortran does (no clamp below at :1210).
KOKKOS_INLINE_FUNCTION void calc_ustar(Real windspeed, Real tauw, Real& ustar, Real& z0,
                                       Real& charn, const Real* taut, Real deltauw,
                                       Real delu, Real zzwnd, Real aalpha) {
  using namespace calc_ustar_detail;
  constexpr long kLd = kItauMax + 1;

  const Real tauw_local = maxss(minss(tauw, kTauwMax), static_cast<Real>(0.));  // :1204
  const Real xi = Kokkos::sqrt(tauw_local) / deltauw;
  const int ind = imin(fint(xi), kItauMax - 1);
  const Real deli1 = minss(xi - static_cast<Real>(ind), static_cast<Real>(1.));
  const Real deli2 = static_cast<Real>(1.) - deli1;
  const Real xj = windspeed / delu;
  const int j = imin(fint(xj), kJuMax - 1);
  const Real delj1 = minss(xj - static_cast<Real>(j), static_cast<Real>(1.));
  const Real delj2 = static_cast<Real>(1.) - delj1;
  // :1214, offsets in 64 bits as gfortran forms them (no int overflow on the J < 0 path).
  const long c0 = static_cast<long>(j) * kLd;
  const long c1 = static_cast<long>(j + 1) * kLd;
  const Real us = (taut[c0 + ind] * deli2 + taut[c0 + (ind + 1)] * deli1) * delj2 +
                  (taut[c1 + ind] * deli2 + taut[c1 + (ind + 1)] * deli1) * delj1;
  ustar = us;

  // Determines roughness length (:1218-1224).
  const Real sqrtcdm1 = minss(windspeed / us, static_cast<Real>(100.0));
  z0 = zzwnd * Kokkos::exp(-(kKappa * sqrtcdm1));
  if (us > static_cast<Real>(0.001)) {
    charn = ww::srce::kGrav * z0 / (us * us);  // USTAR**2
  } else {
    charn = aalpha;
  }
}

}  // namespace ww::srce::st3
