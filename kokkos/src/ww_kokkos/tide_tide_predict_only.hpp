// kokkos/src/ww_kokkos/tide_tide_predict_only.hpp
// TIDE_PREDICT_ONLY: tidal elevation series from analysed amplitudes and phases with the
// nodal corrections of VUF, without residuals. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3tidemd.F90:2571-2617 at NOAA-EMC/WW3 03adda91;
// authors M. Foreman (tidal package), adapted to WW3 by F. Ardhuin.
// Evidence: kokkos/port_report/TIDE_PREDICT_ONLY_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::tide {

namespace predict_only_detail {
// TWPI = 3.1415926535898*2. (:87): the literal is default REAL, so the DOUBLE PRECISION
// parameter is the float pi widened, times 2. FAC = TWPI/360. (:88) in double.
inline constexpr double kTwpi = 0x1.921fb6p+2;
inline constexpr double kFac = 0x1.1df46aaaaaaabp-6;
static_assert(kTwpi == static_cast<double>(3.1415926535898f) * 2.0);
static_assert(kFac == kTwpi / 360.0);
// 365.*24. (:2588), folded by gfortran in REAL, widened to double: exact.
inline constexpr double kHoursPerYear = 8760.0;
static_assert(kHoursPerYear == static_cast<double>(365.f * 24.f));
}  // namespace predict_only_detail

/// TIDE_PREDICT_ONLY, host. Dummies in Fortran order, then the W3TIDEMD state it reads.
/// tide_hours (N, REAL(8)); predicted (N,NDEF), written (i-1) + n*(idef-1); xlat is unused,
/// as in the Fortran.
/// tide_ampc, tide_phg: (MC=70, 2). tide_ampci, tide_phgi: (10,10,2). tide_ninf (10).
/// Names are CHARACTER*5 as consecutive 5-byte records: tidecon_name (TIDE_MF, element 1 at
/// index 0), tide_konin (10,10), column major.
/// vuf(const char* konx, double& vx, double& ux, double& fx, int itime) is VUF (:1706-1746),
/// called with the 1-based time index I; it is not translated here and stops the run when
/// the name is unknown, which is why this routine is host only.
template <class Vuf>
inline void tide_predict_only(int itrend, int ndef, int n, const double* tide_hours,
                              float* predicted, float xlat, int tide_mf, int tide_nin,
                              const int* tide_ninf, const char* tidecon_name,
                              const char* tide_konin, const float* tide_ampc,
                              const float* tide_phg, const float* tide_ampci,
                              const float* tide_phgi, Vuf&& vuf) {
  using namespace predict_only_detail;
  constexpr long mc = 70;
  for (int idef = 1; idef <= ndef; ++idef) {
    for (int i = 1; i <= n; ++i) {
      const long c1 = mc * (idef - 1);
      float sum1;
      // :2587-2591.
      if (itrend == 1) {
        sum1 = static_cast<float>(static_cast<double>(tide_ampc[c1]) +
                                  static_cast<double>(tide_phg[c1]) * tide_hours[i - 1] /
                                      kHoursPerYear);
      } else {
        sum1 = tide_ampc[c1];
      }
      // :2593-2598.
      for (int j = 2; j <= tide_mf; ++j) {
        double vx, ux, fx;
        vuf(tidecon_name + 5L * (j - 1), vx, ux, fx, i);
        const long cj = c1 + (j - 1);
        const float arg = static_cast<float>((vx + ux) * kTwpi -
                                             static_cast<double>(tide_phg[cj]) * kFac);
        const float add = static_cast<float>(fx * static_cast<double>(tide_ampc[cj]) *
                                             static_cast<double>(Kokkos::cos(arg)));
        sum1 = sum1 + add;
      }
      // :2600-2609.
      if (tide_nin != 0) {
        for (int k = 1; k <= tide_nin; ++k) {
          for (int k2 = 1; k2 <= tide_ninf[k - 1]; ++k2) {
            double vx, ux, fx;
            const long kk = (k - 1) + 10L * (k2 - 1);
            vuf(tide_konin + 5L * kk, vx, ux, fx, i);
            const long ci = kk + 100L * (idef - 1);
            const float arg = static_cast<float>((vx + ux) * kTwpi -
                                                 static_cast<double>(tide_phgi[ci]) * kFac);
            const float add = static_cast<float>(fx * static_cast<double>(tide_ampci[ci]) *
                                                 static_cast<double>(Kokkos::cos(arg)));
            sum1 = sum1 + add;
          }
        }
      }
      // :2611.
      predicted[(i - 1) + static_cast<long>(n) * (idef - 1)] = sum1;
    }
  }
}

}  // namespace ww::tide
