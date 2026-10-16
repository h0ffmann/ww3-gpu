// kokkos/src/ww_kokkos/srce_st6_util.hpp
// IRANGE, RRANGE and TAUWINDS, the private helper functions of W3SRC6MD (ST6) that
// LFACTOR, TAU_WAVE_ATMOS, W3SIN6 and W3SDS6 call. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src6md.F90 at NOAA-EMC/WW3 03adda91 (IRANGE
// :1342-1371, RRANGE :1387-1416, TAUWINDS :1440-1469); author S. Zieger.
// Evidence: kokkos/port_report/W3SRC6MD_IRANGE_KOKKOS.md, RRANGE_KOKKOS.md,
// TAUWINDS_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"

namespace ww::srce::st6 {

namespace util_detail {
// GRAV * DWAT (constants.F90:62-63), folded by gfortran to 9.806e+3 in TAUWINDS and in
// every inlined copy of it (w3src6md.F90:1467).
inline constexpr Real kGravDwat = 0x1.327p+13f;
static_assert(kGravDwat == static_cast<Real>(9.806) * static_cast<Real>(1000.));
}  // namespace util_detail

/// N of IRANGE and RRANGE (:1365, :1410): INT(REAL(X1-X0)/REAL(DX))+1, a float division
/// truncated as cvttss2si does (DX = 0 or |quotient| >= 2^31 gives INT_MIN+1).
/// The Fortran result has SIZE max(N,0); range_size() returns that.
KOKKOS_INLINE_FUNCTION int range_n(int x0, int x1, int dx) {
  return ww::st4::fint(static_cast<Real>(x1 - x0) / static_cast<Real>(dx)) + 1;
}

/// SIZE of the array IRANGE/RRANGE(x0, x1, dx) returns; size a caller's buffer with it.
KOKKOS_INLINE_FUNCTION int range_size(int x0, int x1, int dx) {
  const int n = range_n(x0, x1, dx);
  return n > 0 ? n : 0;
}

/// IRANGE(X0,X1,DX) (:1342-1371): ix[i-1] = X0+(I-1)*DX for I = 1..N, values as the
/// Fortran gives them (so 1-based when X0 = 1, e.g. IKN = IRANGE(1,NSPEC,NTH) at :450,
/// :697). ix holds range_size(x0,x1,dx) elements; returns that size. The ALLOCATE of
/// the result becomes the caller's ix.
KOKKOS_INLINE_FUNCTION int irange(int x0, int x1, int dx, int* ix) {
  const int n = range_n(x0, x1, dx);
  for (int i = 1; i <= n; ++i) ix[i - 1] = x0 + (i - 1) * dx;
  return n > 0 ? n : 0;
}

/// RRANGE(X0,X1,DX) (:1387-1416): rx[i-1] = REAL(X0+(I-1)*DX), the sum in INTEGER
/// then one int-to-float rounding. rx holds range_size(x0,x1,dx) elements; returns
/// that size.
KOKKOS_INLINE_FUNCTION int rrange(int x0, int x1, int dx, Real* rx) {
  const int n = range_n(x0, x1, dx);
  for (int i = 1; i <= n; ++i) rx[i - 1] = static_cast<Real>(x0 + (i - 1) * dx);
  return n > 0 ? n : 0;
}

/// TAUWINDS(SDENSIG,CINV,DSII) (:1440-1469) = GRAV*DWAT*SUM(SDENSIG*CINV*DSII) over the
/// n = SIZE(SDENSIG) elements, 0-based. Each term is (s*c)*d; the SUM adds the terms one
/// by one from 0.0 in index order (the object's vector loops add the lanes in order,
/// :1467), then one multiplication by the folded 9806. A caller passing an expression
/// such as SDENS10Hz*LF10Hz (:1067) passes that product rounded to REAL, element by
/// element, as the Fortran's temporary holds it.
KOKKOS_INLINE_FUNCTION Real tauwinds(const Real* sdensig, const Real* cinv, const Real* dsii,
                                     int n) {
  Real sum = static_cast<Real>(0.);
  for (int i = 0; i < n; ++i) sum = sum + (sdensig[i] * cinv[i]) * dsii[i];
  return sum * util_detail::kGravDwat;
}

}  // namespace ww::srce::st6
