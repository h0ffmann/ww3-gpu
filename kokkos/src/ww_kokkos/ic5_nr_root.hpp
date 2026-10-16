// kokkos/src/ww_kokkos/ic5_nr_root.hpp
// NR_ROOT of W3SIC5MD: Newton-Raphson iteration for the complex wavenumber of the ice-covered
// dispersion relation, from the first guess of POLYROOTS, with an optional random kick.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic5md.F90:1765-1924 at NOAA-EMC/WW3 03adda91;
// author Q. Liu (module authors Q. Liu, E. Rogers).
// Evidence: kokkos/port_report/NR_ROOT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <complex>

#include <Kokkos_Core.hpp>

#include "ic5_nr_corr.hpp"

namespace ww::ic5 {

namespace nr_root_detail {
/// Returned where the Fortran writes format 1001 'NR_ROOT' on IAPROC == NAPERR and calls
/// EXTCDE(1) (:1908-1911). The shim writes and stops the run.
inline constexpr int kTooManyIterations = 1;

/// ERRTOL is a default REAL (w3sic5md.F90:144), widened to double for the test on ABS(DK).
inline constexpr float kErrtol = 1.E-12f;

/// ABS of a COMPLEX(8) is glibc's cabs in the object; std::abs of std::complex<double> lowers
/// to the same call. ⚠ A device has no cabs: Kokkos::abs (hypot) need not give its bits.
KOKKOS_INLINE_FUNCTION double cabs(const Kokkos::complex<double>& z) {
  KOKKOS_IF_ON_HOST((return std::abs(std::complex<double>(z.real(), z.imag()));))
  KOKKOS_IF_ON_DEVICE((return Kokkos::abs(z);))
}
}  // namespace nr_root_detail

/// NR_ROOT(C1, C2, H, GUESS), :1765-1924. C1, C2, GUESS are COMPLEX(KDPC) = COMPLEX(8) (C2 is
/// complex at :1854, though the header's table says real), H is REAL(KDP). The result goes to
/// `root`; on a nonzero return `root` is not written.
/// ic5maxiter, ic5rkick, ic5kfilter: IC5PARS(6), IC5PARS(7), IC5PARS(8) of W3GDATMD.
/// init_random_seed(): INIT_RANDOM_SEED (:2266-2347), called once when ic5rkick > 0.5; it seeds
/// libgfortran's generator from SYSTEM_CLOCK, which the caller owns.
/// random_r4(): returns the float of one CALL RANDOM_NUMBER(TRANVAL) of libgfortran's generator.
/// ⚠ Both stay with the caller (the Fortran side of the shim): the kick path is not reproducible
/// from run to run in WW3 itself, and a device has no libgfortran generator.
/// Returns 0, or kTooManyIterations where the Fortran stops.
template <class InitRandomSeed, class RandomR4>
KOKKOS_INLINE_FUNCTION int nr_root(const Kokkos::complex<double>& c1, const Kokkos::complex<double>& c2, double h,
                                   const Kokkos::complex<double>& guess, Kokkos::complex<double>& root,
                                   float ic5maxiter, float ic5rkick, float ic5kfilter,
                                   InitRandomSeed&& init_random_seed, RandomR4&& random_r4) {
  Kokkos::complex<double> k0 = guess;
  Kokkos::complex<double> dk = nr_corr(k0, c1, c2, h);
  double k1r = k0.real() - dk.real();
  double k1i = k0.imag() - dk.imag();
  int iter = 0;

  if (ic5rkick > 0.5f) init_random_seed();

  while (nr_root_detail::cabs(dk) > static_cast<double>(nr_root_detail::kErrtol)) {
    k0 = Kokkos::complex<double>(k1r, k1i);
    dk = nr_corr(k0, c1, c2, h);
    k1r = k0.real() - dk.real();
    k1i = k0.imag() - dk.imag();
    iter = iter + 1;
    // :1902-1906, REAL(K1) stays double and IC5KFILTER is widened to it.
    if (ic5rkick > 0.5f && Kokkos::fabs(k1r) < static_cast<double>(ic5kfilter)) {
      const float tranval = random_r4();
      // K1 + 2*TRANVAL: 2*TRANVAL in REAL, then (x, 0) in COMPLEX(8); 0 + Im(K1) is kept, it
      // turns -0 into +0.
      const double kick = static_cast<double>(2.0f * tranval);
      k1r = kick + k1r;
      k1i = 0.0 + k1i;
    }
    // :1908, ITER is converted to REAL for the comparison.
    if (static_cast<float>(iter) >= ic5maxiter) return nr_root_detail::kTooManyIterations;
  }
  root = Kokkos::complex<double>(k1r, k1i);
  return 0;
}

}  // namespace ww::ic5
