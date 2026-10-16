// kokkos/src/ww_kokkos/st4_tabu_tauhf.cpp
// TABU_TAUHF: tabulation of the high-frequency wave-supported stress
// TAUHFT(0:IUSTAR,0:IALPHA), translated as host code (runs once per grid).
// F. Ardhuin (origination 27-Feb-2004, Bidlot modification 14-Aug-2006).
// Phase 1, translated, not regtested.
//
// WW3 heritage: WW3/model/src/w3src4md.F90:1427-1596 at WW3@bb08623a.
#include <cmath>

#include "st4_tables_build.hpp"

namespace ww::st4 {


void tabu_tauhf(Params& p, Real sigmax, HostArrays& a) {
  constexpr int kJtot = 250;  // JTOT :1514
  const Real aalpha = p.aalpha;

  const Real ustarm = static_cast<Real>(5.);
  const Real alpham = static_cast<Real>(20.) * aalpha;
  p.delust = ustarm / static_cast<Real>(kIustar);
  p.delalp = alpham / static_cast<Real>(kIalpha);
  // KAPPA**2 folded to KAPPA*KAPPA in REAL.
  const Real const1 = p.bbeta / (kKappa * kKappa);
  const Real omegac = sigmax;
  const Real delust = p.delust;
  const Real delalp = p.delalp;

  for (int l = 0; l <= kIalpha; ++l)
    for (int k = 0; k <= kIustar; ++k) a.tauhft(k, l) = static_cast<Real>(0.);

  // ALLOCATE(W(JTOT)) :1538; w[j-1] = W(J).
  Real w[kJtot];
  for (int j = 1; j < kJtot - 1; ++j) w[j] = static_cast<Real>(1.);
  w[0] = static_cast<Real>(0.5);
  w[kJtot - 1] = static_cast<Real>(0.5);
  const Real x0 = static_cast<Real>(0.05);

  for (int l = 0; l <= kIalpha; ++l) {
    for (int k = 0; k <= kIustar; ++k) {
      const Real ust = max_r(static_cast<Real>(k) * delust, static_cast<Real>(0.000001));
      Real zz00 = ust * ust * aalpha / kGrav;
      if (p.zz0max != static_cast<Real>(0)) zz00 = min_r(zz00, p.zz0max);
      const Real zz0 = zz00 * (static_cast<Real>(1) + static_cast<Real>(l) * delalp / aalpha);
      const Real omegacc = max_r(omegac, x0 * kGrav / ust);
      const Real yc = omegacc * std::sqrt(zz0 / kGrav);
      const Real dely = max_r((static_cast<Real>(1.) - yc) / static_cast<Real>(kJtot), static_cast<Real>(0.));
      // Integrated in J order up to 0.05*g/UST (:1555-1570).
      for (int j = 1; j <= kJtot; ++j) {
        const Real y = yc + static_cast<Real>(j - 1) * dely;
        const Real omega = y * std::sqrt(kGrav / zz0);
        const Real cm = kGrav / omega;
        const Real zx = ust / cm + p.zzalp;
        const Real zarg = min_r(kKappa / zx, static_cast<Real>(20.));
        const Real zmu = min_r(kGrav * zz0 / (cm * cm) * std::exp(zarg), static_cast<Real>(1.));
        const Real zlog = min_r(std::log(zmu), static_cast<Real>(0.));
        const Real zlog2 = zlog * zlog;  // ZLOG**4 as (ZLOG**2)**2, -O3 power tree
        const Real zbeta = const1 * zmu * (zlog2 * zlog2);
        a.tauhft(k, l) = a.tauhft(k, l) + w[j - 1] * zbeta / y * dely;
      }
    }
  }
}

}  // namespace ww::st4
