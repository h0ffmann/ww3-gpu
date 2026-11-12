// kokkos/src/ww_kokkos/bt9_w3sbt9.hpp
// W3SBT9: dissipation by viscous fluid mud after Ng (2000), one spectral point
// (S and D from the action density AC). Phase 1, translated, not wired, not regtested.
// WW3 heritage: WW3/model/src/w3sbt9md.F90:118-406 at h0ffmann/WW3 96892c95 (byte-identical
// to NOAA-EMC/WW3 03adda91 as far as this routine goes: the same -O3 object was compiled
// from both trees, see the port report); authors M. Orzech, W. E. Rogers.
// Evidence: kokkos/port_report/W3SBT9_KOKKOS.md.
// SPDX-FileCopyrightText: 2009 National Weather Service (NWS), National Oceanic and Atmospheric Administration
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Host side, not translated (the Fortran does them before the arithmetic, :264-281):
//  - the three INFLAGS1(-2), (-1), (0) tests and the loads RHOM = MUDD(IX,IY),
//    DM = MUDT(IX,IY), KINVISM = MUDV(IX,IY); the caller passes the values and, when a flag
//    is off, does the WRITE(NDSE,*) and EXTCDE(1..3) itself;
//  - the NaN diagnostics of :386-390 and EXTCDE(1) (:391): w3sbt9_point returns the first IS
//    whose DMW is not finite (1-based) and fills Bt9Nan, the caller prints and stops.
// Not translated because the -O3 object does not compute them (dead stores): the CALC_ND calls
// (:307, :355), ND_ROCK, ND_MUD, CG_ROCK, NWAVE_ROCK, SBLTW, the SND2 = 0 and CWAVE = 0 resets (:310-311, :356-357).
//
// ⚠ SMUDWD(IS) is stored only where KD < KDCUTOFF (:377-379) and the Fortran reads all of
// SMUDWD(MAPWN(IS)) at :399 (malloc'd, never initialised): an element it did not store is
// indeterminate in the Fortran. The caller's `smudwd` work array stands in for that memory,
// and the port does not initialise it.
#pragma once

#include <cmath>

#include <Kokkos_Core.hpp>

#include "bt9_ng.hpp"
#include "real.hpp"

namespace ww::bt9 {

namespace w3sbt9_detail {
// The object calls glibc's scalar sinhf (NWAVE_MUD, :344-350).
// ⚠ On a device, Kokkos' sinh is CUDA's and not shown to return glibc's bits.
KOKKOS_INLINE_FUNCTION Real sinh_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::sinhf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::sinh(x);))
#else
  return Kokkos::sinh(x);
#endif
}
// GRAV, DWAT, NU_WATER of constants.F90:62-68 as REAL.
inline constexpr Real kGrav = static_cast<Real>(9.806);
inline constexpr Real kDwat = static_cast<Real>(1000.);
inline constexpr Real kNuWater = static_cast<Real>(1.31E-6);
}  // namespace w3sbt9_detail

/// What the host prints before EXTCDE(1) when DMW(IS) is not finite (:383-391).
struct Bt9Nan {
  int is = 0;  ///< IS, 1-based
  Real kd = 0, k_mud = 0, cg_mud = 0, nwave_mud = 0;
};

/// W3SBT9(AC, H_WDEPTH, S, D, IX, IY) for one point, arrays 0-based.
///   nk, nspec  W3GDATMD NK, NSPEC
///   sig        SIG(1:NK) (the Fortran's SIG is a pointer with bounds 0:NK+1: pass SIG(1))
///   mapwn      MAPWN(1:NSPEC), Fortran 1-based wavenumber index of each spectral point
///   rhom, dm, kinvism  MUDD, MUDT, MUDV at (IX,IY), already loaded
///   ac         AC(NSPEC); s, d  out, S(NSPEC), D(NSPEC)
///   smudwd     work, NK reals (the Fortran's malloc'd SMUDWD(NK))
/// Returns 0, or 1 when DMW(IS) is NaN or infinite (the Fortran then stops, EXTCDE(1), :391); then
/// `nan` is filled and s, d are not complete.
KOKKOS_INLINE_FUNCTION int w3sbt9_point(int nk, int nspec, const Real* sig, const int* mapwn,
                                        Real rhom, Real dm, Real kinvism, const Real* ac,
                                        Real h_wdepth, Real* s, Real* d, Real* smudwd,
                                        Bt9Nan& nan) {
  using w3sbt9_detail::kDwat;
  using w3sbt9_detail::kGrav;
  using w3sbt9_detail::kNuWater;
  const Real one = static_cast<Real>(1.);
  const Real two = static_cast<Real>(2.);
  const Real half = static_cast<Real>(0.5);
  const Real kdcutoff = static_cast<Real>(10.0);  // :317

  const Real rootdg = Kokkos::sqrt(h_wdepth / kGrav);  // :283
  for (int is = 1; is <= nk; ++is) {
    const Real sg = sig[is - 1];
    const Real snd = sg * rootdg;  // :287
    Real k_rock;
    if (snd >= static_cast<Real>(2.5)) {  // :288
      k_rock = sg * sg / kGrav;           // :290
    } else if (snd < static_cast<Real>(1.E-6)) {
      k_rock = snd / h_wdepth;  // :296
    } else {
      const Real snd2 = snd * snd;  // :302
      // SND2**2, **3, **4 as -O3 builds them: s4 = s2*s2, s6 = s2*s4, s8 = s4*s4.
      const Real s4 = snd2 * snd2;
      const Real s6 = snd2 * s4;
      const Real s8 = s4 * s4;
      // :303-304 left to right, 1./(...) is a division, PAREN_EXPR keeps the sum whole.
      const Real poly = one + static_cast<Real>(0.666) * snd2 + static_cast<Real>(0.445) * s4 -
                        static_cast<Real>(0.105) * s6 + static_cast<Real>(0.272) * s8;
      const Real cwave = Kokkos::sqrt(h_wdepth * kGrav / (snd2 + one / poly));
      k_rock = sg / cwave;  // :305
    }

    Real k_mud;
    Real dmw = 0.0f;  // :322, DMW(IS) = 0.0
    const Real kd_rock = h_wdepth * k_rock;  // :323
    if (kd_rock < kdcutoff && dm > static_cast<Real>(1.0E-5)) {  // :325
      const Real zeta = Kokkos::sqrt(kinvism / kNuWater);   // :328
      const Real gamma = kDwat / rhom;                     // :329
      const Real sbltm = Kokkos::sqrt(kinvism * two / sg);  // :331
      const Real dtilde = dm / sbltm;                      // :333
      ng(sg, h_wdepth, dtilde, zeta, sbltm, gamma, k_rock, k_mud, dmw);  // :334
    } else {
      k_mud = k_rock;  // :338
    }

    // :342-350
    const Real cwave = sg / k_mud;
    const Real ztmp0 = h_wdepth * (k_mud * two);
    const Real ztmp = (ztmp0 < static_cast<Real>(70)) ? w3sbt9_detail::sinh_(ztmp0)
                                                      : static_cast<Real>(1.0E+30);
    const Real nwave_mud = (one + ztmp0 / ztmp) * half;
    const Real cg_mud = nwave_mud * cwave;  // :352

    const Real kd = h_wdepth * k_mud;  // :376
    if (kd < kdcutoff) {
      smudwd[is - 1] = cg_mud * (dmw * two);  // :379
    }
    // :383-384: .NOT. (DMW >= -HUGE .AND. DMW <= HUGE)
    if (!(dmw >= -static_cast<Real>(3.4028234663852886e+38) &&
          dmw <= static_cast<Real>(3.4028234663852886e+38))) {
      nan.is = is;
      nan.kd = kd;
      nan.k_mud = k_mud;
      nan.cg_mud = cg_mud;
      nan.nwave_mud = nwave_mud;
      return 1;
    }
  }

  // :398-402
  for (int is = 1; is <= nspec; ++is) {
    d[is - 1] = -smudwd[mapwn[is - 1] - 1];
  }
  for (int is = 0; is < nspec; ++is) {
    s[is] = ac[is] * d[is];
  }
  return 0;
}

}  // namespace ww::bt9
