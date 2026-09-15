// kokkos/src/ww_kokkos/snl1_config.hpp
// The inputs of WAVEWATCH III's DIA nonlinear interactions, and the View types
// the ported kernel speaks.
//
// WW3 heritage: WW3/model/src/w3snl1md.F90 (WAVEWATCH III 7.14, `develop`),
// W3SNL1 lines 115-473 and INSNL1 lines 483-779. Config holds exactly what those
// two routines read from W3GDATMD, under the WW3 names, so a reader can put this
// file next to the Fortran and check it field by field. SIG travels separately
// because W3SNL1 wants it on the device and INSNL1 wants it on the host.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::snl1 {

/// The lab's device memory space, spelled out once. Every View below names it:
/// an unqualified View picks up whatever Kokkos' default is, which is exactly the
/// kind of implicit choice that turns into a silent host/device mismatch.
using DeviceSpace = Kokkos::DefaultExecutionSpace::memory_space;

using IntView1D = Kokkos::View<int*, DeviceSpace>;
using RealView1D = Kokkos::View<Real*, DeviceSpace>;
using ConstRealView1D = Kokkos::View<const Real*, DeviceSpace>;
/// (nspec, npts) or (nk, npts): LayoutLeft, because the Fortran side hands over
/// column-major spectra and the port must not transpose them.
using RealView2D = Kokkos::View<Real**, Kokkos::LayoutLeft, DeviceSpace>;
using ConstRealView2D = Kokkos::View<const Real**, Kokkos::LayoutLeft, DeviceSpace>;

using HostRealView1D = Kokkos::View<Real*, Kokkos::HostSpace>;
using ConstHostRealView1D = Kokkos::View<const Real*, Kokkos::HostSpace>;

// PI, TPI and TPIINV exactly as constants.F90 lines 72-75 builds them: PI is
// rounded to REAL first, and only then doubled and inverted. Evaluating
// 1/(2*pi) in double and narrowing at the end is a different number, by up to a
// ULP, and a ULP is the whole budget of a bit-parity test.
inline constexpr Real kPi = static_cast<Real>(3.141592653589793);
inline constexpr Real kTpi = static_cast<Real>(2) * kPi;
inline constexpr Real kTpiInv = static_cast<Real>(1) / kTpi;

/// Everything INSNL1 and W3SNL1 read from W3GDATMD, in WW3's own names.
struct Config {
  int nk;      ///< NK      number of frequency bands
  int nth;     ///< NTH     number of directions
  Real xfr;    ///< XFR     frequency increment factor sigma(k+1)/sigma(k)
  Real dth;    ///< DTH     directional increment, 2*pi/NTH
  Real lam;    ///< LAM     quadruplet lambda (LAMBDA, default 0.25)
  Real snlc1;  ///< SNLC1   NLPROP / GRAV**4
  Real kdcon;  ///< KDCONV  shallow-water scaling, default 0.75
  Real kdmn;   ///< KDMIN   lower bound on the scaled depth, default 0.50
  Real snls1;  ///< SNLCS1  default 5.5
  Real snls2;  ///< SNLCS2  default 0.833
  Real snls3;  ///< SNLCS3  default -1.25
  Real fachfe; ///< FACHFE  XFR**(-FACHF), the high-frequency tail factor

  /// NSPEC = NK*NTH, the length of one packed spectrum.
  constexpr int nspec() const { return nk * nth; }
};

}  // namespace ww::snl1
