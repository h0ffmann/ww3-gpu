// kokkos/src/ww_kokkos/st4_tabu_tauhf2.cpp
// TABU_TAUHF2, translated as host code: it runs once per grid at initialisation.
// Tabulates the high-frequency wave-supported stress against ustar, alpha
// (modified Charnock) and tail level: TAUHFT2, and overwrites TAUHFT.
//
// WW3 heritage: WW3/model/src/w3src4md.F90:1598-1831 at WW3@bb08623a, TABU_TAUHF2,
// F. Ardhuin (origination 15-May-2007, 3.10.SHOM; table read 24-Jan-2013, 4.08).
// Phase 1, translated, not regtested.
//
// Not ported: the cache file ST4TABUHF2.bin (unit 993, :1710-1725, :1796-1800,
// :1809-1821). This always takes the Fortran's NOFILE=.TRUE. path, i.e. what WW3
// does when the file is absent or does not match; it neither reads nor writes it.
// Arithmetic follows the -O3 GIMPLE of w3src4md (operand order of each product
// as the dump shows; ZLOG**4 as (z*z)*(z*z), gfortran's -O3 power tree).
#include <cmath>

#include "st4_tables_build.hpp"

namespace ww::st4 {

namespace {
// Fortran MAX/MIN of REAL: MAX_EXPR/MIN_EXPR in the dump; no NaN reaches them here.
inline Real fmax_(Real a, Real b) { return a > b ? a : b; }
inline Real fmin_(Real a, Real b) { return a < b ? a : b; }
}  // namespace

void tabu_tauhf2(Params& p, Real sigmax, HostArrays& a) {
  constexpr int kJtot = 250;  // JTOT :1692
  const Real g = kGrav;
  const Real ustarm = static_cast<Real>(5.);                  // :1727
  const Real alpham = static_cast<Real>(20.) * p.aalpha;      // :1728
  p.delust = ustarm / static_cast<Real>(kIustar);             // :1730
  p.delalp = alpham / static_cast<Real>(kIalpha);             // :1731
  p.deltail = alpham / static_cast<Real>(kIlevTail);          // :1732 quirk: ALPHAM, not LEVTAILM
  constexpr Real kKappa2 = kKappa * kKappa;                   // KAPPA**2 folded, 0.16000001
  const Real const1 = p.bbeta / kKappa2;                      // :1733
  const Real omegac = sigmax;                                 // :1734

  auto& tauhft = a.tauhft;
  auto& tauhft2 = a.tauhft2;
  // :1741 TAUHFT(0:IUSTAR,0:IALPHA)=0.
  for (int l = 0; l <= kIalpha; ++l)
    for (int k = 0; k <= kIustar; ++k) tauhft(k, l) = static_cast<Real>(0.);

  // :1743-1746 ALLOCATE(W(JTOT)); w[j-1] = W(J).
  Real w[kJtot];
  for (int j = 1; j < kJtot - 1; ++j) w[j] = static_cast<Real>(1.);
  w[0] = static_cast<Real>(0.5);
  w[kJtot - 1] = static_cast<Real>(0.5);
  const Real x0 = static_cast<Real>(0.05);  // :1747

  for (int k = 0; k <= kIustar; ++k) {
    const Real ust0 = fmax_(static_cast<Real>(k) * p.delust, static_cast<Real>(0.000001));  // :1750
    for (int l = 0; l <= kIalpha; ++l) {
      Real ust = ust0;  // :1752, not reset per I: each I starts from the previous I's final UST
      const Real zz0 = ((p.aalpha + static_cast<Real>(l) * p.delalp) * (ust0 * ust0)) / g;  // :1753
      const Real omegacc = fmax_((x0 * g) / ust, omegac);  // :1754
      const Real yc = std::sqrt(zz0 / g) * omegacc;        // :1755
      const Real dely = fmax_((static_cast<Real>(1.) - yc) / static_cast<Real>(kJtot), static_cast<Real>(0.));  // :1756
      for (int i = 0; i <= kIlevTail; ++i) {
        const Real levtail = static_cast<Real>(i) * p.deltail;  // :1761
        // :1762 TAUHFT(K,L) is reset for every I; its integrand uses UST0 only, so
        // the value left is the same for every I.
        tauhft(k, l) = static_cast<Real>(0.);
        tauhft2(k, l, i) = static_cast<Real>(0.);
        const Real tauw0 = ust0 * ust0;  // :1764
        Real tauw = tauw0;
        for (int j = 1; j <= kJtot; ++j) {
          const Real y = static_cast<Real>(j - 1) * dely + yc;  // :1767
          const Real omega = std::sqrt(g / zz0) * y;            // :1768
          const Real cm = g / omega;                            // :1770
          Real zx = ust0 / cm + p.zzalp;                        // :1772
          Real zarg = fmin_(kKappa / zx, static_cast<Real>(20.));
          Real zmu = fmin_(((zz0 * g) / (cm * cm)) * std::exp(zarg), static_cast<Real>(1.));
          Real zlog = fmin_(std::log(zmu), static_cast<Real>(0.));
          Real zl2 = zlog * zlog;
          Real zbeta = (const1 * zmu) * (zl2 * zl2);
          tauhft(k, l) = tauhft(k, l) + ((w[j - 1] * zbeta) / y) * dely;  // :1778
          zx = ust / cm + p.zzalp;                                        // :1779
          zarg = fmin_(kKappa / zx, static_cast<Real>(20.));
          zmu = fmin_(((zz0 * g) / (cm * cm)) * std::exp(zarg), static_cast<Real>(1.));
          zlog = fmin_(std::log(zmu), static_cast<Real>(0.));
          zl2 = zlog * zlog;
          zbeta = (const1 * zmu) * (zl2 * zl2);
          const Real r = ust / ust0;
          tauhft2(k, l, i) = tauhft2(k, l, i) + (((w[j - 1] * zbeta) * (r * r)) / y) * dely;  // :1785
          tauw = tauw - (((((w[j - 1] * (ust * ust)) * zbeta) * levtail) / y) * dely);      // :1786
          ust = std::sqrt(fmax_(tauw, static_cast<Real>(0.)));                               // :1787
        }
      }
    }
  }
}

}  // namespace ww::st4
