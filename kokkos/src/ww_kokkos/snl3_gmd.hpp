// kokkos/src/ww_kokkos/snl3_gmd.hpp
// W3SNL3 with its internal EXPAND and EXPND2: the Generalized Multiple DIA (NL3) source term
// and its diagonal at one spectral point. Phase 1, translated, not regtested. WW3 heritage:
// WW3-noaa/model/src/w3snl3md.F90 at NOAA-EMC/WW3 03adda91 (W3SNL3 :181-777, EXPAND :644-703,
// EXPND2 :717-773); author H. L. Tolman. Evidence: kokkos/port_report/W3SNL3_KOKKOS.md.
// The non-PDLIB variant (WAVNU1 at :400): no switch with NL3 at the pin has PDLIB. The tables
// are INSNL3's (snl3_tables.hpp) for the current grid; the W3GDATMD scalars of Snl3Params and
// WAVNU1's DISTAB table are the grid's at call time. W3_S is not ported.
// ⚠ On a device logf, sqrtf, powf and the libmvec lanes of the SCALE loop are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "prop_wavnu1.hpp"
#include "real.hpp"
#include "snl3_tables.hpp"
#include "st4_libmvec.hpp"
#include "st4_params.hpp"

namespace ww::snl3 {

/// W3GDATMD scalars W3SNL3 reads besides the tables (w3snl3md.F90:332-336).
struct Snl3Params {
  Real fachfe;  ///< FACHFE, tail factor of EXPAND (:691)
  Real facti1, facti2;  ///< FACTI1, FACTI2, frequency index of a filter frequency (:450, :466)
  Real snlmsc, snlnsc;  ///< SNLMSC, SNLNSC, deep and shallow scaling powers (:410-417)
  Real snlsfd, snlsfs;  ///< SNLSFD, SNLSFS, nondimensional filter frequencies (:449, :465)
};

/// Caller-provided work arrays of one call (the Fortran's automatic arrays, :356-370, and
/// EXPND2's TEMP, :754), with t = Snl3Tables:
///   xwn, xcg: t.nfrmax; scale1, scale2, jkd: t.nfrcut;
///   ue: t.nspmax - t.nspmin + 1; dsb, dd1, dd2, dd3, dd4: t.nspmx2 - t.nspmin + 1;
///   temp: t.nth * t.nfrcut.
/// None needs initialising: every element read is written first in the same call.
struct Snl3Scratch {
  Real *xwn, *xcg, *scale1, *scale2;
  int* jkd;
  Real *ue, *dsb, *dd1, *dd2, *dd3, *dd4, *temp;
};

namespace snl3_detail {
inline constexpr Real kGrav = ww::st4::kGrav;
inline constexpr Real kTpi = ww::st4::kTpi;
inline constexpr Real kTpiInv = ww::st4::kTpiInv;
// TPI**11 and GRAV**2/TPI**11 as gfortran folds them (:410-411), from the object.
inline constexpr Real kTpi11 = 0x1.1f4f06p+29f;
inline constexpr Real kAux2 = 0x1.56b76ap-23f;
static_assert(kTpi == 0x1.921fb6p+2f && kTpiInv == 0x1.45f306p-3f);
static_assert(kAux2 == (kGrav * kGrav) / kTpi11);

/// EXPND2 (:717-773) on an array declared (NSPMIN:NSPMX2), called as EXPND2(X(1:NTH*NFRCUT), X)
/// (:548-552): the input is the compact block at Fortran index 1, the output the whole array,
/// so TEMP is copied first. arr[j - nspmin] is X(j).
KOKKOS_INLINE_FUNCTION void expnd2(const Snl3Tables& t, Real* arr, Real* temp) {
  const int nth = t.nth, nthexp = t.nthexp, nspmin = t.nspmin;
  const int n = nth * t.nfrcut;
  for (int k = 0; k < n; ++k) temp[k] = arr[k + 1 - nspmin];  // :758
  // AROUT(ith,ifr) is X(ith + (ifr-1)*NTHEXP)
  for (int ifr = t.nfrmin; ifr <= 0; ++ifr)  // :760
    for (int ith = 1 - t.nthmax; ith <= nth + t.nthmax; ++ith)
      arr[ith + (ifr - 1) * nthexp - nspmin] = static_cast<Real>(0.);
  for (int ifr = 1; ifr <= t.nfrcut; ++ifr)  // :762
    for (int ith = 1; ith <= nth; ++ith)
      arr[ith + (ifr - 1) * nthexp - nspmin] = temp[(ifr - 1) * nth + ith - 1];
  // :764-767 in this order: with NTHMAX > NTH a row reads one written earlier in the loop.
  for (int ith = 1; ith <= t.nthmax; ++ith) {
    for (int ifr = 1; ifr <= t.nfrcut; ++ifr)
      arr[nth + ith + (ifr - 1) * nthexp - nspmin] = arr[ith + (ifr - 1) * nthexp - nspmin];
    for (int ifr = 1; ifr <= t.nfrcut; ++ifr)
      arr[1 - ith + (ifr - 1) * nthexp - nspmin] = arr[nth + 1 - ith + (ifr - 1) * nthexp - nspmin];
  }
}
}  // namespace snl3_detail

/// W3SNL3(A, CG, WN, DEPTH, S, D) for one point. a, s, d are (NTH,NK) column-major,
/// a[(ik-1)*nth + ith-1]; cg, wn are (NK). s and d are overwritten.
KOKKOS_INLINE_FUNCTION void w3snl3_point(const Snl3Tables& t, const Snl3Params& p,
                                         const ww::prop::DispTable& disp, const Real* a,
                                         const Real* cg, const Real* wn, Real depth, Real* s,
                                         Real* d, const Snl3Scratch& w) {
  using namespace snl3_detail;
  using ww::st4::nint;
  const Real zero = static_cast<Real>(0.);
  const int nfr = t.nfr, nth = t.nth, nthexp = t.nthexp, nspmin = t.nspmin;
  const int nfrcut = t.nfrcut, nfrmax = t.nfrmax, nthmax = t.nthmax;
  Real* const ue = w.ue;

  // 1.a :381-402
  const Real xsitln = Kokkos::log(t.xsit);
  for (int k = 0; k < nth * nfr; ++k) s[k] = zero;
  for (int k = 0; k < nth * nfr; ++k) d[k] = zero;
  for (int ifr = 1; ifr <= nfr; ++ifr) {
    w.xwn[ifr - 1] = wn[ifr - 1];
    w.xcg[ifr - 1] = cg[ifr - 1];
  }
  for (int ifr = nfr + 1; ifr <= nfrmax; ++ifr)
    ww::prop::wavnu1(disp, t.xsi(ifr - 1), depth, w.xwn[ifr - 1], w.xcg[ifr - 1]);

  // 1.b EXPAND (:682-697); SPEC(ith,ifr) is UE(ith + (ifr-1)*NTHEXP)
  auto ux = [&](int ith, int ifr) -> Real& { return ue[ith + (ifr - 1) * nthexp - nspmin]; };
  for (int ifr = t.nfrmin; ifr <= 0; ++ifr)
    for (int ith = 1 - nthmax; ith <= nth + nthmax; ++ith) ux(ith, ifr) = zero;
  for (int ifr = 1; ifr <= nfr; ++ifr)
    for (int ith = 1; ith <= nth; ++ith) ux(ith, ifr) = a[(ifr - 1) * nth + ith - 1] * kTpi;
  for (int ifr = 1; ifr <= nfr; ++ifr)
    for (int ith = 1; ith <= nth; ++ith)
      ux(ith, ifr) = ux(ith, ifr) * t.xsi(ifr - 1) / w.xcg[ifr - 1];
  for (int ifr = nfr + 1; ifr <= nfrmax; ++ifr)
    for (int ith = 1; ith <= nth; ++ith) ux(ith, ifr) = ux(ith, ifr - 1) * p.fachfe;
  for (int ith = 1; ith <= nthmax; ++ith) {
    for (int ifr = 1; ifr <= nfrmax; ++ifr) ux(nth + ith, ifr) = ux(ith, ifr);
    for (int ifr = 1; ifr <= nfrmax; ++ifr) ux(1 - ith, ifr) = ux(nth + 1 - ith, ifr);
  }

  // 1.c :410-418. gfortran vectorises the loop with libmvec: three powf per group of four
  // from IFR = 1 when NFRCUT >= 4, scalar powf for the rest; elementwise otherwise.
  {
    const Real aux1 = static_cast<Real>(1.) /
                      (Kokkos::pow(kGrav, static_cast<Real>(4.) - p.snlmsc) * kTpi11);
    const Real e1 = p.snlmsc + static_cast<Real>(4.);
    const Real e2 = static_cast<Real>(13.) - p.snlmsc * static_cast<Real>(2.);
    const Real e3 = p.snlnsc;
    Real* const sc1 = w.scale1;
    Real* const sc2 = w.scale2;
    const Real* const xwn = w.xwn;
    const Real* const xcg = w.xcg;
    const auto& xsi = t.xsi;
    ww::st4::mvec_powf(
        nfrcut, [&](int i) { return xwn[i]; }, [&](int) { return e1; },
        [&](int i, Real v) { sc1[i] = aux1 * v; });
    ww::st4::mvec_powf(
        nfrcut, [&](int i) { return xsi(i); }, [&](int) { return e2; },
        [&](int i, Real v) { sc1[i] = v * sc1[i] / (xcg[i] * xcg[i]); });
    ww::st4::mvec_powf(
        nfrcut, [&](int i) { return depth * xwn[i]; }, [&](int) { return e3; },
        [&](int i, Real v) {
          const Real x = xwn[i];
          const Real x2 = x * x;
          const Real x3 = x2 * x;
          const Real x11 = (x3 * x3) * (x3 * x2);  // gfortran's power tree for **11
          sc2[i] = v * (x11 * kAux2) / xcg[i];
        });
  }

  // 1.d :422-426
  {
    const Real sq = Kokkos::sqrt(depth / kGrav);
    const Real lsitmin = Kokkos::log(t.sitmin);
    for (int ifr = 1; ifr <= nfrcut; ++ifr) {
      const Real sit = t.xsi(ifr - 1) * sq;
      const int ikd = 1 + nint((Kokkos::log(sit) - lsitmin) / xsitln);
      // W3SNL3MD's NKD = 275 even when the tables have 1300 levels (snl3_tables.hpp)
      w.jkd[ifr - 1] = ww::st4::max_i(ww::st4::min_i(ikd, kNkdSnl3), 1);
    }
  }

  // 2. :430-621. IF2MIN, IF2MAX are those of the last IQA in section 5; with NQA = 0 the
  // Fortran reads them uninitialised, here the loop of section 5 is empty.
  int if2min = 1, if2max = 0;
  Real* const dsb = w.dsb;
  Real* const dd1 = w.dd1;
  Real* const dd2 = w.dd2;
  Real* const dd3 = w.dd3;
  Real* const dd4 = w.dd4;
  for (int iqa = 1; iqa <= t.nqa; ++iqa) {
    // 3.a :435-479
    {
      const Real aux1 = t.qst3(4, iqa - 1, 0);
      const Real aux2 = t.qst3(5, iqa - 1, 0);
      int if1min = 1;
      int if1max = nfrcut;
      if2min = 1;
      if2max = nfr;
      if (aux1 <= zero && aux2 <= zero) {
        continue;
      } else if (aux2 <= zero) {
        const Real sit = p.snlsfd * Kokkos::sqrt(kGrav / depth);
        const int ifr = nint(p.facti2 + p.facti1 * Kokkos::log(sit));
        if (ifr > nfr) continue;
        if (ifr > 1) {
          if1min = ww::st4::max_i(1, ifr);
          if2min = ww::st4::max_i(1, if1min + t.nfrmin);
          for (int j = 1; j <= (if1min - 1) * nth; ++j) {
            dsb[j - nspmin] = zero;
            dd1[j - nspmin] = zero;
            dd2[j - nspmin] = zero;
            dd3[j - nspmin] = zero;
            dd4[j - nspmin] = zero;
          }
        }
      } else if (aux1 <= zero) {
        const Real sit = p.snlsfs * Kokkos::sqrt(kGrav / depth);
        const int ifr = nint(p.facti2 + p.facti1 * Kokkos::log(sit));
        if (ifr < 1) continue;
        if (ifr < nfrcut) {
          if1max = ww::st4::min_i(nfrcut, ifr);
          for (int j = if1max * nth + 1; j <= nfrcut * nth; ++j) {
            dsb[j - nspmin] = zero;
            dd1[j - nspmin] = zero;
            dd2[j - nspmin] = zero;
            dd3[j - nspmin] = zero;
            dd4[j - nspmin] = zero;
          }
        }
      }

      // 3.b-3.e :483-544, DSB..DD4 in the compact layout ISP = (IFR-1)*NTH + ITH
      for (int ifr = if1min; ifr <= if1max; ++ifr) {
        const int ikd = w.jkd[ifr - 1];
        int lq1[16];
        Real lq2[16], fact[6];
        for (int k = 0; k < 16; ++k) lq1[k] = t.qst1(k, iqa - 1, ikd - 1);
        for (int k = 0; k < 16; ++k) lq2[k] = t.qst2(k, iqa - 1, ikd - 1);
        for (int k = 0; k < 6; ++k) fact[k] = t.qst3(k, iqa - 1, ikd - 1);
        const Real xcg = w.xcg[ifr - 1];
        const Real den = w.xwn[ifr - 1] * t.xsi(ifr - 1);
        for (int k = 0; k < 4; ++k) fact[k] = fact[k] * xcg / den;
        const Real fprop = w.scale1[ifr - 1] * fact[4] + w.scale2[ifr - 1] * fact[5];
        const int isp0 = (ifr - 1) * nth;
        const int ispx0 = (ifr - 1) * nthexp;
        for (int ith = 1; ith <= nth; ++ith) {
          const int isp = isp0 + ith - nspmin;
          const Real* const u = ue + (ispx0 + ith - nspmin);
          const Real fq1 = (u[lq1[0]] * lq2[0] + u[lq1[1]] * lq2[1] + u[lq1[2]] * lq2[2] +
                            u[lq1[3]] * lq2[3]) * fact[0];
          const Real fq2 = (u[lq1[4]] * lq2[4] + u[lq1[5]] * lq2[5] + u[lq1[6]] * lq2[6] +
                            u[lq1[7]] * lq2[7]) * fact[1];
          const Real fq3 = (u[lq1[8]] * lq2[8] + u[lq1[9]] * lq2[9] + u[lq1[10]] * lq2[10] +
                            u[lq1[11]] * lq2[11]) * fact[2];
          const Real fq4 = (u[lq1[12]] * lq2[12] + u[lq1[13]] * lq2[13] + u[lq1[14]] * lq2[14] +
                            u[lq1[15]] * lq2[15]) * fact[3];
          Real x1 = fq1 * fq2 * (fq3 + fq4);
          Real x2 = fq3 * fq4 * (fq1 + fq2);
          dsb[isp] = fprop * (x1 - x2);
          x1 = fq3 + fq4;
          x2 = fq3 * fq4;
          dd1[isp] = fprop * fact[0] * (fq2 * x1 - x2);
          dd2[isp] = fprop * fact[1] * (fq1 * x1 - x2);
          x1 = fq1 + fq2;
          x2 = fq1 * fq2;
          dd3[isp] = fprop * fact[2] * (x2 - fq4 * x1);
          dd4[isp] = fprop * fact[3] * (x2 - fq3 * x1);
        }
      }
    }

    // 3.e :548-552
    expnd2(t, dsb, w.temp);
    expnd2(t, dd1, w.temp);
    expnd2(t, dd2, w.temp);
    expnd2(t, dd3, w.temp);
    expnd2(t, dd4, w.temp);

    // 4. :557-617, sums left to right as written
    for (int ifr = if2min; ifr <= if2max; ++ifr) {
      const int ikd = w.jkd[ifr - 1];
      int lq4[16];
      Real lq5[16], lq6[16];
      for (int k = 0; k < 16; ++k) lq4[k] = t.qst4(k, iqa - 1, ikd - 1);
      for (int k = 0; k < 16; ++k) lq5[k] = t.qst5(k, iqa - 1, ikd - 1);
      for (int k = 0; k < 16; ++k) lq6[k] = t.qst6(k, iqa - 1, ikd - 1);
      const int ispx0 = (ifr - 1) * nthexp;
      for (int ith = 1; ith <= nth; ++ith) {
        const int o = ispx0 + ith - nspmin;
        Real& sv = s[(ifr - 1) * nth + ith - 1];
        Real& dv = d[(ifr - 1) * nth + ith - 1];
        Real acc = sv;
        for (int k = 0; k < 16; ++k) acc = acc + dsb[o + lq4[k]] * lq5[k];
        sv = acc;
        const Real* const dk[4] = {dd1, dd2, dd3, dd4};
        acc = dv;
        for (int k = 0; k < 16; ++k) acc = acc + dk[k / 4][o + lq4[k]] * lq6[k];
        dv = acc;
      }
    }
  }

  // 5. :625-627, only the last IQA's IF2MIN..IF2MAX (WW3 defect, port report)
  for (int ifr = if2min; ifr <= if2max; ++ifr)
    for (int ith = 1; ith <= nth; ++ith) {
      Real& sv = s[(ifr - 1) * nth + ith - 1];
      sv = sv / t.xsi(ifr - 1) * w.xcg[ifr - 1] * kTpiInv;
    }
}

}  // namespace ww::snl3
