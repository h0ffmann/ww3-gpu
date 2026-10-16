// kokkos/src/ww_kokkos/ic5_w3ic5wncg.hpp
// W3IC5WNCG of W3SIC5MD: complex wavenumber (k_r, k_i) in ice for every frequency, one FSDISP
// call per frequency at the period TPI/SIG(IK). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic5md.F90:493-633 at NOAA-EMC/WW3 03adda91;
// authors Q. Liu, E. Rogers.
// Evidence: kokkos/port_report/W3IC5WNCG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::ic5 {

namespace w3ic5wncg_detail {
/// Returned where the Fortran stops in EXTCDE(3) (:611-614): SIZE(WN_R) is neither NK nor NK+2.
/// The shim writes format 900 on IAPROC == NAPERR and stops the run.
inline constexpr int kExtcde = 3;
}  // namespace w3ic5wncg_detail

/// W3IC5WNCG(WN_R, WN_I, CG, HICE, IVISC, RHOI, ISMODG, HWAT), arrays 0-based and contiguous.
/// wn_r, wn_i: n_wn values (Fortran base 1), INOUT. cg: n_cg values, read only for SIZE(CG).
/// nk, sig: module state NK and SIG(0:NK+1) of W3GDATMD, sig[0] = SIG(0).
/// sigma: caller-provided work for the ALLOCATABLE SIGMA (:600), at least max(n_cg, n_wn) values.
/// FSDISP (w3sic5md.F90:652-943) has no header yet: `fsdisp` stands for it, called as
/// fsdisp(hice, ivisc, rhoi, ismodg, hwat, wt, wnr, wni) with wnr, wni float&, and returning 0,
/// or the EXTCDE code where FSDISP stops; a nonzero code is returned at once, before WN_R(IK),
/// WN_I(IK) are stored, the values of the earlier IK kept as the Fortran leaves them.
/// Returns 0, kExtcde, or FSDISP's code.
template <class Fsdisp>
KOKKOS_INLINE_FUNCTION int w3ic5wncg(float* wn_r, float* wn_i, int n_wn,
                                     [[maybe_unused]] const float* cg, int n_cg, float hice,
                                     float ivisc, float rhoi, float ismodg, float hwat, int nk,
                                     const float* sig, float* sigma, const Fsdisp& fsdisp) {
  // TPI = 2.0 * PI, PI = 3.141592653589793 (constants.F90:72-73), folded by gfortran.
  constexpr float tpi = 0x1.921fb6p+2f;
  static_assert(tpi == 2.0f * 3.141592653589793f);

  // :600-601. ALLOCATE(SIGMA(SIZE(CG))); SIGMA = 0.
  for (int i = 0; i < n_cg; ++i) sigma[i] = 0.0f;

  // :603-615. SIGMA = SIG(...) reallocates SIGMA to the section's size (-frealloc-lhs, the
  // __builtin_realloc in the object), so only SIZE(WN_R) decides the bounds.
  int ku;
  if (n_wn == nk) {
    ku = nk;
    for (int i = 0; i < nk; ++i) sigma[i] = sig[i + 1];  // SIG(1:NK)
  } else if (n_wn == nk + 2) {
    ku = nk + 2;
    for (int i = 0; i < nk + 2; ++i) sigma[i] = sig[i];  // SIG(0:NK+1)
  } else {
    return w3ic5wncg_detail::kExtcde;
  }

  // :618-623, KL = 1.
  for (int ik = 1; ik <= ku; ++ik) {
    const float wt = tpi / sigma[ik - 1];
    float twn_r;
    float twn_i;
    const int code = fsdisp(hice, ivisc, rhoi, ismodg, hwat, wt, twn_r, twn_i);
    if (code != 0) return code;
    wn_r[ik - 1] = twn_r;
    wn_i[ik - 1] = twn_i;
  }
  return 0;
}

}  // namespace ww::ic5
