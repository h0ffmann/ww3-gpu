// kokkos/src/ww_kokkos/st4_calc_ustar.hpp
// CALC_USTAR, ported to Kokkos (phase 1, translated, not regtested).
//
// WW3 heritage: WW3/model/src/w3src4md.F90:1849-2011 at WW3@bb08623a. Authors of
// the Fortran: F. Ardhuin (origination 2004, Bidlot modification 2006, Charnock
// output 2010); UK Met Office (CAPCHNK functional form, version 7.15, 2024).
//
// Arithmetic follows gfortran -O3's optimized GIMPLE of calc_ustar: constants
// folded at REAL precision (SQRT(CDRAG), 0.11*NU_AIR, XM+1), (1-X)**XM kept as
// powf(_, 0.5), USTOLD**2 and UST**2. as x*x. The W3_T/W3_S branches are not
// ported: a kernel never prints.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_arrays.hpp"
#include "st4_params.hpp"

namespace ww::st4 {

namespace calc_ustar_detail {
// Fortran MIN/MAX of two REALs (MIN_EXPR/MAX_EXPR in the dump; operands are
// never NaN here, so the order of the operands does not change the bits).
KOKKOS_INLINE_FUNCTION Real fmin2(Real a, Real b) { return (b < a) ? b : a; }
KOKKOS_INLINE_FUNCTION Real fmax2(Real a, Real b) { return (b > a) ? b : a; }
KOKKOS_INLINE_FUNCTION int imin2(int a, int b) { return (b < a) ? b : a; }
}  // namespace calc_ustar_detail

/// CALC_USTAR(WINDSPEED, TAUW, USTAR, Z0, CHARN): friction velocity, roughness
/// and Charnock coefficient from the 10 m wind and the wave-supported stress.
/// Called by w3spr4_point (w3src4md.F90:361).
KOKKOS_INLINE_FUNCTION void calc_ustar(const Params& p, const Fields& f, Real windspeed,
                                       Real tauw, Real& ustar, Real& z0, Real& charn) {
  using calc_ustar_detail::fmax2;
  using calc_ustar_detail::fmin2;
  using calc_ustar_detail::imin2;
  constexpr int kNiter = 10;                         // NITER :1925
  constexpr Real kXm = static_cast<Real>(0.50);      // XM    :1926
  constexpr Real kEps1 = static_cast<Real>(0.00001); // EPS1  :1926

  Real chath = p.aalpha;  // :1935
  Real ust = 0;

  if (p.sintailpar[0] > static_cast<Real>(0.5)) {  // :1937, table of TABU_STRESS
    const Real tauw_local = fmax2(fmin2(tauw, kTauwMax), static_cast<Real>(0.));  // :1938
    const Real xi = Kokkos::sqrt(tauw_local) / p.deltauw;
    const int ind = imin2(kItauMax - 1, fint(xi));  // INT truncates
    const Real deli1 = fmin2(static_cast<Real>(1.), xi - static_cast<Real>(ind));
    const Real deli2 = static_cast<Real>(1.) - deli1;
    const Real xj = windspeed / p.delu;
    const int j = imin2(kJuMax - 1, fint(xj));
    const Real delj1 = fmin2(static_cast<Real>(1.), xj - static_cast<Real>(j));
    const Real delj2 = static_cast<Real>(1.) - delj1;
    // :1947-1948; TAUT(0:ITAUMAX,0:JUMAX) is 0-based, so (IND,J) is f.taut(ind,j).
    ustar = (f.taut(ind, j) * deli2 + f.taut(ind + 1, j) * deli1) * delj2 +
            (f.taut(ind, j + 1) * deli2 + f.taut(ind + 1, j + 1) * deli1) * delj1;
  } else {
    Real xmin;
    if (p.capchnk[0] == static_cast<Real>(1.)) {  // :1950, Donelan (2018)
      chath = p.capchnk[1] + static_cast<Real>(0.5) * (p.capchnk[2] - p.capchnk[1]) *
                                 (static_cast<Real>(1) -
                                  Kokkos::tanh((windspeed - p.capchnk[3]) / p.capchnk[4]));
      xmin = static_cast<Real>(0.15) * (p.capchnk[2] - chath);
    } else {
      xmin = static_cast<Real>(0.);
    }

    // :1962-1963: the MAX/MIN on TAUW is commented out in the Fortran.
    const Real tauw_local = tauw;
    const Real cdrag = static_cast<Real>(0.0012875);
    const Real wcd = Kokkos::sqrt(cdrag);  // folded to 3.5881750e-2 at -O3, same bits
    const Real ustold = windspeed * wcd;
    Real tauold = fmax2(ustold * ustold, tauw_local + kEps1);  // USTOLD**2
    // Newton method for ustar in U=ustar*log(Z/Z0); a fixed NITER, no convergence test (:1969).
    constexpr Real kZnuNum = static_cast<Real>(0.11) * kNuAir;  // 0.11*nu_air, folded
    constexpr Real kXm1 = kXm + static_cast<Real>(1);            // XM+1, folded to 1.5
    for (int iter = 1; iter <= kNiter; ++iter) {
      const Real x = fmax2(tauw_local / tauold, xmin);
      ust = Kokkos::sqrt(tauold);
      Real zz00 = chath * tauold / kGrav;
      if (p.zz0max != static_cast<Real>(0)) zz00 = fmin2(zz00, p.zz0max);  // :1973
      // Corrects roughness ZZ00 for quasi-linear effect; REAL exponent -> powf.
      const Real omx = static_cast<Real>(1.) - x;
      Real zz0 = zz00 / Kokkos::pow(omx, kXm);
      const Real znu = kZnuNum / fmax2(ust, static_cast<Real>(1E-6));
      zz0 = p.sintailpar[4] * znu + zz0;
      const Real lg = Kokkos::log(p.zzwnd / zz0);  // ALOG(ZZWND/ZZ0), computed once at -O3
      const Real fval = ust - kKappa * windspeed / lg;
      // :1979-1980, left to right: ((((K*W)/lg**2)*2.)/UST)*(1-(XM+1)*X))/(1-X)
      const Real delf = static_cast<Real>(1.) -
                        kKappa * windspeed / (lg * lg) * static_cast<Real>(2.) / ust *
                            (static_cast<Real>(1.) - kXm1 * x) / omx;
      ust = ust - fval / delf;
      tauold = fmax2(ust * ust, tauw_local + kEps1);  // UST**2. -> UST*UST
    }
    ustar = ust;
  }

  // Determines roughness length (:1989-2001).
  if (ustar > static_cast<Real>(0.001)) {
    const Real sqrtcdm1 = fmin2(windspeed / ustar, static_cast<Real>(100.0));
    z0 = p.zzwnd * Kokkos::exp(-kKappa * sqrtcdm1);
    charn = kGrav * z0 / (ustar * ustar);  // USTAR**2
  } else {
    if (ustar > static_cast<Real>(0)) {
      const Real sqrtcdm1 = fmin2(windspeed / ustar, static_cast<Real>(100.0));
      z0 = p.zzwnd * Kokkos::exp(-kKappa * sqrtcdm1);
    } else {
      z0 = chath * static_cast<Real>(0.001) * static_cast<Real>(0.001) / kGrav;
    }
    charn = chath;
  }
  if (p.capchnk[0] == static_cast<Real>(1)) {  // :2002
    // Problem with large values of CHARN for low winds
    charn = fmin2(static_cast<Real>(0.09), charn);
    if (charn < chath) charn = chath;
  }
}

}  // namespace ww::st4
