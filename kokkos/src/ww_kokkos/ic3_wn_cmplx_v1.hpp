// kokkos/src/ww_kokkos/ic3_wn_cmplx_v1.hpp
// WN_CMPLX_V1: complex wavenumber in ice (IC3 "V1", Wang and Shen 2010) from an open-water first
// guess, stepped from a 10 s period down to the wave's period when that is shorter.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic3md.F90:839-981 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, S. Zieger (from Zhao and Shen's Clarkson code).
// Evidence: kokkos/port_report/WN_CMPLX_V1_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "ic3_cmplx_root_muller_v1.hpp"
#include "ic3_wn_cmplx_hf.hpp"

namespace ww::ic3 {

/// WN_CMPLX_V1(SIGMA, WN_O, ES, NU, DICE, HICE, DEPTH): DOUBLE PRECISION scalars, returns DOUBLE
/// COMPLEX, here written to `wn`. Returns 0, or the EXTCDE code of CMPLX_ROOT_MULLER_V1
/// (:1244, :1268) at the first call that stops; `wn` is then not written and the caller does
/// the WRITE. Every product of a REAL literal and a DOUBLE COMPLEX is the full complex product
/// with (literal, 0.0), as gfortran -O3 emits it: the products with 0.0 carry NaN and the sign
/// of zero, and in the object every imaginary part is addsd with wi*c as its first operand.
KOKKOS_INLINE_FUNCTION int wn_cmplx_v1(double sigma, double wn_o, double es, double nu,
                                       double dice, double hice, double depth,
                                       Kokkos::complex<double>& wn) {
  using cd = Kokkos::complex<double>;
  // g++ pairs the lanes of X2 after the first root as (wr*0.0) + (wi*c) in vaddsubpd; the
  // object's addsd has wi*c first, which keeps its NaN when both addends are NaN.
  using wn_cmplx_hf_detail::slp_barrier;
  // DBLE(TPI), TPI = 2.0 * PI (REAL), PI = 3.141592653589793 (constants.F90:72-73).
  constexpr double tpi = 0x1.921fb6p+2;
  static_assert(tpi == static_cast<double>(2.0f * 3.141592653589793f));
  // TS = 10. (:944) is exact; DBLE(TPI)/TS is folded by gfortran (:954, :959).
  constexpr double ts = 10.0;
  constexpr double tpi_ts = 0x1.41b2f8p-1;
  static_assert(tpi_ts == tpi / ts);
  // REAL literals promoted to DOUBLE (:947, :950-952, :955-957, :974-976).
  constexpr double hmin = 0x1.0624dep-10;
  static_assert(hmin == static_cast<double>(0.001f));
  constexpr double c001 = 0x1.47ae14p-7;
  static_assert(c001 == static_cast<double>(0.01f));
  constexpr double c01 = 0x1.99999ap-4;
  static_assert(c01 == static_cast<double>(0.1f));
  constexpr double c08 = 0x1.99999ap-1;
  static_assert(c08 == static_cast<double>(0.8f));
  constexpr double c09 = 0x1.ccccccp-1;
  static_assert(c09 == static_cast<double>(0.90f));
  constexpr double c11 = 0x1.19999ap+0;
  static_assert(c11 == static_cast<double>(1.1f));
  constexpr double c12 = 0x1.333334p+0;
  static_assert(c12 == static_cast<double>(1.2f));

  // :947-948. CMPLX(WN_O, 0.) has the default (REAL) kind: WN_O is rounded to float.
  if (hice < hmin) {
    wn = cd(static_cast<double>(static_cast<float>(wn_o)), 0.0);
    return 0;
  }
  const double t = tpi / sigma;  // :943
  cd p3;
  int ierr = 0;
  if (t < ts) {
    const double dt = ts - t;
    const int nsub = wn_cmplx_hf_detail::fint_d(dt * 10.0);  // :945
    // :950-954.
    cd x0(c001, 0.0);
    cd x1(c01, 0.0);
    cd x2(1.0, 0.0);
    cd wn0;
    ierr = cmplx_root_muller_v1(x0, x1, x2, 0, tpi_ts, es, nu, dice, hice, depth, wn0);
    if (ierr != 0) return ierr;
    double wr = wn0.real();
    double wi = wn0.imag();
    // :955-959.
    x0 = cd(wr * c09 - wi * 0.0, wi * c09 + wr * 0.0);
    x1 = cd(wr, wi);
    x2 = cd(wr * c11 - slp_barrier(wi * 0.0), wi * c11 + slp_barrier(wr * 0.0));
    ierr = cmplx_root_muller_v1(x0, x1, x2, 1, tpi_ts, es, nu, dice, hice, depth, p3);
    if (ierr != 0) return ierr;
    for (int i = 1; i <= nsub; ++i) {
      wr = p3.real();
      wi = p3.imag();
      // :961-963.
      x0 = cd(wr * c09 - wi * 0.0, wi * c09 + wr * 0.0);
      x1 = cd(wr, wi);
      x2 = cd(wr * c11 - wi * 0.0, wi * c11 + wr * 0.0);
      // :964. REAL(NSUB) and REAL(I) are REAL (float), then promoted.
      const double tt = ts - dt / static_cast<double>(static_cast<float>(nsub)) *
                                 static_cast<double>(static_cast<float>(i));
      // :965-966.
      ierr = cmplx_root_muller_v1(x0, x1, x2, 1, tpi / tt, es, nu, dice, hice, depth, p3);
      if (ierr != 0) return ierr;
    }
  } else {
    // :969-973.
    cd x0(c001, 0.0);
    cd x1(c01, 0.0);
    cd x2(1.0, 0.0);
    cd wn0;
    ierr = cmplx_root_muller_v1(x0, x1, x2, 0, sigma, es, nu, dice, hice, depth, wn0);
    if (ierr != 0) return ierr;
    const double wr = wn0.real();
    const double wi = wn0.imag();
    // :974-978.
    x0 = cd(wr * c08 - wi * 0.0, wi * c08 + wr * 0.0);
    x1 = cd(wr, wi);
    x2 = cd(wr * c12 - slp_barrier(wi * 0.0), wi * c12 + slp_barrier(wr * 0.0));
    ierr = cmplx_root_muller_v1(x0, x1, x2, 1, sigma, es, nu, dice, hice, depth, p3);
    if (ierr != 0) return ierr;
  }
  wn = p3;
  return 0;
}

}  // namespace ww::ic3
