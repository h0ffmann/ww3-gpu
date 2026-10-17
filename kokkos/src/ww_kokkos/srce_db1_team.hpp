// kokkos/src/ww_kokkos/srce_db1_team.hpp
// W3SDB1 split over the threads of a team, for the team-parallel W3SRCE (srce_kernel_team.hpp,
// w3srce_split). The arithmetic of every element, sum, MIN/MAX and running value is that of
// w3sdb1_point (srce_db1.hpp), which stays the code of every host run; this file only says which
// thread does which element, and where the barriers are. Heritage, quirks and operand orders:
// srce_db1.hpp.
//
// SPDX-License-Identifier: MIT
#pragma once

#include <Kokkos_Core.hpp>

#include "srce_db1.hpp"
#include "srce_team.hpp"

namespace ww::srce {

/// W3SDB1 for one point by the whole team; ex is the team's executor
/// (srce_team.hpp, TeamExec): EB(IK) is split over the team
/// one IK per iteration (its sum over ITH in order); ETOT and FMEAN2 are sums
#include "libm_device.hpp"
/// over IK, on one thread; CBJ reads only scalar arguments, and every thread
/// computes it; D and S = D*A are split element by element. LBREAK is a
/// reference each thread may own (W3SRCE does not read it). A team caller puts
/// a barrier after.
template <class X>
KOKKOS_INLINE_FUNCTION void w3sdb1_team(const X& ex, const Params& p, const Real* sig,
                                        const Real* dden, const Real* a, Real depth, Real emean,
                                        Real fmean, Real wnmean, const Real* cg, bool& lbreak,
                                        Real* s, Real* d, double* eb) {
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  const double thr = static_cast<double>(kThr);  // THR in a REAL*8 comparison

  // :238-240. EMEAN is promoted to REAL*8 for the comparison.
  if (static_cast<double>(emean) < kTinyD) return;

  // :250-265. Under IWB == 1 only EB is live: -O3 drops ETOT and FMEAN2 (they feed
  // the dead IWB == 2 branch and the W3_DEBUGRUN print). Kept as the source has
  // them; the compiler drops them here too.
  ex.for_(nk, [&](int ik0) {
    const int ik = ik0 + 1;
    eb[ik - 1] = 0.;
    for (int ith = 1; ith <= nth; ++ith) {
      eb[ik - 1] = eb[ik - 1] + static_cast<double>(a[ith + (ik - 1) * nth - 1]);
    }
  });
  ex.sync();
  ex.one([&]() {
    double etot = 0.;
    double fmean2 = 0.;
    for (int ik = 1; ik <= nk; ++ik) {
      eb[ik - 1] = eb[ik - 1] * static_cast<double>(dden[ik - 1]) / static_cast<double>(cg[ik - 1]);
      etot = etot + eb[ik - 1];
    }
    for (int ik = 1; ik <= nk; ++ik) {
      fmean2 = fmean2 + eb[ik - 1] * static_cast<double>(sig[ik - 1]);
    }
    fmean2 = fmean2 / etot * static_cast<double>(kTpiInv);
    (void)fmean2;
  });

  // :272-279. MAX(DEPTH,0.) is a REAL MAX before the conversion.
  double hm;
  if (p.fdonly != 0) {
    hm = static_cast<double>(p.sdbc2) * static_cast<double>(depth);
  } else {
    const Real dpos = (depth > static_cast<Real>(0.)) ? depth : static_cast<Real>(0.);  // MAX_EXPR
    const double wnm = static_cast<double>(wnmean);
    hm = static_cast<double>(p.sdbc2) / wnm * dm::tanh(wnm * static_cast<double>(dpos));
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
    const double arg = dm::exp((q0 - 1.) / bb);
    qb = q0 - bb * (q0 - arg) / (bb - arg);
    for (int is = 1; is <= 3; ++is) {
      qb = dm::exp((qb - 1.) / bb);
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
  ex.for_(nspec, [&](int is) {
    d[is] = dval;
    s[is] = d[is] * a[is];
  });

  // :343-347.
  lbreak = (cbj > 0.);
}


}  // namespace ww::srce
