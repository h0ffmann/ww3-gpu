// kokkos/src/ww_kokkos/is2_w3rpwnice.hpp
// W3RPWNICE of W3SIS2MD: wavenumber, damping and group speed under a continuous ice sheet
// from the Robinson and Palmer dispersion relation, solved per frequency by the Newton
// iteration FINDROOTS_NR. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sis2md.F90:1098-1187 (W3RPWNICE) and :1204-1301
// (FINDROOTS_NR) at NOAA-EMC/WW3 03adda91; author C. Sevigny (module authors F. Ardhuin,
// P. Nicot, C. Sevigny, G. Boutin). Evidence: kokkos/port_report/W3RPWNICE_KOKKOS.md and
// kokkos/port_report/FINDROOTS_NR_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "is2_funcd_fderiv.hpp"
#include "is2_funcd_fval.hpp"
#include "real.hpp"

namespace ww::is2 {

namespace w3rpwnice_detail {
inline constexpr Real kGrav = static_cast<Real>(9.806);  // GRAV, constants.F90:62
// Folded by gfortran (GIMPLE of w3sis2md.F90:1172, :1183):
inline constexpr Real kYoung = 0x1.473ac0p+32f;     // YOUNG = 5.49E+9, :1169
inline constexpr Real kFlexDen = 0x1.5d70a4p+3f;    // 12*(1-POISSON**2), :1172
inline constexpr Real kDensGrav = 0x1.3a1932p+13f;  // DENS*GRAV, :1183
inline constexpr Real kDens = static_cast<Real>(1025.0);    // DENS, :1166
inline constexpr Real kDensIce = static_cast<Real>(922.5);  // DENS_ICE, :1167
static_assert(kYoung == static_cast<Real>(5.49E+9));
static_assert(kFlexDen == 12 * (static_cast<Real>(1) - static_cast<Real>(0.3) * static_cast<Real>(0.3)));
static_assert(kDensGrav == kDens * kGrav);
inline constexpr Real kViscRp = static_cast<Real>(10);  // VISC_RP, :1165
// TOL = 1E-9 is REAL, compared with the DOUBLE ABS(DX), :1271, :1284.
inline constexpr double kTol = static_cast<double>(static_cast<Real>(1E-9));
inline constexpr int kMaxit = 300;  // MAXIT, :1270
/// Returned where FINDROOTS_NR stops in EXTCDE(1) (:1290-1293). Unreachable: J is at most
/// MAXIT inside the loop (:1278), and gfortran removes the branch.
inline constexpr int kExtcde = 1;

/// ABS of a DOUBLE COMPLEX: the object calls libm's cabs, which glibc computes as hypot.
KOKKOS_INLINE_FUNCTION double cabs(double re, double im) { return Kokkos::hypot(re, im); }
}  // namespace w3rpwnice_detail

/// FINDROOTS_NR(GUESS, VISC_RP, X, ICEH, SIGMA), :1204-1301. Returns 0, or kExtcde.
KOKKOS_INLINE_FUNCTION int findroots_nr(const Kokkos::complex<double>& guess, Real visc_rp,
                                        Kokkos::complex<double>& x, Real iceh, Real sigma) {
  using namespace w3rpwnice_detail;
  double x0r = guess.real();
  double x0i = guess.imag();
  bool unfinished_roots = true;
  int j = 1;
  while (j < kMaxit && unfinished_roots) {
    const Kokkos::complex<double> fval = funcd_fval(Kokkos::complex<double>(x0r, x0i), visc_rp, iceh, sigma);
    const Kokkos::complex<double> fderiv =
        funcd_fderiv(Kokkos::complex<double>(x0r, x0i), visc_rp, iceh, sigma);
    // :1281, DX = FVAL/FDERIV as gfortran inlines it (Smith's algorithm, -fcx-fortran-rules),
    // not Kokkos::complex's division.
    const double fr = fval.real();
    const double fi = fval.imag();
    const double dr = fderiv.real();
    const double di = fderiv.imag();
    double dxr;
    double dxi;
    if (Kokkos::fabs(di) > Kokkos::fabs(dr)) {
      const double r = dr / di;
      const double den = r * dr + di;
      dxr = (fi + r * fr) / den;
      dxi = (r * fi - fr) / den;
    } else {
      const double r = di / dr;
      const double den = dr + di * r;
      dxr = (fr + fi * r) / den;
      dxi = (fi - fr * r) / den;
    }
    // :1282-1283.
    x0r = x0r - dxr;
    x0i = x0i - dxi;
    x = Kokkos::complex<double>(x0r, x0i);
    // :1284, ABS(FVAL) is evaluated only when ABS(DX) <= TOL, as in the object.
    if (cabs(dxr, dxi) > kTol || cabs(fr, fi) > 1.0) {
      j = j + 1;
    } else {
      unfinished_roots = false;
    }
    if (j > kMaxit) return kExtcde;
  }
  return 0;
}

/// W3RPWNICE(ICEH, WN_I, DAMPING, CG_I), :1098-1187, arrays 0-based over IK = 1..NK
/// (wn_i, damping, cg_i are WN_I(1:NK), DAMPING(1:NK), CG_I(1:NK)); sig is SIG(1:NK) of
/// W3GDATMD's SIG(0:NK+1). Returns 0, or kExtcde from FINDROOTS_NR (never, see above).
KOKKOS_INLINE_FUNCTION int w3rpwnice(Real iceh, Real* wn_i, Real* damping, Real* cg_i, int nk,
                                     const Real* sig) {
  using namespace w3rpwnice_detail;
  // :1172, ICEH**3 as (ICEH*ICEH)*ICEH.
  const Real flex_rigid = ((iceh * iceh) * iceh * kYoung) / kFlexDen;
  // :1174, CMPLX without KIND is single COMPLEX, widened on assignment.
  Kokkos::complex<double> gs1(static_cast<double>((sig[0] * sig[0]) / kGrav), 0.0);
  Kokkos::complex<double> wn_root(0.0, 0.0);

  for (int ik = 1; ik <= nk; ++ik) {
    const Real s = sig[ik - 1];
    // :1177-1178.
    int ierr = findroots_nr(gs1, static_cast<Real>(0.), wn_root, iceh, s);
    if (ierr != 0) return ierr;
    wn_i[ik - 1] = static_cast<Real>(wn_root.real());
    // :1179-1180.
    ierr = findroots_nr(gs1, kViscRp, wn_root, iceh, s);
    if (ierr != 0) return ierr;
    damping[ik - 1] = static_cast<Real>(wn_root.imag());
    // :1181.
    const Real w = wn_i[ik - 1];
    gs1 = Kokkos::complex<double>(static_cast<double>(w), 0.0);
    // :1182-1183, WN_I(IK)**4 as (W*W)*(W*W).
    const Real w2 = w * w;
    const Real num = ((static_cast<Real>(5) * flex_rigid) * (w2 * w2) + kDensGrav) - (kDensIce * iceh) * (s * s);
    const Real den = (static_cast<Real>(2) * s) * (kDens + (kDensIce * w) * iceh);
    cg_i[ik - 1] = num / den;
  }
  return 0;
}

}  // namespace ww::is2
