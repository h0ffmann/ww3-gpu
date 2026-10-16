// kokkos/src/ww_kokkos/srce_db1.hpp
// W3SDB1, ported to Kokkos: depth-induced breaking after Battjes and Janssen
// (1978), one spectral point. Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3sdb1md.F90:96-382 at NOAA-EMC/WW3 03adda91;
// authors J. H. Alves, H. L. Tolman, A. Roland. Only the IWB == 1 branch
// (:319-330): IWB is the literal 1 (:242), so the Thornton and Guza branch
// (:331-341) is dead code in WW3 too and is not ported. W3_T, W3_S and
// W3_DEBUGRUN output is not ported: a kernel never prints.
//
// REAL*8 locals are double here, REAL arguments float, and each conversion sits
// where the Fortran has it. Arithmetic checked against gfortran 16.2 -O3 GIMPLE of
// w3sdb1md.F90 (WW3's own flags, switch_lab_shrd): scalar exp, tanh and sqrt in
// double, (2B-1)**2 as t*t, MAX(DEPTH,0.) as a float MAX_EXPR, THR = TINY(1.0)
// compared as a double, 1.0-THR folded to 1.0f. The EB loop is scalar in ITH
// order; its IK loop is vectorised element by element (no bits change).
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"

namespace ww::srce {

/// THR = TINY(1.0), w3parall.F90:97: a REAL, FLT_MIN.
inline constexpr Real kThr = 0x1p-126f;
/// TINY(1.d0), DBL_MIN.
inline constexpr double kTinyD = 0x1p-1022;

/// W3SDB1(IX, A, DEPTH, EMEAN, FMEAN, WNMEAN, CG, LBREAK, S, D) for one point.
/// IX is not read under this switch (only by W3_DEBUGRUN) and is not passed.
/// DEPTH, EMEAN, FMEAN, WNMEAN are INOUT in the Fortran but never written, so
/// they are passed by value. sig = SIG(1:NK), dden = DDEN(1:NK), cg = CG(1:NK),
/// a/s/d = A/S/D(1:NSPEC), all 0-based. eb (NK, double) is caller-provided work
/// for the Fortran local EB (w3sdb1md.F90:224).
///
/// Quirk (:238-240): when EMEAN < TINY(1.d0) the routine returns before writing
/// s, d or lbreak. The caller passes W3SRCE's VSDB, VDDB and its LBREAK, which
/// then keep the previous sub-step's values.
KOKKOS_INLINE_FUNCTION void w3sdb1_point(const Params& p, const Real* sig, const Real* dden,
                                         const Real* a, Real depth, Real emean, Real fmean,
                                         Real wnmean, const Real* cg, bool& lbreak, Real* s,
                                         Real* d, double* eb) {
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  const double thr = static_cast<double>(kThr);  // THR in a REAL*8 comparison

  // :238-240. EMEAN is promoted to REAL*8 for the comparison.
  if (static_cast<double>(emean) < kTinyD) return;

  // :250-265. Under IWB == 1 only EB is live: -O3 drops ETOT and FMEAN2 (they feed
  // the dead IWB == 2 branch and the W3_DEBUGRUN print). Kept as the source has
  // them; the compiler drops them here too.
  double etot = 0.;
  double fmean2 = 0.;
  for (int ik = 1; ik <= nk; ++ik) {
    eb[ik - 1] = 0.;
    for (int ith = 1; ith <= nth; ++ith) {
      eb[ik - 1] = eb[ik - 1] + static_cast<double>(a[ith + (ik - 1) * nth - 1]);
    }
  }
  for (int ik = 1; ik <= nk; ++ik) {
    eb[ik - 1] = eb[ik - 1] * static_cast<double>(dden[ik - 1]) / static_cast<double>(cg[ik - 1]);
    etot = etot + eb[ik - 1];
  }
  for (int ik = 1; ik <= nk; ++ik) {
    fmean2 = fmean2 + eb[ik - 1] * static_cast<double>(sig[ik - 1]);
  }
  fmean2 = fmean2 / etot * static_cast<double>(kTpiInv);
  (void)fmean2;

  // :272-279. MAX(DEPTH,0.) is a REAL MAX before the conversion.
  double hm;
  if (p.fdonly != 0) {
    hm = static_cast<double>(p.sdbc2) * static_cast<double>(depth);
  } else {
    const Real dpos = (depth > static_cast<Real>(0.)) ? depth : static_cast<Real>(0.);  // MAX_EXPR
    const double wnm = static_cast<double>(wnmean);
    hm = static_cast<double>(p.sdbc2) / wnm * Kokkos::tanh(wnm * static_cast<double>(dpos));
  }

  // :284-291.
  double hrms = Kokkos::sqrt(8. * static_cast<double>(emean));
  double bb;
  double b;
  if (hm > thr) {
    bb = hrms * hrms / (hm * hm);
    b = Kokkos::sqrt(bb);
  } else {
    bb = 0.;
    b = 0.;
  }

  // :296-300. Q0 is left unset for B > 1 and not read on that path; zero here
  // only to have a defined value.
  double q0 = 0.;
  if (b <= 0.5) {
    q0 = 0.;
  } else if (b <= 1.) {
    const double t = 2. * b - 1.;  // (2B-1)**2 as t*t
    q0 = t * t;
  }

  // :304-314. QB-1. mixes the REAL literal 1. (exact) into REAL*8. 1.0-THR is
  // computed in REAL and is 1.0f.
  double qb;
  if (b <= 0.2) {
    qb = 0.;
  } else if (b < 1.) {
    const double arg = Kokkos::exp((q0 - 1.) / bb);
    qb = q0 - bb * (q0 - arg) / (bb - arg);
    for (int is = 1; is <= 3; ++is) {
      qb = Kokkos::exp((qb - 1.) / bb);
    }
  } else {
    qb = static_cast<double>(static_cast<Real>(1.0) - kThr);
  }

  // :318-330, IWB == 1. 2 * DBLE(SDBC1) is an INTEGER 2 promoted to REAL*8.
  // BB .LT. 1.0 compares with the REAL 1.0 promoted (exact).
  //
  // ⚠ B == 1. exactly: the -O3 build goes straight to CBJ = 0 (GIMPLE: the
  // `b < 1.0` test in the B <= 1 branch jumps to the CBJ = 0 fill), as if
  // B == 1 implied BB == 1. The source would take ABS(BB-QB) > THR when
  // BB = 1+2**-52 (SQRT rounds that to 1.) and give CBJ = 2*SDBC1*FMEAN*BB.
  // The port follows the build, the reference (AGENTS.md, "Bit for bit").
  double cbj = 0.;
  if (b != 1. && (bb > thr) && (Kokkos::fabs(bb - qb) > thr)) {
    if (bb < 1.0) {
      cbj = 2. * static_cast<double>(p.sdbc1) * qb * static_cast<double>(fmean) / bb;
    } else {
      cbj = 2. * static_cast<double>(p.sdbc1) * static_cast<double>(fmean) * bb;
    }
  } else {
    cbj = 0.;
  }
  // D = REAL(-CBJ,4): CBJ = 0 gives -0.0f, as in the Fortran.
  const Real dval = static_cast<Real>(-cbj);
  for (int is = 0; is < nspec; ++is) d[is] = dval;
  for (int is = 0; is < nspec; ++is) s[is] = d[is] * a[is];

  // :343-347.
  lbreak = (cbj > 0.);
}

}  // namespace ww::srce
