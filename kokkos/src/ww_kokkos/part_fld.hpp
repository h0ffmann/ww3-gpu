// kokkos/src/ww_kokkos/part_fld.hpp
// PT_FLD with its internal FIFO_ADD, FIFO_EMPTY, FIFO_FIRST: the incremental flooding of
// W3PART's watershed partitioning, one spectrum per call. Phase 1, translated, not
// regtested. WW3 heritage: WW3-noaa/model/src/w3partmd.F90:839-1156 at NOAA-EMC/WW3
// 03adda91; authors H. L. Tolman, B. Tracy (FIFO). Evidence: kokkos/port_report/PT_FLD_KOKKOS.md.
//
//   pt_fld(nspec, ihmax, neigh, imi, ind, imo, zp, npart, imd, iq)
//     in  neigh[9*nspec]  NEIGH(K,N) = neigh[(N-1)*9 + K-1], NEIGH(9,N) the count, as
//                         PTNGHB builds it (ww::part::ptnghb, part_ptnghb.hpp)
//         imi[nspec]      levels 1..IHMAX (W3PART :349)
//         ind[nspec]      spectral indices 1..NSPEC sorted by level (ww::part::ptsort)
//         zp[nspec]       the spectrum, ISP = IK + (ITH-1)*NK
//     out imo[nspec]      partition labels; npart = IC_LABEL
//     scratch imd[nspec], iq[nspec] (the FIFO: exactly NSPEC slots, as IQ(NSPEC) :899)
// Arrays are 0-based pointers holding the Fortran's values (indices stay 1-based).
// IHMAX is OUTPTS(IMOD)%OUT6%IHMAX (w3odatmd.F90:1836), MAX(50, IHM) (w3gridmd.F90:3140).
// The only float work is MAXVAL(ZP) and ABS(ZP-ZP) with comparisons, no libm call, so
// device math is not at stake.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cfloat>
#include <cmath>

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::part {

namespace pt_fld_detail {
inline constexpr int kNeighStride = 9;  // NEIGH(9,NSPEC), w3partmd.F90:697
// FIFO_ADD, FIFO_EMPTY, FIFO_FIRST (:1103-1152): a ring of NSPEC slots; full reads as empty.
struct Fifo {
  int* iq;
  int nspec;
  int start = 1;  // IQ_START :920
  int end = 1;    // IQ_END :921
  KOKKOS_INLINE_FUNCTION void add(int iv) {
    iq[end - 1] = iv;
    end = end + 1;
    if (end > nspec) end = 1;
  }
  KOKKOS_INLINE_FUNCTION bool empty() const { return start == end; }
  KOKKOS_INLINE_FUNCTION int first() {
    const int iv = iq[start - 1];
    start = start + 1;
    if (start > nspec) start = 1;
    return iv;
  }
};

// MAXVAL(ZP) (:923) as gfortran -O3 inlines it: the first non-NaN element seeds the
// limit, then strict '>' (NaN skipped); all NaN gives NaN, an empty array -HUGE.
KOKKOS_INLINE_FUNCTION Real maxval(const Real* zp, int n) {
  if (n <= 0) return -FLT_MAX;
  int s = 1;
  for (; s <= n; ++s)
    if (zp[s - 1] >= -INFINITY) break;
  if (s > n) return NAN;
  Real limit = zp[s - 1];
  for (int i = s + 1; i <= n; ++i)
    if (zp[i - 1] > limit) limit = zp[i - 1];
  return limit;
}
}  // namespace pt_fld_detail

/// PT_FLD (w3partmd.F90:839-1156): incremental flooding (Vincent and Soille 1991) over the
/// levels 1..IHMAX in IND's order, then up to five sweeps that give each watershed pixel
/// (IMO = 0) the label of its neighbour closest in ZP. Serial per spectrum.
KOKKOS_INLINE_FUNCTION void pt_fld(int nspec, int ihmax, const int* neigh, const int* imi,
                                   const int* ind, int* imo, const Real* zp, int& npart,
                                   int* imd, int* iq) {
  constexpr int kMask = -2;
  constexpr int kInit = -1;
  constexpr int kIwshed = 0;
  constexpr int kIfictPixel = -100;
  auto nb = [neigh](int k, int n) {
    return neigh[(n - 1) * pt_fld_detail::kNeighStride + (k - 1)];
  };

  for (int s = 0; s < nspec; ++s) imo[s] = kInit;  // :915
  int ic_label = 0;
  for (int s = 0; s < nspec; ++s) imd[s] = 0;  // :917
  pt_fld_detail::Fifo fifo{iq, nspec};
  const Real zpmax = pt_fld_detail::maxval(zp, nspec);

  int m = 1;
  for (int ih = 1; ih <= ihmax; ++ih) {
    const int msave = m;
    // :935-961 mask the level; queue pixels touching a basin or a watershed
    for (;;) {
      const int ip = ind[m - 1];
      if (imi[ip - 1] != ih) break;
      imo[ip - 1] = kMask;
      for (int i = 1; i <= nb(9, ip); ++i) {
        const int ipp = nb(i, ip);
        if ((imo[ipp - 1] > 0) || (imo[ipp - 1] == kIwshed)) {
          imd[ip - 1] = 1;
          fifo.add(ip);
          break;
        }
      }
      if (m + 1 > nspec) break;
      m = m + 1;
    }
    // :965-1018 extend basins breadth first, IC_DIST counting fictitious-pixel passes
    int ic_dist = 1;
    fifo.add(kIfictPixel);
    for (;;) {
      int ip = fifo.first();
      if (ip == kIfictPixel) {
        if (fifo.empty()) break;
        fifo.add(kIfictPixel);
        ic_dist = ic_dist + 1;
        ip = fifo.first();
      }
      for (int i = 1; i <= nb(9, ip); ++i) {
        const int ipp = nb(i, ip);
        if ((imd[ipp - 1] < ic_dist) && ((imo[ipp - 1] > 0) || (imo[ipp - 1] == kIwshed))) {
          if (imo[ipp - 1] > 0) {
            if ((imo[ip - 1] == kMask) || (imo[ip - 1] == kIwshed)) {
              imo[ip - 1] = imo[ipp - 1];
            } else if (imo[ip - 1] != imo[ipp - 1]) {
              imo[ip - 1] = kIwshed;
            }
          } else if (imo[ip - 1] == kMask) {
            imo[ip - 1] = kIwshed;
          }
        } else if ((imo[ipp - 1] == kMask) && (imd[ipp - 1] == 0)) {
          imd[ipp - 1] = ic_dist + 1;
          fifo.add(ipp);
        }
      }
    }
    // :1022-1062 new minima: label each still-masked component
    m = msave;
    for (;;) {
      const int ip = ind[m - 1];
      if (imi[ip - 1] != ih) break;
      imd[ip - 1] = 0;
      if (imo[ip - 1] == kMask) {
        ic_label = ic_label + 1;
        fifo.add(ip);
        imo[ip - 1] = ic_label;
        for (;;) {
          if (fifo.empty()) break;
          const int ipp = fifo.first();
          for (int i = 1; i <= nb(9, ipp); ++i) {
            const int ippp = nb(i, ipp);
            if (imo[ippp - 1] == kMask) {
              fifo.add(ippp);
              imo[ippp - 1] = ic_label;
            }
          }
        }
      }
      if (m + 1 > nspec) break;
      m = m + 1;
    }
  }

  // :1071-1089 Jacobi sweeps: reads IMO, writes IMD; '<=' lets the last tie win
  for (int j = 1; j <= 5; ++j) {
    for (int s = 0; s < nspec; ++s) imd[s] = imo[s];
    for (int jl = 1; jl <= nspec; ++jl) {
      int ipt = -1;
      if (imo[jl - 1] == 0) {
        Real ep1 = zpmax;
        for (int jn = 1; jn <= nb(9, jl); ++jn) {
          const Real diff = Kokkos::fabs(zp[jl - 1] - zp[nb(jn, jl) - 1]);
          if ((diff <= ep1) && (imo[nb(jn, jl) - 1] != 0)) {
            ep1 = diff;
            ipt = jn;
          }
        }
        if (ipt > 0) imd[jl - 1] = imo[nb(ipt, jl) - 1];
      }
    }
    for (int s = 0; s < nspec; ++s) imo[s] = imd[s];
    int mn = imo[0];  // MINVAL(IMO) > 0 (:1088); NSPEC >= 1
    for (int s = 1; s < nspec; ++s) mn = (imo[s] < mn) ? imo[s] : mn;
    if (mn > 0) break;
  }

  npart = ic_label;
}

}  // namespace ww::part
