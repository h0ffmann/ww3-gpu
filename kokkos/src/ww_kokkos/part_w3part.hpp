// kokkos/src/ww_kokkos/part_w3part.hpp
// W3PART: the watershed partitioning of one spectrum (PTMETH 1 to 5): levels, PTSORT, PT_FLD,
// PTMEAN, the wind-sea sort and combination, the wave-age (4) and two-band (5) methods.
// Phase 1, translated, not regtested. WW3 heritage: WW3-noaa/model/src/w3partmd.F90:138-497 at
// NOAA-EMC/WW3 03adda91; authors B. Tracy, H. L. Tolman, M. Szyszka, C. Bunney. Evidence:
// kokkos/port_report/W3PART_KOKKOS.md.
//
// Arrays are 0-based C pointers holding the Fortran's values; NSPEC = NK*NTH, ISP = IK + (ITH-1)*NK.
//
//   int w3part(g, o, po, neigh, spec, uabs, udir, depth, wn, np, xp, dimxp, w, nxpfull)
//     g, o   PtmeanGrid, PtmeanOpts (part_ptmean.hpp); W3PART reads g.sig[1..nk], g.th and
//            o.wsmult, o.ptmeth (one PTMETH for W3PART and PTMEAN, as in the Fortran).
//     po     W3partOpts: IHMAX, WSCUT, PTFCUT, FLCOMB of W3ODATMD (w3odatmd.F90:1836-1850).
//     neigh  NEIGH(9,NSPEC) for this NK, NTH as PtnghbTable::update() leaves it (part_ptnghb.hpp).
//     spec   SPEC(NK,NTH) column-major; wn[nk].
//     np, xp out, as PTMEAN's npo, xp: XP(J,N) = xp[(J-1) + 15*N], N = 0..dimxp.
//     w      W3partScratch, carved by w3part_carve() from w3part_nreal() Reals and
//            w3part_nint() ints; nothing is shared between spectra.
//     nxpfull  out, PTMEAN calls that returned kPtmeanXpFull; each one is a format 1000 line
//            with DIMXP+1 that the shim writes on NDSE when IAPROC == NAPERR (w3partmd.F90:1416).
//   Returns kPtmeanOk, kPtmeanXpFull (nxpfull > 0, the Fortran went on and so did this), or
//   kPtmeanNoPeak: PTMEAN met IFPMAX = 0 and the Fortran reads out of bounds; np and xp are not
//   the Fortran's and the shim calls the Fortran W3PART for that spectrum, discarding nxpfull: an
//   earlier PTMEAN call may have counted one, and the Fortran call writes its own format 1000.
//
// ⚠ On a device cosf (and PTMEAN's libm calls) and roundf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cmath>

#include <Kokkos_Core.hpp>

#include "part_fld.hpp"
#include "part_ptmean.hpp"
#include "part_ptnghb.hpp"
#include "part_ptsort.hpp"
#include "prop_params.hpp"
#include "real.hpp"
#include "st4_params.hpp"

namespace ww::part {

struct W3partOpts {
  int ihmax;  // >= 1, as PTSORT needs
  Real wscut, ptfcut;
  bool flcomb;
};

struct W3partScratch {
  Real* zp;    // ZP(NSPEC)
  Real* z;     // Z(NSPEC)
  Real* tp;    // TP(DIMP,DIMXP), TP(J,N) = tp[(J-1) + 15*(N-1)]
  Real* mean;  // PTMEAN's Reals for NPI <= NSPEC+1
  int* imi;    // IMI, IMO, IND (NSPEC)
  int* imo;
  int* ind;
  int* index;  // INDEX(DIMXP), PMAP(DIMXP)
  int* pmap;
  int* numv;   // PTSORT: NUMV, IADDR (IHMAX), IORDER (NSPEC)
  int* iaddr;
  int* iorder;
  int* imd;    // PT_FLD: IMD, IQ (NSPEC)
  int* iq;
  int* ifpmax;  // PTMEAN: IFPMAX(0:NPI), NSPEC+2
};

// NP_MAX is PT_FLD's label count (<= NSPEC), plus one under PTMETH 2 (:385), 2 under 4 and 5.
KOKKOS_INLINE_FUNCTION int w3part_nreal(int nk, int nth, int dimxp) {
  const int nspec = nk * nth;
  return 2 * nspec + kDimp * dimxp + ptmean_nreal(nk, nth, nspec + 1);
}

KOKKOS_INLINE_FUNCTION int w3part_nint(int nk, int nth, int dimxp, int ihmax) {
  const int nspec = nk * nth;
  return 6 * nspec + 2 * dimxp + 2 * ihmax + (nspec + 2);
}

KOKKOS_INLINE_FUNCTION W3partScratch w3part_carve(Real* r, int* ib, int nk, int nth, int dimxp,
                                                  int ihmax) {
  const int nspec = nk * nth;
  W3partScratch w;
  w.zp = r;
  w.z = w.zp + nspec;
  w.tp = w.z + nspec;
  w.mean = w.tp + kDimp * dimxp;
  w.imi = ib;
  w.imo = w.imi + nspec;
  w.ind = w.imo + nspec;
  w.index = w.ind + nspec;
  w.pmap = w.index + dimxp;
  w.numv = w.pmap + dimxp;
  w.iaddr = w.numv + ihmax;
  w.iorder = w.iaddr + ihmax;
  w.imd = w.iorder + nspec;
  w.iq = w.imd + nspec;
  w.ifpmax = w.iq + nspec;
  return w;
}

namespace w3part_detail {
// MINVAL(ZP) (:342) as gfortran -O3 inlines it: the first non-NaN element seeds the limit,
// then strict '<' (NaN skipped); all NaN gives NaN. NSPEC >= 1. MAXVAL is pt_fld's.
KOKKOS_INLINE_FUNCTION Real minval(const Real* a, int n) {
  int s = 1;
  for (; s <= n; ++s)
    if (a[s - 1] <= INFINITY) break;
  if (s > n) return NAN;
  Real limit = a[s - 1];
  for (int i = s + 1; i <= n; ++i)
    if (a[i - 1] < limit) limit = a[i - 1];
  return limit;
}

// MAXLOC(A(1:N)) of a row of TP (stride DIMP): first non-NaN seeds, strict '>' keeps the first
// of equal maxima, all NaN gives 1 (the object's PHI at :417, :434, :462, :485). N >= 1.
KOKKOS_INLINE_FUNCTION int maxloc(const Real* a, int n) {
  int s = 1;
  for (; s <= n; ++s)
    if (a[kDimp * (s - 1)] >= -INFINITY) break;
  if (s > n) return 1;
  int pos = s;
  Real limit = a[kDimp * (s - 1)];
  for (int i = s + 1; i <= n; ++i) {
    if (a[kDimp * (i - 1)] > limit) {
      pos = i;
      limit = a[kDimp * (i - 1)];
    }
  }
  return pos;
}
}  // namespace w3part_detail

/// W3PART (w3partmd.F90:138-497), one spectrum, serial.
KOKKOS_INLINE_FUNCTION int w3part(const PtmeanGrid& g, const PtmeanOpts& o, const W3partOpts& po,
                                  const int* neigh, const Real* spec, Real uabs, Real udir,
                                  Real depth, const Real* wn, int& np, Real* xp, int dimxp,
                                  const W3partScratch& w, int& nxpfull) {
  using ww::st4::max_r;
  const int nk = g.nk, nth = g.nth, nspec = nk * nth;
  const Real zero = static_cast<Real>(0.), one = static_cast<Real>(1.);
  const Real minus1 = static_cast<Real>(-1.);
  auto XP = [&](int j, int n) -> Real& { return xp[(j - 1) + kDimp * n]; };
  auto TP = [&](int j, int n) -> Real& { return w.tp[(j - 1) + kDimp * (n - 1)]; };
  int np_max = 0;
  nxpfull = 0;
  // Each PTMEAN call; true when it met kPtmeanNoPeak and the shim must take over.
  auto mean = [&]() -> bool {
    const PtmeanScratch pw = ptmean_carve(w.mean, w.ifpmax, nk, nth, np_max);
    const int rc = ptmean(g, o, np_max, w.imo, w.zp, depth, uabs, udir, wn, np, xp, dimxp,
                          w.pmap, pw);
    if (rc == kPtmeanXpFull) nxpfull = nxpfull + 1;
    return rc == kPtmeanNoPeak;
  };
  auto status = [&]() { return nxpfull > 0 ? kPtmeanXpFull : kPtmeanOk; };

  np = 0;  // :266-267
  for (int i = 0; i < kDimp * (dimxp + 1); ++i) xp[i] = zero;

  for (int i = 0; i < nspec; ++i) w.zp[i] = spec[i];  // :273-275, same column-major order

  if (o.ptmeth == 4) {  // :281-308
    for (int ik = 1; ik <= nk; ++ik) {
      for (int ith = 1; ith <= nth; ++ith) {
        const int isp = ik + (ith - 1) * nk;
        const Real upar = o.wsmult * uabs *
                          max_r(Kokkos::cos(g.th[ith - 1] - ww::prop::kDera * udir), zero);
        const Real c = g.sig[ik] / wn[ik - 1];
        w.imo[isp - 1] = (upar <= c) ? 2 : 1;
      }
    }
    np_max = 2;
    if (mean()) return kPtmeanNoPeak;
    return status();
  }

  if (o.ptmeth == 5) {  // :313-338
    const Real sigcut = ww::st4::kTpi * po.ptfcut;
    for (int ik = 1; ik <= nk; ++ik) {
      const int ip = (g.sig[ik] <= sigcut) ? 2 : 1;
      for (int ith = 1; ith <= nth; ++ith) w.imo[ik + (ith - 1) * nk - 1] = ip;
    }
    np_max = 2;
    if (mean()) return kPtmeanNoPeak;
    return status();
  }

  const Real zmin = w3part_detail::minval(w.zp, nspec);  // :342-344
  const Real zmax = pt_fld_detail::maxval(w.zp, nspec);
  if (zmax - zmin < static_cast<Real>(1.E-9)) return kPtmeanOk;

  for (int i = 0; i < nspec; ++i) w.z[i] = zmax - w.zp[i];  // :346

  const Real fact = static_cast<Real>(po.ihmax - 1) / (zmax - zmin);  // :348-349
  for (int i = 0; i < nspec; ++i) {
    const int lev = ww::st4::nint(one + w.z[i] * fact);  // NINT is lroundf in the object
    const int lo = (po.ihmax < lev) ? po.ihmax : lev;
    w.imi[i] = (lo > 1) ? lo : 1;
  }

  ptsort(nspec, po.ihmax, w.imi, w.ind, w.numv, w.iaddr, w.iorder);  // :353
  // :359 PTNGHB: the caller passes the table (PtnghbTable::update, or the host overload below).
  pt_fld(nspec, po.ihmax, neigh, w.imi, w.ind, w.imo, w.zp, np_max, w.imd, w.iq);  // :363
  if (mean()) return kPtmeanNoPeak;  // :368

  if (np > 0 && o.ptmeth == 2) {  // :374-400
    int wind_part = np_max;
    for (int ik = 1; ik <= nk; ++ik) {
      for (int ith = 1; ith <= nth; ++ith) {
        const int isp = ik + (ith - 1) * nk;
        const Real upar = o.wsmult * uabs *
                          max_r(Kokkos::cos(g.th[ith - 1] - ww::prop::kDera * udir), zero);
        const Real c = g.sig[ik] / wn[ik - 1];
        if (c < upar) {
          wind_part = np_max + 1;
          w.imo[isp - 1] = wind_part;
        }
      }
    }
    if (wind_part != np_max) {
      np_max = wind_part;
      if (mean()) return kPtmeanNoPeak;
    }
  }

  if (np <= 1) return status();  // :406

  if (o.ptmeth == 3) {  // :412-423, sort by Hs only
    for (int ip = 1; ip <= np; ++ip)
      for (int j = 1; j <= kDimp; ++j) TP(j, ip) = XP(j, ip);
    for (int ip = 1; ip <= np; ++ip)
      for (int j = 1; j <= kDimp; ++j) XP(j, ip) = zero;
    for (int ip = 1; ip <= np; ++ip) {
      const int it = w3part_detail::maxloc(&TP(1, 1), np);
      for (int j = 1; j <= kDimp; ++j) XP(j, ip) = TP(j, it);
      TP(1, it) = minus1;
    }
    return status();
  }

  // :428-439 sort by wind-sea fraction XP(6,:), counting the wind seas
  auto sort_ws = [&](int& nws) {
    for (int ip = 1; ip <= np; ++ip)
      for (int j = 1; j <= kDimp; ++j) TP(j, ip) = XP(j, ip);
    for (int ip = 1; ip <= np; ++ip)
      for (int j = 1; j <= kDimp; ++j) XP(j, ip) = zero;
    for (int ip = 1; ip <= np; ++ip) w.index[ip - 1] = 0;
    nws = 0;
    for (int ip = 1; ip <= np; ++ip) {
      const int it = w3part_detail::maxloc(&TP(6, 1), np);
      w.index[ip - 1] = it;
      for (int j = 1; j <= kDimp; ++j) XP(j, ip) = TP(j, w.index[ip - 1]);
      if (TP(6, it) >= po.wscut) nws = nws + 1;
      TP(6, it) = minus1;
    }
  };
  int nws = 0;
  sort_ws(nws);

  if (nws > 1 && po.flcomb) {  // :443-469
    const int ipw = w.pmap[w.index[0] - 1];
    for (int ip = 2; ip <= nws; ++ip) {
      const int ipt = w.pmap[w.index[ip - 1] - 1];
      for (int isp = 1; isp <= nspec; ++isp)
        if (w.imo[isp - 1] == ipt) w.imo[isp - 1] = ipw;
    }
    if (mean()) return kPtmeanNoPeak;
    if (np <= 1) return status();
    sort_ws(nws);
  }

  nws = (1 < nws) ? 1 : nws;  // :473-488, the rest by Hs

  for (int ip = 1; ip <= np; ++ip)
    for (int j = 1; j <= kDimp; ++j) TP(j, ip) = XP(j, ip);
  for (int ip = 1; ip <= np; ++ip)
    for (int j = 1; j <= kDimp; ++j) XP(j, ip) = zero;

  if (nws > 0) {
    for (int j = 1; j <= kDimp; ++j) XP(j, 1) = TP(j, 1);
    TP(1, 1) = minus1;
    nws = 1;
  }

  for (int ip = nws + 1; ip <= np; ++ip) {
    const int it = w3part_detail::maxloc(&TP(1, 1), np);
    for (int j = 1; j <= kDimp; ++j) XP(j, ip) = TP(j, it);
    TP(1, it) = minus1;
  }
  return status();
}

/// Host form with W3PARTMD's NEIGH state. The table is a function of (NK, NTH) only, so
/// updating it before the early returns of :281-344 changes no output. Not thread-safe:
/// a parallel caller updates the table once and calls the form above.
inline int w3part(PtnghbTable& table, const PtmeanGrid& g, const PtmeanOpts& o,
                  const W3partOpts& po, const Real* spec, Real uabs, Real udir, Real depth,
                  const Real* wn, int& np, Real* xp, int dimxp, const W3partScratch& w,
                  int& nxpfull) {
  table.update(g.nk, g.nth);
  return w3part(g, o, po, table.neigh.data(), spec, uabs, udir, depth, wn, np, xp, dimxp, w,
                nxpfull);
}

}  // namespace ww::part
