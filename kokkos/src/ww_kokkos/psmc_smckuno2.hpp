// kokkos/src/ww_kokkos/psmc_smckuno2.hpp
// SMCkUNO2: shift of the spectrum in wavenumber space by refraction, UNO2 scheme with a CFL
// limiter and a positive filter, one direction at a time. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3psmcmd.F90:2821-2890 at NOAA-EMC/WW3 03adda91;
// author Jian-Guo Li. Evidence: kokkos/port_report/SMCKUNO2_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::psmc {

/// CNST = XFR**(-7), :2833. gfortran -O3 builds it as 1/((x2*x2)*(x*x2)), x2 = x*x.
KOKKOS_INLINE_FUNCTION Real smckuno2_cnst(Real xfr) {
  const Real x2 = xfr * xfr;
  const Real x3 = xfr * x2;
  const Real x4 = x2 * x2;
  const Real x7 = x4 * x3;
  return static_cast<Real>(1.0) / x7;
}

/// One direction n (1-based, the Fortran loop counter), :2837-2885.
/// corfr: CoRfr(NTH,0:NK), spethk: SpeTHK(NTH,NK), column-major, 0-based here.
/// dkc: DKC(0:NK+1) as dkc[j]; dks: DKS(-1:NK+1) as dks[j+1].
/// cnst: smckuno2_cnst(XFR); ctmax: CTMAX (w3gdatmd).
/// spectr, sperfr, speflx: the automatic arrays Spectr, SpeRfr, SpeFlx(-1:NK+2), NK+4 Reals
/// each, index j+1; caller-provided, one set per concurrent direction.
KOKKOS_INLINE_FUNCTION void smckuno2_point(int n, const Real* corfr, Real* spethk,
                                           const Real* dkc, const Real* dks, int nk, int nth,
                                           Real cnst, Real ctmax, Real* spectr, Real* sperfr,
                                           Real* speflx) {
  const long st = nth > 0 ? static_cast<long>(nth) : 0L;
  const long n0 = static_cast<long>(n - 1);
  const Real zero = static_cast<Real>(0.0);
  const Real half = static_cast<Real>(0.5);

  spectr[0] = zero;
  spectr[1] = zero;
  for (int j = 1; j <= nk; ++j) spectr[j + 1] = spethk[n0 + st * (j - 1)];
  spectr[nk + 2] = spectr[nk + 1] * cnst;
  spectr[nk + 3] = spectr[nk + 2] * cnst;

  sperfr[0] = zero;
  for (int j = -1; j <= nk + 1; ++j) {
    sperfr[j + 1] = (spectr[j + 2] - spectr[j + 1]) / dks[j + 1];
  }

  for (int j = 0; j <= nk; ++j) {
    const Real cnst6 = corfr[n0 + st * j];
    const Real rj = sperfr[j + 1];
    const Real sgn = Kokkos::copysign(half, rj);
    const Real arj = Kokkos::fabs(rj);
    // MIN operand order from the object (minss keeps its destination only when strictly less).
    if (cnst6 > zero) {
      const Real t = ctmax * dkc[j];
      const Real cnst0 = t < cnst6 ? t : cnst6;
      const Real am = Kokkos::fabs(sperfr[j]);
      const Real mn = am < arj ? am : arj;
      speflx[j + 1] = cnst0 * (spectr[j + 1] + sgn * (dkc[j] - cnst0) * mn);
    } else {
      const Real t = ctmax * dkc[j + 1];
      const Real mc6 = -cnst6;
      const Real cnst0 = t < mc6 ? t : mc6;
      const Real ap = Kokkos::fabs(sperfr[j + 2]);
      const Real mn = ap < arj ? ap : arj;
      speflx[j + 1] = -(cnst0 * (spectr[j + 2] - sgn * (dkc[j + 1] - cnst0) * mn));
    }
  }

  for (int j = 1; j <= nk; ++j) {
    const Real v = spectr[j + 1] + (speflx[j] - speflx[j + 1]) / dkc[j];
    // MAX(0.0, v): maxss/maxps keep v only when v > 0, so a NaN becomes 0.
    spethk[n0 + st * (j - 1)] = v > zero ? v : zero;
  }
}

/// SMCkUNO2 over all directions, n = 1..NTH in the Fortran order (:2835). Arguments as in
/// smckuno2_point; xfr: XFR (w3gdatmd).
KOKKOS_INLINE_FUNCTION void smckuno2(const Real* corfr, Real* spethk, const Real* dkc,
                                     const Real* dks, int nk, int nth, Real xfr, Real ctmax,
                                     Real* spectr, Real* sperfr, Real* speflx) {
  const Real cnst = smckuno2_cnst(xfr);
  for (int n = 1; n <= nth; ++n) {
    smckuno2_point(n, corfr, spethk, dkc, dks, nk, nth, cnst, ctmax, spectr, sperfr, speflx);
  }
}

}  // namespace ww::psmc
