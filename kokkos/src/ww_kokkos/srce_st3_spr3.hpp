// kokkos/src/ww_kokkos/srce_st3_spr3.hpp
// W3SPR3: mean wave parameters, drag coefficient and roughness for the WAM4+ (ST3)
// source terms, one spectral point. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src3md.F90:134-347 at NOAA-EMC/WW3 03adda91;
// authors F. Ardhuin, H. L. Tolman. Evidence: kokkos/port_report/W3SPR3_KOKKOS.md.
// ⚠ On a device there is no libmvec (section 2) and powf, sqrtf are CUDA's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"
#include "srce_st3_calc_ustar.hpp"
#include "st4_libmvec.hpp"

namespace ww::srce::st3 {

namespace spr3_detail {
// maxss of w3src3md.o, d the destination: a NaN or a tie gives s. At every site of
// W3SPR3 the variable is d and the constant (or A) is s.
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }

// Folded by gfortran (.rodata.cst4 of w3src3md.o).
inline constexpr Real kTpiInv = 0x1.45f306p-3f;  // TPIINV constants.F90:75, 0x3e22f983
inline constexpr Real kEps = 0x1.ad7f2ap-24f;    // 1.E-7 :299-317, 0x33d6bf95
inline constexpr Real kUsMin = 0x1.a36e2ep-14f;  // 0.0001 :247, 0x38d1b717
inline constexpr Real kUMin = 0x1.47ae14p-7f;    // 0.01 :326, 0x3c23d70a
static_assert(kTpiInv == ww::srce::kTpiInv);
static_assert(kEps == 1.E-7f);
static_assert(kUsMin == 0.0001f);
static_assert(kUMin == 0.01f);
}  // namespace spr3_detail

/// Reals of per-point work storage w3spr3_point needs: EB(NK), EB2(NK) (:238), and
/// the three powers of section 2, SIG**(2*WWNMEANPTAIL), SIG**(2*WWNMEANP), WN**WWNMEANP.
inline int spr3_work_reals(const Params& p) { return 5 * p.nk; }

/// W3SPR3 for one point, arrays 0-based: a = A(NTH,NK) as A(NSPEC), cg = CG(1:NK),
/// wn = WN(1:NK), llws = LLWS(1:NSPEC) as LOGICAL(4) 0/1. ustar, usdir are INOUT;
/// the other references are OUT.
/// Module state: sig = SIG(1:NK) (not SIG(0)), dden = DDEN(1:NK); wwnmeanp,
/// wwnmeanptail, fte, sstxftf, sstxftwn, sstxftftail of W3GDATMD; taut, deltauw, delu,
/// zzwnd, aalpha as calc_ustar takes them. work: spr3_work_reals(p) reals.
/// ALFA (:273) is not computed: a local array the routine never reads.
KOKKOS_INLINE_FUNCTION void w3spr3_point(
    const Params& p, const Real* a, const Real* cg, const Real* wn, Real& emean, Real& fmean,
    Real& fmeans, Real& wnmean, Real& amax, Real u, Real udir, Real& ustar, Real& usdir,
    Real tauwx, Real tauwy, Real& cd, Real& z0, Real& charn, const int* llws, Real& fmeanws,
    const Real* sig, const Real* dden, Real wwnmeanp, Real wwnmeanptail, Real fte,
    Real sstxftf, Real sstxftwn, Real sstxftftail, const Real* taut, Real deltauw, Real delu,
    Real zzwnd, Real aalpha, Real* work) {
  using namespace spr3_detail;
  const int nk = p.nk;
  const int nth = p.nth;
  const Real zero = static_cast<Real>(0.);
  Real* eb = work;               // EB(IK)  = eb[IK-1]
  Real* eb2 = work + nk;         // EB2(IK) = eb2[IK-1]
  Real* pwsigt = work + 2 * nk;  // SIG(IK)**(2.*WWNMEANPTAIL), :277 and :281
  Real* pwsigp = work + 3 * nk;  // SIG(IK)**(2.*WWNMEANP), :278
  Real* pwwn = work + 4 * nk;    // WN(IK)**WWNMEANP, :279

  // :246 UNZ is dead; :326 sets it again.
  ustar = maxss(ustar, kUsMin);  // :247

  // :249-255.
  emean = zero;
  Real emeanws = zero;
  fmeanws = zero;
  fmean = zero;
  fmeans = zero;
  wnmean = zero;
  amax = zero;

  // 1. :259-268, scalar in the object, sums in ITH order.
  for (int ik = 1; ik <= nk; ++ik) {
    eb[ik - 1] = zero;
    eb2[ik - 1] = zero;
    for (int ith = 1; ith <= nth; ++ith) {
      const int is = ith + (ik - 1) * nth;
      const Real aa = a[is - 1];
      eb[ik - 1] = eb[ik - 1] + aa;
      if (llws[is - 1] != 0) eb2[ik - 1] = eb2[ik - 1] + aa;
      amax = maxss(amax, aa);
    }
  }

  // 2. :272-282. -O3 vectorises this loop by four from IK = 1 with _ZGVbN4vv_powf
  // and calls scalar powf for the last NK mod 4 (all scalar when NK < 4); the sums
  // are added lane by lane in IK order. :281 reuses the power of :277.
  const Real two_wtail = static_cast<Real>(2.) * wwnmeanptail;
  const Real two_wp = static_cast<Real>(2.) * wwnmeanp;
  ww::st4::mvec_powf(
      nk, [&](int l) { return sig[l]; }, [&](int) { return two_wtail; },
      [&](int l, Real v) { pwsigt[l] = v; });
  ww::st4::mvec_powf(
      nk, [&](int l) { return sig[l]; }, [&](int) { return two_wp; },
      [&](int l, Real v) { pwsigp[l] = v; });
  ww::st4::mvec_powf(
      nk, [&](int l) { return wn[l]; }, [&](int) { return wwnmeanp; },
      [&](int l, Real v) { pwwn[l] = v; });
  Real es = zero, fs = zero, fss = zero, ws = zero, fws = zero;
  for (int ik = 1; ik <= nk; ++ik) {
    const Real e = eb[ik - 1] * dden[ik - 1] / cg[ik - 1];
    const Real e2 = eb2[ik - 1] * dden[ik - 1] / cg[ik - 1];
    eb[ik - 1] = e;
    eb2[ik - 1] = e2;
    es = es + e;
    fs = fs + e * pwsigt[ik - 1];
    fss = fss + e * pwsigp[ik - 1];
    ws = ws + e * pwwn[ik - 1];
    emeanws = emeanws + e2;
    fws = fws + e2 * pwsigt[ik - 1];
  }

  // 3. :287-294.
  Real eband = eb[nk - 1] / dden[nk - 1];
  es = es + eband * fte;
  fs = fs + eband * sstxftftail;
  fss = fss + eband * sstxftf;
  ws = ws + eband * sstxftwn;
  eband = eb2[nk - 1] / dden[nk - 1];
  emeanws = emeanws + eband * fte;
  fws = fws + eband * sstxftftail;
  emean = es;

  // 4. :299-318. In each ELSE branch -O3 drops MAX(1.E-7, x) of the variable the IF
  // just tested (x / MAX(1.E-7, EMEAN) at :303, :309; x / EMEANWS at :317): a NaN
  // reaches powf.
  const Real emax = maxss(es, kEps);
  if (fs < kEps) {
    fmean = kTpiInv * sig[nk - 1];
  } else {
    fmean = kTpiInv * Kokkos::pow(fs / emax, static_cast<Real>(1.) / two_wtail);
  }
  if (fss < kEps) {
    fmeans = kTpiInv * sig[nk - 1];
  } else {
    fmeans = kTpiInv * Kokkos::pow(fss / emax, static_cast<Real>(1.) / two_wp);
  }
  wnmean = Kokkos::pow(maxss(ws, kEps) / emax, static_cast<Real>(1.) / wwnmeanp);
  if (fws < kEps || emeanws < kEps) {
    fmeanws = kTpiInv * sig[nk - 1];
  } else {
    fmeanws = kTpiInv * Kokkos::pow(fws / emeanws, static_cast<Real>(1.) / two_wtail);
  }

  // 5. :322-328. CALC_USTAR is called out of line in the object; Z0 = 0. (:324) is
  // dead, CALC_USTAR writes Z0 on every path.
  const Real tauw = Kokkos::sqrt(tauwx * tauwx + tauwy * tauwy);
  calc_ustar(u, tauw, ustar, z0, charn, taut, deltauw, delu, zzwnd, aalpha);
  const Real unz = maxss(u, kUMin);
  const Real r = ustar / unz;
  cd = r * r;  // **2
  usdir = udir;
}

}  // namespace ww::srce::st3
