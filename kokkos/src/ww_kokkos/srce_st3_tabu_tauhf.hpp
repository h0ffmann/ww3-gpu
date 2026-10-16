// kokkos/src/ww_kokkos/srce_st3_tabu_tauhf.hpp
// TABU_TAUHF of W3SRC3MD (ST3, WAM4+): host build of the high-frequency wave-supported
// stress table TAUHFT(0:IUSTAR,0:IALPHA). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src3md.F90:958-1116 at NOAA-EMC/WW3 03adda91 (module
// constants and tables :95-99, called by INSIN3 :809); author F. Ardhuin. Evidence:
// kokkos/port_report/W3SRC3MD_TABU_TAUHF_KOKKOS.md.
// Not the ST4 routine of st4_tabu_tauhf.cpp: ST3's takes a frequency (OMEGAC = TPI*FRMAX).
// Host only: expf, logf and sqrtf are glibc's, as in w3src3md.o.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <array>
#include <cmath>

#include "real.hpp"
#include "srce_params.hpp"

namespace ww::srce::st3 {

/// IUSTAR, IALPHA (w3src3md.F90:96): TAUHFT is (0:100,0:200).
inline constexpr int kTauhfIustar = 100;
inline constexpr int kTauhfIalpha = 200;

namespace tabu_tauhf_detail {
inline constexpr int kJtot = 250;                          // JTOT :1045
inline constexpr Real kKappa = static_cast<Real>(0.40);    // KAPPA, constants.F90:70
// Folded by gfortran (values in .rodata.cst4 of w3src3md.o):
inline constexpr Real kDelust = 0x1.99999ap-5f;   // USTARM/REAL(IUSTAR) :1058, 0x3d4ccccd
inline constexpr Real kKappa2 = 0x1.47ae16p-3f;   // KAPPA**2 :1060, 0x3e23d70b
inline constexpr Real kX0Grav = 0x1.f61134p-2f;   // X0*GRAV :1077, 0x3efb089a
static_assert(kDelust == static_cast<Real>(5.) / static_cast<Real>(kTauhfIustar));
static_assert(kKappa2 == kKappa * kKappa);
static_assert(kX0Grav == static_cast<Real>(0.05) * ww::srce::kGrav);

// minss/maxss of w3src3md.o, d the destination: a NaN or a tie gives s. Every site has the
// expression as d and the constant, OMEGAC or ZZ0MAX as s (:1073, :1075, :1077, :1079,
// :1090-1092).
inline Real minss(Real d, Real s) { return (d < s) ? d : s; }
inline Real maxss(Real d, Real s) { return (d > s) ? d : s; }
}  // namespace tabu_tauhf_detail

/// TABU_TAUHF's step scalars, W3SRC3MD's DELUST and DELALP (:1058-1059).
struct TauhfSteps {
  Real delust;
  Real delalp;
};

/// TABU_TAUHF(FRMAX), host.
///
/// frmax: maximum frequency (Hz); INSIN3 passes SIG(NK)*TPIINV (:809).
/// aalpha, bbeta, zzalp, zz0max: W3GDATMD's AALPHA, BBETA, ZZALP, ZZ0MAX of the grid.
/// tauhft: written, (IUSTAR+1)*(IALPHA+1) = 101*201 floats, Fortran column-major, so
///   TAUHFT(k,l) is tauhft[k + 101*l]. The caller keeps the returned DELUST and DELALP
///   with it, as W3SIN3 reads all three.
/// The Fortran ALLOCATEs W(JTOT) per call (:1065); here it is a local std::array, as the
/// routine is a host table builder, not a kernel.
inline TauhfSteps tabu_tauhf(Real frmax, Real aalpha, Real bbeta, Real zzalp, Real zz0max,
                             Real* tauhft) {
  using namespace tabu_tauhf_detail;
  constexpr int kLd = kTauhfIustar + 1;

  const Real alpham = static_cast<Real>(20.) * aalpha;  // :1057
  const Real delust = kDelust;
  const Real delalp = alpham / static_cast<Real>(kTauhfIalpha);
  const Real const1 = bbeta / kKappa2;
  const Real omegac = ww::srce::kTpi * frmax;

  for (int i = 0; i < kLd * (kTauhfIalpha + 1); ++i) tauhft[i] = static_cast<Real>(0.);

  std::array<Real, kJtot> w;  // w[j-1] = W(J)
  for (int j = 1; j < kJtot - 1; ++j) w[j] = static_cast<Real>(1.);
  w[0] = static_cast<Real>(0.5);
  w[kJtot - 1] = static_cast<Real>(0.5);

  for (int l = 0; l <= kTauhfIalpha; ++l) {
    for (int k = 0; k <= kTauhfIustar; ++k) {
      const Real ust = maxss(static_cast<Real>(k) * delust, static_cast<Real>(0.000001));
      Real zz00 = ust * ust * aalpha / ww::srce::kGrav;
      // .NE. is true for a NaN ZZ0MAX (ucomiss/jp), so MIN is taken then.
      if (zz0max != static_cast<Real>(0)) zz00 = minss(zz00, zz0max);
      const Real zz0 = zz00 * (static_cast<Real>(1) + static_cast<Real>(l) * delalp / aalpha);
      const Real omegacc = maxss(kX0Grav / ust, omegac);
      // g++ keeps errno (gfortran does not) and calls sqrtf for a negative argument
      // only; sqrtss and glibc's sqrtf return the same default NaN there.
      const Real yc = omegacc * std::sqrt(zz0 / ww::srce::kGrav);
      const Real dely = maxss((static_cast<Real>(1.) - yc) / static_cast<Real>(kJtot),
                              static_cast<Real>(0.));
      // Summed in J order into TAUHFT(K,L) (:1083-1096).
      Real& t = tauhft[k + kLd * l];
      for (int j = 1; j <= kJtot; ++j) {
        const Real y = yc + static_cast<Real>(j - 1) * dely;
        const Real omega = y * std::sqrt(ww::srce::kGrav / zz0);
        const Real cm = ww::srce::kGrav / omega;
        const Real zx = ust / cm + zzalp;
        const Real zarg = minss(kKappa / zx, static_cast<Real>(20.));
        const Real zmu =
            minss(ww::srce::kGrav * zz0 / (cm * cm) * std::exp(zarg), static_cast<Real>(1.));
        const Real zlog = minss(std::log(zmu), static_cast<Real>(0.));
        const Real zlog2 = zlog * zlog;  // ZLOG**4 as (ZLOG**2)**2, the -O3 power tree
        const Real zbeta = const1 * zmu * (zlog2 * zlog2);
        t = t + w[j - 1] * zbeta / y * dely;
      }
    }
  }
  return TauhfSteps{delust, delalp};
}

}  // namespace ww::srce::st3
