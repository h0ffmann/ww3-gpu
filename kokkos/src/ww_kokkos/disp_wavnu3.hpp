// kokkos/src/ww_kokkos/disp_wavnu3.hpp
// WAVNU3 and WAVNU_LOCAL: wavenumber and group velocity of one (sigma, depth) from
// Beji's (2003) explicit approximation of w**2 = g k tanh(k h). Phase 1, translated,
// not regtested. WW3 heritage: WW3-noaa/model/src/w3dispmd.F90:348-454, :456-547 at
// NOAA-EMC/WW3 03adda91; author A. Roland. Evidence: kokkos/port_report/WAVNU3_KOKKOS.md,
// WAVNU_LOCAL_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "disp_distab.hpp"
#include "real.hpp"

namespace ww::disp {

namespace wavnu3_detail {
// glibc's scalar powf/expf on the host, as the Fortran object calls them.
// ⚠ On a device Kokkos' pow/exp (CUDA libdevice) are not shown to return glibc's bits.
KOKKOS_INLINE_FUNCTION Real pow_(Real x, Real y) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::powf(x, y);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::pow(x, y);))
#else
  return Kokkos::pow(x, y);
#endif
}
KOKKOS_INLINE_FUNCTION Real exp_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::expf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::exp(x);))
#else
  return Kokkos::exp(x);
#endif
}

// minss/maxss as AT&T `minss src, dst` computes dst: a NaN or a +0/-0 tie gives src.
// Each call names (dst, src) as the object has them.
KOKKOS_INLINE_FUNCTION Real mins(Real dst, Real src) { return (dst < src) ? dst : src; }
KOKKOS_INLINE_FUNCTION Real maxs(Real dst, Real src) { return (dst > src) ? dst : src; }

// Folded REAL PARAMETERs and literals of :432-436, :443-448, as bits.
inline constexpr Real kZpi = 0x1.921fb6p+2f;    // ZPI = 2*PI, 0x40c90fdb
inline constexpr Real kZpi2 = 0x1.3bd3cep+5f;   // ZPI*ZPI folded, 0x421de9e7
inline constexpr Real kBeta1 = 0x1.8cccccp+0f;  // 1.55, 0x3fc66666
inline constexpr Real kBeta2 = 0x1.4cccccp+0f;  // 1.3, 0x3fa66666
inline constexpr Real kBeta3 = 0x1.ba5e36p-3f;  // 0.216, 0x3e5d2f1b
inline constexpr Real kPow = 0x1.170a3ep+0f;    // 1.09, 0x3f8b851f
inline constexpr Real kKdmax = 20.0f;           // KDMAX, 0x41a00000
static_assert(kZpi == static_cast<Real>(2) * ww::kPi, "ZPI = 2*REAL(PI)");
static_assert(kZpi2 == kZpi * kZpi, "ZPI*ZPI folded in REAL");
static_assert(kGrav == 0x1.39cac0p+3f, "GRAV, 0x411ce560");
}  // namespace wavnu3_detail

/// WAVNU3(SI, H, K, CG), :348-454. No guard for si <= 0 or h <= 0 (:392-393).
KOKKOS_INLINE_FUNCTION void wavnu3(Real si, Real h, Real& k, Real& cg) {
  using namespace wavnu3_detail;
  const Real one = static_cast<Real>(1);
  const Real tp = si / kZpi;                                     // :443
  const Real kh0 = kZpi2 * h / kGrav * tp * tp;                  // :444, ((h*ZPI2)/GRAV*TP)*TP
  const Real tmp = (kh0 * kBeta2 + kBeta1) + kh0 * kBeta3 * kh0;  // :445
  // :446. MIN(KDMAX,TMP) is minss 20,TMP (TMP < 20 ? TMP : 20); MIN(KDMAX,KH0) is
  // minss KH0,20 (20 < KH0 ? 20 : KH0). The calls run powf, expf, tanhf in that order.
  const Real p = pow_(kh0, kPow);
  const Real e = exp_(mins(tmp, kKdmax));
  const Real t = detail::tanh_(mins(kKdmax, kh0));
  const Real kh = ((p / e + one) * kh0) / Kokkos::sqrt(t);
  k = kh / h;  // :447
  // :448. MIN(KDMAX,2*KH) is minss 2KH,20 (20 < 2KH ? 20 : 2KH).
  const Real kh2 = kh + kh;
  const Real s = detail::sinh_(mins(kKdmax, kh2));
  cg = ((kh2 / s + one) * static_cast<Real>(0.5)) * si / k;
}

/// WAVNU_LOCAL(SIG, DW, WNL, CGL), :456-547: WAVNU3 at MAX(DMIN,DW), DMIN of the grid.
KOKKOS_INLINE_FUNCTION void wavnu_local(Real sig, Real dw, Real dmin, Real& wnl, Real& cgl) {
  const Real depth = wavnu3_detail::maxs(dmin, dw);  // :543, maxss DW,DMIN: a NaN DW gives DW
  wavnu3(sig, depth, wnl, cgl);                       // :545
}

}  // namespace ww::disp
