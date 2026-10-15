// kokkos/src/ww_kokkos/st4_tabu_stress.cpp
// TABU_STRESS: the friction velocity table TAUT(TAUW,U10) = SQRT(TAU), host code
// run once per grid at initialisation.
//
// WW3 heritage: WW3/model/src/w3src4md.F90:1290-1425 at WW3@bb08623a.
// Authors: F. Ardhuin (origination 23-Jun-2006), adapted from WAM (P.A.E.M.
// Janssen, KNMI 1990) and its STRESS subroutine (J. Bidlot, ECMWF 2004).
// Phase 1, translated, not regtested.
#include <algorithm>
#include <cmath>

#include "st4_tables_build.hpp"

namespace ww::st4 {

void tabu_stress(Params& p, HostArrays& a) {
  constexpr int kNiter = 10;                           // NITER :1360
  constexpr Real kXm = static_cast<Real>(0.50);        // XM    :1361
  constexpr Real kEps1 = static_cast<Real>(0.00001);   // EPS1  :1361
  const Real one = static_cast<Real>(1.);

  p.delu = kUmax / static_cast<Real>(kJuMax);          // :1374
  p.deltauw = kTauwMax / static_cast<Real>(kItauMax);  // :1375
  const Real delu = p.delu;
  const Real deltauw = p.deltauw;

  for (int i = 0; i <= kItauMax; ++i) {
    const Real ztauw = sqr(static_cast<Real>(i) * deltauw);  // **2 :1377
    for (int j = 0; j <= kJuMax; ++j) {
      const Real utop = static_cast<Real>(j) * delu;
      const Real cdrag = static_cast<Real>(0.0012875);
      const Real wcd = std::sqrt(cdrag);
      const Real ustold = utop * wcd;
      // MAX(USTOLD**2, ZTAUW+EPS1) :1383
      Real tauold = std::max(sqr(ustold), ztauw + kEps1);
      for (int iter = 1; iter <= kNiter; ++iter) {
        const Real x = ztauw / tauold;
        Real ust = std::sqrt(tauold);
        Real zz00 = p.aalpha * tauold / kGrav;
        if (p.zz0max != static_cast<Real>(0)) zz00 = std::min(zz00, p.zz0max);
        // (1.-X)**XM with REAL XM=0.5: a pow call, not sqrt (:1390)
        const Real zz0 = zz00 / std::pow(one - x, kXm);
        // ALOG(ZZWND/ZZ0) appears twice; -O3 calls logf once (same value).
        const Real lg = std::log(p.zzwnd / zz0);
        const Real f = ust - kKappa * utop / lg;
        // Left to right as written :1394-1395; (XM+1) folds to 1.5.
        const Real delf = one - kKappa * utop / sqr(lg) * static_cast<Real>(2.) / ust *
                                    (one - (kXm + one) * x) / (one - x);
        ust = ust - f / delf;
        // UST**2. with a REAL exponent 2.0: folded to UST*UST (:1397)
        tauold = std::max(sqr(ust), ztauw + kEps1);
      }
      a.taut(i, j) = std::sqrt(tauold);
    }
  }
  // Force zero wind to have zero stress (Bidlot 1996) :1405-1409
  for (int i = 0; i <= kItauMax; ++i) {
    a.taut(i, 0) = static_cast<Real>(0.0);
  }
}

}  // namespace ww::st4
