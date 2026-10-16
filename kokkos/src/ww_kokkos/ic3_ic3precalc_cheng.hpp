// kokkos/src/ww_kokkos/ic3_ic3precalc_cheng.hpp
// IC3PRECALC_CHENG: one column of the IC3 (Cheng) table, k_r, k_i and c_g of the ice-covered
// dispersion relation for one ice thickness at every frequency 0:NK+1. Phase 1, translated, not
// regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic3md.F90:2047-2160 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, S. Zieger, X. Zhao, S. Cheng. Evidence: kokkos/port_report/IC3PRECALC_CHENG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "ic3_cginic3_cheng.hpp"
#include "ic3_smooth_k.hpp"
#include "prop_wavnu1.hpp"
#include "real.hpp"

namespace ww::ic3 {

/// IC3PRECALC_CHENG. wn_r, wn_i, cg: Fortran (0:NK+1), here 0-based with the same index; wn_r and
/// wn_i are read at ik-1, ik-2 after being written. Module state: nk, sig = SIG(0:NK+1), disp the
/// DISTAB table WAVNU1 reads. diff, removeid: nk + 2 floats each, SMOOTH_K's automatic arrays.
/// wn_precalc_cheng is WN_PRECALC_CHENG (:2164-2417), not translated yet: a host callable
/// (Kokkos::complex<double>& wn, double sigma, double sigmam1, Kokkos::complex<double> wn_o,
///  Kokkos::complex<double> wnm1, Kokkos::complex<double> wnm2, double es_mod, double nu,
///  double dice, double hice, double depth, int& switchid, int ik, int ku); it writes wn and may
/// write switchid, and PRINTs (:2412-2414), so this runs on the host. The caller passes nk >= 1
/// (SMOOTH_K needs n >= 3).
template <class WnPrecalcCheng>
inline void ic3precalc_cheng(Real* wn_r, Real* wn_i, Real* cg, Real ice1, Real ice2, Real ice3,
                             Real ice4, Real dpt, int nk, const Real* sig,
                             const ww::prop::DispTable& disp, Real* diff, Real* removeid,
                             WnPrecalcCheng&& wn_precalc_cheng) {
  using C8 = Kokkos::complex<double>;
  // :2125-2129, each REAL widened to REAL(8) once.
  const double depth = static_cast<double>(dpt);
  const double hice = static_cast<double>(ice1);
  const double nu = static_cast<double>(ice2);
  const double dice = static_cast<double>(ice3);
  const double es_mod = static_cast<double>(ice4);
  const int kl = 0;
  const int ku = nk + 1;
  int switchid = 0;
  for (int ik = kl; ik <= ku; ++ik) {
    // :2136-2137. K_NOICE is REAL(8) only to be narrowed back by CMPLX at :2150 (exact).
    Real k_ocean;
    Real cg_ocean;
    ww::prop::wavnu1(disp, sig[ik], dpt, k_ocean, cg_ocean);
    C8 wnm1;
    C8 wnm2;
    double sigmam1;
    if (ik < kl + 2) {  // :2140-2143
      wnm1 = C8(0.0, 0.0);
      wnm2 = C8(0.0, 0.0);
      sigmam1 = 0.0;
    } else {  // :2145-2147, CMPLX of two REALs then widened, exact.
      wnm1 = C8(static_cast<double>(wn_r[ik - 1]), static_cast<double>(wn_i[ik - 1]));
      wnm2 = C8(static_cast<double>(wn_r[ik - 2]), static_cast<double>(wn_i[ik - 2]));
      sigmam1 = static_cast<double>(sig[ik - 1]);
    }
    const C8 wn_o(static_cast<double>(k_ocean), 0.0);  // :2150
    C8 wncomplex;
    // :2152-2153. SWITCHID is the 0-based IK of the switch point.
    wn_precalc_cheng(wncomplex, static_cast<double>(sig[ik]), sigmam1, wn_o, wnm1, wnm2, es_mod,
                     nu, dice, hice, depth, switchid, ik, ku);
    // :2154-2155, REAL(8) rounded to REAL.
    wn_r[ik] = static_cast<Real>(wncomplex.real());
    wn_i[ik] = static_cast<Real>(wncomplex.imag());
  }
  // :2157-2158. SMOOTH_K and CGINIC3_CHENG index 1:N while SWITCHID is a 0:NK+1 index, so
  // SMOOTH_K marks the two frequencies below the switch point; reproduced as written.
  const int n = ku - kl + 1;
  smooth_k(wn_r, wn_i, sig, n, switchid, diff, removeid);
  cginic3_cheng(cg, sig, wn_r, n);
}

}  // namespace ww::ic3
