// kokkos/src/ww_kokkos/ic3_w3ic3wncg_v1.hpp
// W3IC3WNCG_V1: wavenumber, attenuation and group velocity of waves in ice at every frequency,
// from the IC3 "V1" dispersion relation (Wang and Shen 2010): for each frequency the complex
// wavenumber is found by WN_CMPLX_V1 (first frequency, or a period of 10 s and longer) or by
// WN_CMPLX_HF (shorter periods, continued from the previous frequency), and the group velocity
// is DELTA(SIGMA) / DELTA(WN_R). Phase 1, translated, not wired, not regtested.
// WW3 heritage: WW3/model/src/w3sic3md.F90:657-836 at h0ffmann/WW3 96892c95 (byte-identical to
// NOAA-EMC/WW3 03adda91: git -C WW3 diff 03adda91 HEAD -- model/src/w3sic3md.F90 is empty
// for this module's source); authors E. Rogers, S. Zieger. Called by W3SIC3
// (ic3_w3sic3.hpp, :549) and W3WAVE (w3wavemd.F90:1063).
// Evidence: kokkos/port_report/W3IC3WNCG_V1_KOKKOS.md.
// SPDX-FileCopyrightText: 2009 National Weather Service (NWS), National Oceanic and Atmospheric Administration
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "ic3_cmplx_root_muller_v1.hpp"
#include "ic3_delta.hpp"
#include "ic3_wn_cmplx_hf.hpp"
#include "ic3_wn_cmplx_v1.hpp"
#include "prop_wavnu1.hpp"
#include "real.hpp"

namespace ww::ic3 {

/// W3IC3WNCG_V1(WN_R, WN_I, CG, ICE1, ICE2, ICE3, ICE4, DPT). wn_r, wn_i, cg: the Fortran
/// assumed-shape WN_R(:), WN_I(:), CG(:) of n elements, 0-based; n is NK or NK+2 (:775-786), and
/// sig the SIGMA of that size, SIG(1:NK) or SIG(0:NK+1), 0-based. nk is NK. ic3hilim and
/// ic3kilim are IC3PARS(10) and IC3PARS(11). disp is the DISTAB table WAVNU1 reads. dsig and
/// dwn are n-element work arrays for the two DELTA results (the Fortran allocates them at :827).
/// Returns 0; 3 where the Fortran writes "CANNOT DETERMINE BOUNDS OF WAVENUMBER ARRAY" and
/// calls EXTCDE(3) (:784-785), with nothing written; or the code of WN_CMPLX_V1 or WN_CMPLX_HF
/// (2) where they stop in EXTCDE(2), with the outputs written so far left as they are. The
/// caller does the WRITEs. n >= 2 (DELTA reads X(2), :1782).
KOKKOS_INLINE_FUNCTION int w3ic3wncg_v1(Real* wn_r, Real* wn_i, Real* cg, Real ice1, Real ice2,
                                        Real ice3, Real ice4, Real dpt, int nk, int n,
                                        const Real* sig, Real ic3hilim, Real ic3kilim,
                                        const ww::prop::DispTable& disp, Real* dsig, Real* dwn) {
  using cd = Kokkos::complex<double>;
  // TPI = 2.0 * PI (constants.F90:72-73), REAL; STENSEC = TPI/10.0 is a REAL quotient (:759).
  constexpr Real tpi = static_cast<Real>(2.0f * 3.141592653589793f);
  constexpr Real stensec = tpi / 10.0f;
  constexpr double khmax = 18.0;  // :750

  // :766-770. Each REAL is widened to REAL(8) once.
  double hwat = static_cast<double>(dpt);
  double hice = static_cast<double>(ice1);
  const double nu = static_cast<double>(ice2);
  const double dice = static_cast<double>(ice3);
  const double es_mod = static_cast<double>(ice4);

  // :773. MIN(DBLE(IC3HILIM), HICE) is a minpd of the object with IC3HILIM as the destination:
  // a tie or a NaN gives HICE.
  const double hilim = static_cast<double>(ic3hilim);
  hice = (hilim < hice) ? hilim : hice;

  // :775-786. KL = 1; SIGMA is SIG(1:NK) or SIG(0:NK+1).
  if (n != nk && n != nk + 2) return 3;
  const int kl = 1;
  const int ku = n;

  cd wncomplex_old(0.0, 0.0);  // :788
  const CmplxRootMullerV1 root{};
  for (int ik = kl; ik <= ku; ++ik) {  // :790
    Real k_ocean;
    Real cg_ocean;
    ww::prop::wavnu1(disp, sig[ik - 1], dpt, k_ocean, cg_ocean);  // :793
    const double k_noice = static_cast<double>(k_ocean);  // :794
    const double kh = k_noice * hwat;                       // :797
    if (kh > khmax) {  // :798, HWAT stays reduced for the next frequency
      hwat = khmax / k_noice;
    }
    cd wncomplex;
    int code;
    if (ik > kl && sig[ik - 1] > stensec) {  // :803
      code = wn_cmplx_hf(static_cast<double>(sig[ik - 1]), k_noice, es_mod, nu, dice, hice, hwat,
                         static_cast<double>(sig[ik - 2]), wncomplex_old, wncomplex, root);
    } else {
      code = wn_cmplx_v1(static_cast<double>(sig[ik - 1]), k_noice, es_mod, nu, dice, hice, hwat,
                         wncomplex);
    }
    if (code != 0) return code;
    wncomplex_old = wncomplex;  // :807, :813
    // :819-823. REAL() of a DOUBLE COMPLEX rounds to REAL. MIN(WN_I(IK), IC3KILIM) is a minss of
    // the object with WN_I as the destination: a tie or a NaN gives IC3KILIM.
    const Real wi = static_cast<Real>(wncomplex.imag());
    wn_i[ik - 1] = (wi < ic3kilim) ? wi : ic3kilim;
    wn_r[ik - 1] = static_cast<Real>(wncomplex.real());
  }

  // :827-828. CG_IC3 = DELTA(SIGMA) / DELTA(WN_R); CG = CG_IC3.
  delta(sig, n, dsig);
  delta(wn_r, n, dwn);
  for (int i = 0; i < n; ++i) cg[i] = dsig[i] / dwn[i];
  return 0;
}

}  // namespace ww::ic3
