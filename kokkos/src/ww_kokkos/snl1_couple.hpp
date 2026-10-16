// kokkos/src/ww_kokkos/snl1_couple.hpp
// COUPLE: the Webb (1978) four-wave coupling coefficient of the wavenumbers k1..k4, in
// double precision, for the quadrature tables of the GQM form of NL1 (INSNLGQM).
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3snl1md.F90:1185-1307 at NOAA-EMC/WW3 03adda91, adapted
// from TOMAWAC (M. Benoit, E. Gagnaire-Renou). Evidence: kokkos/port_report/COUPLE_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::snl1 {

namespace couple_detail {
/// COEF = PI*GRAV*GRAV/4.D0 (:1243) with PI = ACOS(-1.) (:1242), a default-REAL ACOS widened
/// to double, and GRAV = 9.806 a REAL parameter (constants.F90:62): gfortran folds it.
inline constexpr double kCoef = 0x1.2e168ec8a8f43p+6;
static_assert(kCoef == static_cast<double>(static_cast<float>(3.14159265358979323846)) *
                           static_cast<double>(9.806f) * static_cast<double>(9.806f) / 4.0);
/// ZERO = 1.D-10 (:1244).
inline constexpr double kZero = 1.e-10;

/// Bits of the diagnostic code, one per WRITE the Fortran does instead of the term; the shim
/// writes the messages. Either bit of a pair means that term was not added ("term not used").
inline constexpr int kNan13 = 1;  // :1284, (1-3) 0/0
inline constexpr int kInf13 = 2;  // :1286, (1-3) infinite value
inline constexpr int kNan14 = 4;  // :1296, (1-4) 0/0
inline constexpr int kInf14 = 8;  // :1298, (1-4) infinite value
}  // namespace couple_detail

/// COUPLE, :1185-1307. diag is set to 0 or to the couple_detail bits of the WRITEs the
/// Fortran would do; the value returned is the Fortran's in every case.
KOKKOS_INLINE_FUNCTION double couple(double xk1, double yk1, double xk2, double yk2,
                                     double xk3, double yk3, double xk4, double yk4,
                                     int& diag) {
  using namespace couple_detail;
  diag = 0;
  // :1246-1254.
  const double rk1 = Kokkos::sqrt(xk1 * xk1 + yk1 * yk1);
  const double rk2 = Kokkos::sqrt(xk2 * xk2 + yk2 * yk2);
  const double rk3 = Kokkos::sqrt(xk3 * xk3 + yk3 * yk3);
  const double rk4 = Kokkos::sqrt(xk4 * xk4 + yk4 * yk4);
  const double wk1 = Kokkos::sqrt(rk1);
  const double wk2 = Kokkos::sqrt(rk2);
  const double wk3 = Kokkos::sqrt(rk3);
  const double wk4 = Kokkos::sqrt(rk4);
  // :1256-1261.
  const double s12 = xk1 * xk2 + yk1 * yk2;
  const double s13 = xk1 * xk3 + yk1 * yk3;
  const double s14 = xk1 * xk4 + yk1 * yk4;
  const double s23 = xk2 * xk3 + yk2 * yk3;
  const double s24 = xk2 * xk4 + yk2 * yk4;
  const double s34 = xk3 * xk4 + yk3 * yk4;
  // :1263-1268.
  const double w1p2 = Kokkos::sqrt((xk1 + xk2) * (xk1 + xk2) + (yk1 + yk2) * (yk1 + yk2));
  const double w1m3 = Kokkos::sqrt((xk1 - xk3) * (xk1 - xk3) + (yk1 - yk3) * (yk1 - yk3));
  const double w1m4 = Kokkos::sqrt((xk1 - xk4) * (xk1 - xk4) + (yk1 - yk4) * (yk1 - yk4));
  const double q12 = (wk1 + wk2) * (wk1 + wk2);
  const double q13 = (wk1 - wk3) * (wk1 - wk3);
  const double q14 = (wk1 - wk4) * (wk1 - wk4);
  // :1272-1278, left to right as gfortran associates it.
  double ddd = 2.00 * q12 * (rk1 * rk2 - s12) * (rk3 * rk4 - s34) / (w1p2 - q12) +
               0.50 * (s12 * s34 + s13 * s24 + s14 * s23) +
               0.25 * (s13 + s24) * q13 * q13 -
               0.25 * (s12 + s34) * q12 * q12 +
               0.25 * (s14 + s23) * q14 * q14 +
               2.50 * rk1 * rk2 * rk3 * rk4 +
               q12 * q13 * q14 * (rk1 + rk2 + rk3 + rk4);
  // :1280-1291.
  const double deno13 = w1m3 - q13;
  const double nume13 = 2.00 * q13 * (rk1 * rk3 + s13) * (rk2 * rk4 + s24);
  if (Kokkos::fabs(deno13) < kZero) {
    diag |= (Kokkos::fabs(nume13) < kZero) ? kNan13 : kInf13;
  } else {
    ddd = ddd + nume13 / deno13;
  }
  // :1292-1303.
  const double deno14 = w1m4 - q14;
  const double nume14 = 2.00 * q14 * (rk1 * rk4 + s14) * (rk2 * rk3 + s23);
  if (Kokkos::fabs(deno14) < kZero) {
    diag |= (Kokkos::fabs(nume14) < kZero) ? kNan14 : kInf14;
  } else {
    ddd = ddd + nume14 / deno14;
  }
  // :1305.
  return kCoef * ddd * ddd / (wk1 * wk2 * wk3 * wk4);
}

}  // namespace ww::snl1
