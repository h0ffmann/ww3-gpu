// kokkos/src/ww_kokkos/st4_spr4.hpp
// W3SPR4, ported to Kokkos: mean wave parameters for the ST4 source terms.
// Phase 1, translated, not regtested.
//
// WW3 heritage: WW3/model/src/w3src4md.F90:142-384 at WW3@bb08623a; authors
// F. Ardhuin, H. L. Tolman. Only the #else branch of W3_FLX5 (:357-364):
// ww_st4_init refuses W3_FLX5. W3_S and W3_T output is not ported.
//
// Arithmetic checked against the -O3 GIMPLE inventory of w3src4md.F90: sums in
// IK/ITH order (the vectorised loop of section 2 uses in-order reductions), MAX as
// MAX_EXPR, (USTAR/UNZ)**2 as t*t. -O3 computes the powers of :316 and :317
// with libmvec's _ZGVbN4vv_powf on groups of four, emulated by mvec_powf on the
// host; ⚠ on a device those two sites fall back to scalar powf.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_arrays.hpp"
#include "st4_params.hpp"
#include "st4_calc_ustar.hpp"
#include "st4_libmvec.hpp"

namespace ww::st4 {

/// Reals and ints of per-point work storage w3spr4_point needs (no allocation in a kernel).
/// Reals: EB(NK), EB2(NK) (w3src4md.F90:268-269), then the powers of :316 and :317,
/// computed before the sums (the libmvec sites, see section 2).
inline int spr4_work_reals(const Params& p) { return 4 * p.nk; }
inline int spr4_work_ints(const Params& p) { (void)p; return 0; }

namespace detail {
/// Fortran MAX of two REALs, as gfortran -O3 emits it (MAX_EXPR).
KOKKOS_INLINE_FUNCTION Real spr4_max(Real a, Real b) { return (a > b) ? a : b; }
}  // namespace detail

/// W3SPR4 for one point. Arrays are the caller's, 0-based: a = A(NTH,NK) as
/// A(NSPEC), cg = CG(1:NK), wn = WN(1:NK), llws = LLWS(1:NSPEC) as 0/1.
/// ustar, usdir are INOUT; the other references are OUT and arrive holding the
/// caller's values, so a path that leaves one unassigned keeps it.
KOKKOS_INLINE_FUNCTION void w3spr4_point(const Params& p, const Fields& f, const Real* a,
                                         const Real* cg, const Real* wn, Real u, Real udir,
                                         Real tauwx, Real tauwy, const int* llws, Real& ustar,
                                         Real& usdir, Real& emean, Real& fmean, Real& fmean1,
                                         Real& wnmean, Real& amax, Real& cd, Real& z0,
                                         Real& charn, Real& fmeanws, Real& dlwmean, Real* work,
                                         int* iwork) {
  (void)iwork;
  using detail::spr4_max;
  const int nk = p.nk;
  const int nth = p.nth;
  const Real eps = static_cast<Real>(1.E-7);
  Real* eb = work;        // EB(IK)  = eb[IK-1]
  Real* eb2 = work + nk;  // EB2(IK) = eb2[IK-1]
  Real* pwsig = work + 2 * nk;  // SIG(IK)**(2.*WWNMEANPTAIL), :316 and :319
  Real* pwwn = work + 3 * nk;   // WN(IK)**WWNMEANP, :317

  Real unz = spr4_max(static_cast<Real>(0.01), u);  // :280
  ustar = spr4_max(static_cast<Real>(0.0001), ustar);  // :278

  emean = 0;
  Real emeanws = 0;
  fmeanws = 0;
  fmean = 0;
  fmean1 = 0;
  wnmean = 0;
  amax = 0;
  dlwmean = 0;
  Real elcs = 0;
  Real elsn = 0;

  // 1.  Integral over directions and maximum (:293-305).
  // Quirk 11 (:300-301): ELCS and ELSN run over IK, then ITH, into one scalar each.
  for (int ik = 1; ik <= nk; ++ik) {
    eb[ik - 1] = 0;
    eb2[ik - 1] = 0;
    const Real sigfac = Kokkos::pow(f.sig(ik), p.ssdsc[11]) * f.dden(ik - 1) / cg[ik - 1];
    for (int ith = 1; ith <= nth; ++ith) {
      const int is = ith + (ik - 1) * nth;
      const Real aa = a[(ik - 1) * nth + ith - 1];
      eb[ik - 1] = eb[ik - 1] + aa;
      elcs = elcs + aa * f.ecos(is - 1) * sigfac;
      elsn = elsn + aa * f.esin(is - 1) * sigfac;
      if (llws[is - 1] != 0) eb2[ik - 1] = eb2[ik - 1] + aa;
      amax = spr4_max(amax, aa);  // MAX(AMAX, A): -O3 MAX_EXPR <A, AMAX>
    }
  }

  dlwmean = Kokkos::atan2(elsn, elcs);  // :307

  // 2.  Integrate over directions (:311-320).
  // The powers are computed before the sum loop (mvec_powf); the values
  // are the same, and the sums below keep the IK order (in-order reductions at -O3).
  const Real two_wtail = static_cast<Real>(2.) * p.wwnmeanptail;
  // Lane l is IK = l+1.
  // LIBMVEC SITE: -O3 uses _ZGVbN4vv_powf here (INVENTORY.md), emulated on the host
  mvec_powf(
      nk, [&](int l) { return f.sig(l + 1); }, [&](int) { return two_wtail; },
      [&](int l, Real v) { pwsig[l] = v; });
  // LIBMVEC SITE: -O3 uses _ZGVbN4vv_powf here (INVENTORY.md), emulated on the host
  mvec_powf(
      nk, [&](int l) { return wn[l]; }, [&](int) { return p.wwnmeanp; },
      [&](int l, Real v) { pwwn[l] = v; });
  for (int ik = 1; ik <= nk; ++ik) {
    eb[ik - 1] = eb[ik - 1] * f.dden(ik - 1) / cg[ik - 1];
    eb2[ik - 1] = eb2[ik - 1] * f.dden(ik - 1) / cg[ik - 1];
    emean = emean + eb[ik - 1];
    fmean = fmean + eb[ik - 1] / f.sig(ik);
    fmean1 = fmean1 + eb[ik - 1] * pwsig[ik - 1];
    wnmean = wnmean + eb[ik - 1] * pwwn[ik - 1];
    emeanws = emeanws + eb2[ik - 1];
    fmeanws = fmeanws + eb2[ik - 1] * pwsig[ik - 1];  // :319 reuses :316 (CSE)
  }

  // 3.  Add tail beyond discrete spectrum and get mean pars (:325-331).
  Real eband = eb[nk - 1] / f.dden(nk - 1);
  emean = emean + eband * p.fte;
  fmean = fmean + eband * p.ftf;
  fmean1 = fmean1 + eband * p.sstxftftail;
  wnmean = wnmean + eband * p.sstxftwn;
  eband = eb2[nk - 1] / f.dden(nk - 1);
  emeanws = emeanws + eband * p.fte;
  fmeanws = fmeanws + eband * p.sstxftftail;

  // 4.  Final processing (:336-350).
  fmean = kTpiInv * emean / spr4_max(eps, fmean);
  if (fmean1 < eps) {
    fmean1 = kTpiInv * f.sig(nk);
  } else {
    fmean1 = kTpiInv * Kokkos::pow(spr4_max(eps, fmean1) / spr4_max(eps, emean),
                                   static_cast<Real>(1) / (static_cast<Real>(2.) * p.wwnmeanptail));
  }
  wnmean = Kokkos::pow(spr4_max(eps, wnmean) / spr4_max(eps, emean),
                       static_cast<Real>(1) / p.wwnmeanp);
  if (fmeanws < eps || emeanws < eps) {
    fmeanws = kTpiInv * f.sig(nk);
  } else {
    fmeanws = kTpiInv * Kokkos::pow(spr4_max(eps, fmeanws) / spr4_max(eps, emeanws),
                                    static_cast<Real>(1) / (static_cast<Real>(2.) * p.wwnmeanptail));
  }

  // 5.  Cd and z0 (:355-364), the #else branch of W3_FLX5.
  const Real tauw = Kokkos::sqrt(tauwx * tauwx + tauwy * tauwy);  // TAUWX**2: x*x at -O3
  calc_ustar(p, f, u, tauw, ustar, z0, charn);
  unz = spr4_max(static_cast<Real>(0.01), u);
  const Real r = ustar / unz;
  cd = r * r;  // (USTAR/UNZ)**2: one division, squared, at -O3
  usdir = udir;
}

}  // namespace ww::st4
