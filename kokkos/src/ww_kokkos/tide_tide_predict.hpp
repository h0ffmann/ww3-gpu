// kokkos/src/ww_kokkos/tide_tide_predict.hpp
// TIDE_PREDICT: tidal elevation series from analysed amplitudes and phases with the nodal
// corrections of VUF, its residuals against the data, their RMS and standard deviation.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3tidemd.F90:2513-2568 at NOAA-EMC/WW3 03adda91;
// authors M. Foreman (tidal package), adapted to WW3 by F. Ardhuin.
// Evidence: kokkos/port_report/TIDE_PREDICT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::tide {

namespace predict_detail {
// TWPI = 3.1415926535898*2. (:87): the literal is default REAL, so the DOUBLE PRECISION
// parameter is the float pi widened, times 2. FAC = TWPI/360. (:88) in double.
inline constexpr double kTwpi = 0x1.921fb6p+2;
inline constexpr double kFac = 0x1.1df46aaaaaaabp-6;
static_assert(kTwpi == static_cast<double>(3.1415926535898f) * 2.0);
static_assert(kFac == kTwpi / 360.0);
// 365.*24. (:2534), folded by gfortran in REAL, widened to double: exact.
inline constexpr double kHoursPerYear = 8760.0;
static_assert(kHoursPerYear == static_cast<double>(365.f * 24.f));
}  // namespace predict_detail

/// TIDE_PREDICT, host. Dummies in Fortran order, then the W3TIDEMD state it reads.
/// hours (N, REAL(8)); datain, predicted, resid (N,NDEF), element (i,idef) at
/// (i-1) + n*(idef-1); sdev, rmsr (NDEF, REAL(8)); xlat is unused, as in the Fortran.
/// tide_ampc, tide_phg: (MC=70, 2). tide_ampci, tide_phgi: (10,10,2). tide_ninf (10).
/// Names are CHARACTER*5 as consecutive 5-byte records: tidecon_name (TIDE_MF, element 1 at
/// index 0), tide_konin (10,10), column major.
/// vuf(const char* konx, double& vx, double& ux, double& fx, int itime) is VUF (:1706-1746),
/// called with the 1-based time index I; it is not translated here and stops the run when
/// the name is unknown, which is why this routine is host only.
template <class Vuf>
inline void tide_predict(int itrend, int ndef, int n, const double* hours, const float* datain,
                         float* predicted, float* resid, float xlat, double* sdev, double* rmsr,
                         int tide_mf, int tide_nin, const int* tide_ninf,
                         const char* tidecon_name, const char* tide_konin,
                         const float* tide_ampc, const float* tide_phg,
                         const float* tide_ampci, const float* tide_phgi, Vuf&& vuf) {
  using namespace predict_detail;
  constexpr long mc = 70;
  // :2528.
  const int m = 2 * tide_mf;
  for (int idef = 1; idef <= ndef; ++idef) {
    // :2531 SDEV=0.D0 clears the whole array on every IDEF, so only SDEV(NDEF) survives.
    for (int l = 0; l < ndef; ++l) sdev[l] = 0.0;
    for (int i = 1; i <= n; ++i) {
      const long c1 = mc * (idef - 1);
      float sum1;
      // :2533-2537.
      if (itrend == 1) {
        sum1 = static_cast<float>(static_cast<double>(tide_ampc[c1]) +
                                  static_cast<double>(tide_phg[c1]) * hours[i - 1] /
                                      kHoursPerYear);
      } else {
        sum1 = tide_ampc[c1];
      }
      // :2539-2544.
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
      // :2546-2555.
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
      // :2557-2560. RESID**2 is squared in REAL, then widened.
      const long ii = (i - 1) + static_cast<long>(n) * (idef - 1);
      predicted[ii] = sum1;
      resid[ii] = datain[ii] - sum1;
      sdev[idef - 1] = sdev[idef - 1] + static_cast<double>(resid[ii] * resid[ii]);
    }
    // :2563-2565, in REAL; N-M is an integer, zero or negative when N <= 2*TIDE_MF.
    const float ssq = static_cast<float>(sdev[idef - 1]);
    rmsr[idef - 1] = static_cast<double>(Kokkos::sqrt(ssq / static_cast<float>(n - m)));
    sdev[idef - 1] = static_cast<double>(Kokkos::sqrt(ssq / static_cast<float>(n)));
  }
}

}  // namespace ww::tide
