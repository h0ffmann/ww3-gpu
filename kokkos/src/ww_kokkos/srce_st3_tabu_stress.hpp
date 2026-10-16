// kokkos/src/ww_kokkos/srce_st3_tabu_stress.hpp
// TABU_STRESS of W3SRC3MD: the ST3 friction velocity table TAUT(TAUW,U10) = SQRT(TAU),
// host code run once at initialisation. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src3md.F90:95-101 (module parameters and tables),
// :822-943 (TABU_STRESS) at NOAA-EMC/WW3 03adda91; authors F. Ardhuin, H. L. Tolman,
// adapted from WAM (P.A.E.M. Janssen, KNMI 1990; J. Bidlot, ECMWF 2004).
// Evidence: kokkos/port_report/W3SRC3MD_TABU_STRESS_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cmath>

#include "real.hpp"

namespace ww::srce::st3 {

namespace tabu_stress_detail {
// W3SRC3MD module parameters (:95, :100-101). Kept in this namespace: calc_ustar_detail
// (srce_st3_calc_ustar.hpp) declares the same names and is opened by a using-directive, so
// the same names in ww::srce::st3 would make them ambiguous there.
inline constexpr int kItauMax = 200;
inline constexpr int kJuMax = 200;
inline constexpr Real kUmax = static_cast<Real>(50.);
inline constexpr Real kTauwMax = static_cast<Real>(2.2361);
}  // namespace tabu_stress_detail

// TAUT is (0:ITAUMAX, 0:JUMAX), column-major: TAUT(I,J) at I + 201*J.
inline constexpr int kTautSize =
    (tabu_stress_detail::kItauMax + 1) * (tabu_stress_detail::kJuMax + 1);

/// The module scalars TABU_STRESS sets beside TAUT (:97).
struct TabuStressSteps {
  Real delu;
  Real deltauw;
};

namespace tabu_stress_detail {
// minss/maxss of w3src3md.o, d the destination: a NaN or a tie gives s.
inline Real minss(Real d, Real s) { return (d < s) ? d : s; }
inline Real maxss(Real d, Real s) { return (d > s) ? d : s; }

inline constexpr Real kGrav = static_cast<Real>(9.806);   // constants.F90:62
inline constexpr Real kKappa = static_cast<Real>(0.40);   // constants.F90:70
inline constexpr Real kEps1 = static_cast<Real>(0.00001); // :893
inline constexpr int kNiter = 10;                         // :892

// Folded by gfortran: DELU = UMAX/FLOAT(JUMAX) (:906), DELTAUW = TAUWMAX/FLOAT(ITAUMAX)
// (:907), WCD = SQRT(0.0012875) (:912-913), XM+1 = 1.5 with XM = 0.50 (:927).
inline constexpr Real kDelu = 0x1p-2f;
static_assert(kDelu == kUmax / static_cast<Real>(kJuMax));
inline constexpr Real kDeltauw = 0x1.6e5cd4p-7f;
static_assert(kDeltauw == kTauwMax / static_cast<Real>(kItauMax));
inline constexpr Real kCdrag = static_cast<Real>(0.0012875);
inline constexpr Real kWcd = 0x1.25f17cp-5f;
// kWcd is SQRT(kCdrag) correctly rounded: kCdrag lies strictly between the squares of
// the midpoints to its float neighbours (products exact in double).
static_assert(static_cast<double>(kCdrag) >
              (static_cast<double>(kWcd) - 0x1p-29) * (static_cast<double>(kWcd) - 0x1p-29));
static_assert(static_cast<double>(kCdrag) <
              (static_cast<double>(kWcd) + 0x1p-29) * (static_cast<double>(kWcd) + 0x1p-29));
inline constexpr Real kXm = static_cast<Real>(0.50);
inline constexpr Real kXmp1 = 0x1.8p0f;
static_assert(kXmp1 == kXm + static_cast<Real>(1));
}  // namespace tabu_stress_detail

/// TABU_STRESS (:822-943). Reads W3GDATMD ZZWND, AALPHA, ZZ0MAX; writes taut[kTautSize]
/// (TAUT, column-major, 0-based) and returns DELU, DELTAUW.
inline TabuStressSteps tabu_stress(Real zzwnd, Real aalpha, Real zz0max, Real* taut) {
  using namespace tabu_stress_detail;
  const Real one = static_cast<Real>(1.);

  for (int i = 0; i <= kItauMax; ++i) {
    const Real ztauw = sqr(static_cast<Real>(i) * kDeltauw);  // :909
    for (int j = 0; j <= kJuMax; ++j) {
      const Real utop = static_cast<Real>(j) * kDelu;  // :911
      const Real ustold = utop * kWcd;                 // :914
      Real tauold = maxss(sqr(ustold), ztauw + kEps1);  // :915
      for (int iter = 1; iter <= kNiter; ++iter) {
        const Real x = ztauw / tauold;
        Real ust = std::sqrt(tauold);
        Real zz00 = aalpha * tauold / kGrav;
        if (zz0max != static_cast<Real>(0)) zz00 = minss(zz00, zz0max);  // :920
        // (1.-X)**XM with REAL XM: a powf call in the object, not sqrtf (:922).
        const Real omx = one - x;
        const Real zz0 = zz00 / std::pow(omx, kXm);
        // ALOG(ZZWND/ZZ0) is evaluated once for :925 and :926.
        const Real lg = std::log(zzwnd / zz0);
        const Real kutop = kKappa * utop;
        const Real f = ust - kutop / lg;
        const Real delf = one - kutop / sqr(lg) * static_cast<Real>(2.) / ust *
                                    (one - kXmp1 * x) / omx;  // :926-927
        ust = ust - f / delf;
        tauold = maxss(sqr(ust), ztauw + kEps1);  // UST**2. folded to UST*UST (:929)
      }
      taut[i + (kItauMax + 1) * j] = std::sqrt(tauold);  // :931
    }
  }
  // Zero wind, zero stress (:937-941).
  for (int i = 0; i <= kItauMax; ++i) taut[i] = static_cast<Real>(0.0);
  return TabuStressSteps{kDelu, kDeltauw};
}

}  // namespace ww::srce::st3
