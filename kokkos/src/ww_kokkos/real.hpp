// kokkos/src/ww_kokkos/real.hpp
// The lab's working precision and the arithmetic helper every kernel needs.
//
// WAVEWATCH III stores the action spectrum in default REAL, and the port keeps
// that width on purpose: float32 halves the bytes moved per quadruplet, and phase
// 1 of the port is "translate, do not improve". Every literal in kernel code must
// therefore be written as ww::Real, never as a bare double -- the build turns on
// -Wconversion so that a stray double shows up as a warning rather than as a
// silent 2x bandwidth cost.
//
// SPDX-License-Identifier: MIT
#pragma once

#include <Kokkos_Core.hpp>

namespace ww {

/// Working precision of the spectral state, matching WW3's default REAL.
using Real = float;

/// PI exactly as constants.F90 line 72 builds it: the double literal rounded to
/// REAL once. Defined here, and only here, so the fixtures and the ported kernels
/// cannot drift apart by a ULP -- a ULP is the whole budget of a bit-parity test.
inline constexpr Real kPi = static_cast<Real>(3.141592653589793);

/// x squared. Spelled out so kernels never call std::pow for an integer power.
KOKKOS_INLINE_FUNCTION constexpr Real sqr(Real x) { return x * x; }

}  // namespace ww
