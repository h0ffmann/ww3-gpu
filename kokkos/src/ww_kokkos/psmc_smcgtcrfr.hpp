// kokkos/src/ww_kokkos/psmc_smcgtcrfr.hpp
// SMCGtCrfr: great-circle turning and refraction of one SMC cell spectrum as a rotation by
// CoRfr bins in direction. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3psmcmd.F90:2717-2790 at NOAA-EMC/WW3 03adda91;
// author Jian-Guo Li. Evidence: kokkos/port_report/SMCGTCRFR_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"

namespace ww::psmc {

/// corfr: CoRfr(NTH,NK), spethk: SpeTHK(NTH,NK), column-major, 1-based in Fortran, 0-based here;
/// spethk is overwritten. nk, nth: NK, NTH (w3gdatmd). spegct, spectr: the automatic arrays
/// SpeGCT(NTH), Spectr(NTH) (:2725), caller-provided, NTH each.
KOKKOS_INLINE_FUNCTION void smcgtcrfr(const Real* corfr, Real* spethk, int nk, int nth,
                                      Real* spegct, Real* spectr) {
  for (int n = 1; n <= nk; ++n) {
    const long col = static_cast<long>(n - 1) * static_cast<long>(nth);
    for (int i = 0; i < nth; ++i) spectr[i] = spethk[col + i];
    for (int i = 0; i < nth; ++i) spegct[i] = static_cast<Real>(0.0);

    for (int j = 1; j <= nth; ++j) {
      const Real cnst6 = corfr[col + (j - 1)];
      const Real cnst5 = Kokkos::fabs(cnst6);
      // INT(CNST5) is one cvttss2si shared by MOD and FLOAT (:2744, :2747); out of range or NaN
      // gives INT_MIN, then K < 0 and L, M may leave 1..NTH, as in the Fortran.
      const int icnst5 = ww::st4::fint(cnst5);
      const int k = icnst5 % nth;

      const Real cnst1 = cnst5 - static_cast<Real>(icnst5);
      const Real cnst2 = static_cast<Real>(1.0) - cnst1;

      int l;
      int m;
      if (cnst6 > static_cast<Real>(0.0)) {
        l = j + k;
        m = j + k + 1;
        if (l > nth) l = l - nth;
        if (m > nth) m = m - nth;
      } else {
        l = j - k;
        m = j - k - 1;
        if (l < 1) l = l + nth;
        if (m < 1) m = m + nth;
      }
      spegct[l - 1] = spegct[l - 1] + spectr[j - 1] * cnst2;
      spegct[m - 1] = spegct[m - 1] + spectr[j - 1] * cnst1;
    }

    for (int i = 0; i < nth; ++i) spethk[col + i] = spegct[i];
  }
}

}  // namespace ww::psmc
