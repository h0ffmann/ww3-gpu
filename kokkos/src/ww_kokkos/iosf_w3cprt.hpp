// kokkos/src/ww_kokkos/iosf_w3cprt.hpp
// W3CPRT: the spectral partitioning of every local sea point at an output time, ported. Per point
// it converts the action spectrum N(k,theta) of VA to the energy spectrum E(f,theta) = N*TPI*SIG/CG,
// calls W3PART (part_w3part.hpp, unchanged) and stores NP+1 columns XP(1:15,0:NP) in DTPRT one point
// after the other, with ICPRT(JSEA,1) = NP+1 and ICPRT(JSEA,2) the first column of point JSEA.
// Phase 2: it reads the resident VA (ww::resident::device_va), so an output time with partitioning
// copies the partition table back to the host instead of the spectrum.
//
// WW3 heritage: SUBROUTINE W3CPRT, WW3-noaa/model/src/w3iosfmd.F90:109-344 at NOAA-EMC/WW3 03adda91
// (identical in the h0ffmann/WW3 fork at the WW3/ pin 80644ba9), author H. L. Tolman, with C. Bunney's
// DIMXP of 25-Jul-2018. Only what W3_SHRD compiles is ported: INIT_GET_ISEA gives ISEA = JSEA
// (w3parall.F90:1236, NAPROC = 1). Not ported: W3_T (test output), W3_S (STRACE). The NaN depth stop
// (:270-274) is the caller's (W3KOKKOS_CPRT runs it before the call, which changes no output: the
// Fortran stops before it writes anything a later reader sees).
//
// Two parts, both in the Fortran's order:
//   w3cprt_point   sections 2-4 (:245-279) for one JSEA: the MAPSTA test, UABS, UDIR, DEPTH, E2
//                  and the W3PART call. Each point reads only its own column, so points run in
//                  parallel and each one is the Fortran's serial code.
//   w3cprt_store   section 5 (:284-303) for one JSEA after an integer prefix sum: the counters
//                  and the copy of XP(:,0:NP) to its columns. The Fortran's TMP array grows by
//                  TSFAC*NSEAL columns and its new columns are zeroed (:288-299), but every column
//                  1..FINSIZ is written once by :303 before DTPRT = TMP(:,1:FINSIZ) (:320-326), so
//                  DTPRT is the concatenation of the XP(:,0:NP) of the points in JSEA order, which
//                  is what the prefix sum places. Integer sums are exact in any order.
//
// Arithmetic, from the -O3 GIMPLE of w3iosfmd.F90 (gfortran 16.2.0, WW3's Release flags, the
// lab switch; port report kokkos/port_report/W3CPRT_KOKKOS.md):
// - UABS = U10*ASF, UDIR = U10D*RADE, one mulss each.
// - FACT = (TPI*SIG(IK))/CG(IK,ISEA): the product with the folded REAL TPI first, then divss.
// - E2(IK,ITH) = VA(ITH+(IK-1)*NTH,JSEA)*FACT, one mulss. No FMA (contraction off).
//
// SPDX-FileCopyrightText: 2009 National Weather Service (NWS), National Oceanic and Atmospheric Administration
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "part_w3part.hpp"
#include "real.hpp"
#include "st4_params.hpp"

namespace ww::iosf {

using ww::Real;

/// W3PART's result for a point the MAPSTA test of :253 skips (its ICPRT(JSEA,1) stays 0).
inline constexpr int kCprtSkipped = -2;

/// DIMXP of :213-219.
KOKKOS_INLINE_FUNCTION int w3cprt_dimxp(int nk, int nth, int ptmeth) {
  if (ptmeth == 4 || ptmeth == 5) return 2;
  return ((nk + 1) / 2) * ((nth - 1) / 2);
}

/// The Reals and ints of one point's work: E2(NK,NTH), XP(DIMP,0:DIMXP) and W3PART's scratch.
KOKKOS_INLINE_FUNCTION int w3cprt_nreal(int nk, int nth, int dimxp) {
  return nk * nth + ww::part::kDimp * (dimxp + 1) + ww::part::w3part_nreal(nk, nth, dimxp);
}
KOKKOS_INLINE_FUNCTION int w3cprt_nint(int nk, int nth, int dimxp, int ihmax) {
  return ww::part::w3part_nint(nk, nth, dimxp, ihmax);
}

/// The arrays of one call, index 0 at the Fortran's lower bound, LayoutLeft:
///   va      VA(NSPEC,0:NSEALM)   va(isp-1, jsea)
///   mapsf   MAPSF(NSEA,3)        mapsf(isea-1, 0) = IX; mapsta MAPSTA(NY,NX)
///   u10, u10d, asf (1:NSEA)      u10(isea-1)
///   dw      DW(0:NSEA)           dw(isea); cg, wn CG, WN(0:NK+1,0:NSEA), cg(ik, isea)
template <class Mem>
struct W3cprtArgs {
  template <class T>
  using V1 = Kokkos::View<T*, Kokkos::LayoutLeft, Mem, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
  template <class T>
  using V2 = Kokkos::View<T**, Kokkos::LayoutLeft, Mem, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
  V2<const Real> va;
  V2<const int> mapsf, mapsta;
  V1<const Real> u10, u10d, asf, dw;
  V2<const Real> cg, wn;
};

/// Sections 2-4 (:245-279) for JSEA = jsea (1-based). Returns W3PART's status (kPtmeanOk,
/// kPtmeanXpFull, kPtmeanNoPeak) and sets np (kCprtSkipped when MAPSTA < 0), xp as W3PART's
/// XP(J,N) = xp[(J-1) + 15*N], nxpfull W3PART's count. e2 holds NK*NTH Reals; w is carved.
template <class Mem>
KOKKOS_INLINE_FUNCTION int w3cprt_point(const W3cprtArgs<Mem>& a, const ww::part::PtmeanGrid& g,
                                        const ww::part::PtmeanOpts& o, const ww::part::W3partOpts& po,
                                        const int* neigh, int dimxp, int jsea, Real* e2, Real* xp,
                                        const ww::part::W3partScratch& w, int& np, int& nxpfull) {
  const int nk = g.nk, nth = g.nth;
  const int isea = jsea;  // INIT_GET_ISEA under W3_SHRD
  const int ix = a.mapsf(isea - 1, 0);
  const int iy = a.mapsf(isea - 1, 1);
  nxpfull = 0;
  if (a.mapsta(iy - 1, ix - 1) < 0) {  // :253
    np = kCprtSkipped;
    return ww::part::kPtmeanOk;
  }
  const Real uabs = a.u10(isea - 1) * a.asf(isea - 1);  // :258-260
  const Real udir = a.u10d(isea - 1) * ww::st4::kRade;
  const Real depth = a.dw(isea);
  for (int ik = 1; ik <= nk; ++ik) {  // :262-267
    const Real fact = ww::st4::kTpi * g.sig[ik] / a.cg(ik, isea);
    for (int ith = 1; ith <= nth; ++ith)
      e2[(ik - 1) + (ith - 1) * nk] = a.va(ith + (ik - 1) * nth - 1, jsea) * fact;
  }
  return ww::part::w3part(g, o, po, neigh, e2, uabs, udir, depth, &a.wn(1, isea), np, xp, dimxp, w,
                          nxpfull);  // :278-279
}

/// Section 5 (:284-303) for one point: c is NP+1 when NP >= 0 and 0 otherwise, first its
/// ICPRT(JSEA,2) (1-based column of DTPRT). icprt is ICPRT(NSEALM+1,2), dtprt DTPRT(DIMP,:).
template <class V2i, class V2r>
KOKKOS_INLINE_FUNCTION void w3cprt_store(int jsea, int np, int first, const Real* xp, const V2i& icprt,
                                         const V2r& dtprt) {
  // ICPRT(JSEA+1,2) = ICPRT(JSEA,2) [+ NP + 1] (:251, :286) is the caller's prefix sum.
  if (np < 0) return;
  icprt(jsea - 1, 0) = np + 1;
  for (int n = 0; n <= np; ++n)
    for (int j = 0; j < ww::part::kDimp; ++j) dtprt(j, first - 1 + n) = xp[j + ww::part::kDimp * n];
}

}  // namespace ww::iosf
