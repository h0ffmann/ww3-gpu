// kokkos/src/ww_kokkos/part_ptnghb.hpp
// PTNGHB: the nearest-neighbour table of the spectral grid that the watershed partitioning
// (PT_FLD) walks; directions wrap, frequencies do not. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3partmd.F90:630-821 at NOAA-EMC/WW3 03adda91 (module state
// MK, MTH, NEIGH :88-92); authors B. Tracy, H. L. Tolman. Evidence:
// kokkos/port_report/PTNGHB_KOKKOS.md.
//
// Layout, as PT_FLD (part_fld.hpp) reads it: int neigh[9*NSPEC], NSPEC = NK*NTH, column-major
// like NEIGH(9,NSPEC): NEIGH(K,N) = neigh[(N-1)*9 + (K-1)]. Values are kept 1-based as the
// Fortran stores them: slots 1..NEIGH(9,N) hold spectral indices ISP = IK + (ITH-1)*NK in
// 1..NSPEC, slot 9 holds the count (5 to 8 when NK > 1, 2 when NK = 1), unused slots are 0.
//
// Rebuild rule (:694-699): the Fortran rebuilds only when NK or NTH differs from the MK, MTH of
// the last build (both -1 at load), so one table serves every grid of a ww3_multi run with the
// same NK, NTH. PtnghbTable keeps that state; a shim calls update() where W3PART calls PTNGHB
// (:359). Integer only, host only: no device math.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <vector>

namespace ww::part {

/// Builds NEIGH for an NK x NTH grid into neigh[9*NK*NTH] (w3partmd.F90:704-815).
inline void ptnghb(int nk, int nth, int* neigh) {
  const int nspec = nk * nth;
  const std::size_t total = nspec > 0 ? std::size_t{9} * static_cast<std::size_t>(nspec) : 0;
  for (std::size_t q = 0; q < total; ++q) neigh[q] = 0;

  for (int n = 1; n <= nspec; ++n) {
    const int j = (n - 1) / nk + 1;
    const int i = n - (j - 1) * nk;
    int k = 0;
    int* col = neigh + static_cast<std::size_t>(n - 1) * 9;
    auto set = [col](int kk, int v) { col[kk - 1] = v; };

    if (i != 1) { k = k + 1; set(k, n - 1); }
    if (i != nk) { k = k + 1; set(k, n + 1); }
    if (j != 1) { k = k + 1; set(k, n - nk); }
    if (j == 1) { k = k + 1; set(k, nspec - (nk - i)); }
    if (j != nth) { k = k + 1; set(k, n + nk); }
    if (j == nth) { k = k + 1; set(k, n - (nth - 1) * nk); }
    if ((i != 1) && (j != 1)) { k = k + 1; set(k, n - nk - 1); }
    if ((i != 1) && (j == 1)) { k = k + 1; set(k, n - 1 + nk * (nth - 1)); }
    if ((i != nk) && (j != 1)) { k = k + 1; set(k, n - nk + 1); }
    if ((i != nk) && (j == 1)) { k = k + 1; set(k, n + 1 + nk * (nth - 1)); }
    if ((i != 1) && (j != nth)) { k = k + 1; set(k, n + nk - 1); }
    if ((i != 1) && (j == nth)) { k = k + 1; set(k, n - 1 - (nk) * (nth - 1)); }
    if ((i != nk) && (j != nth)) { k = k + 1; set(k, n + nk + 1); }
    if ((i != nk) && (j == nth)) { k = k + 1; set(k, n + 1 - (nk) * (nth - 1)); }

    set(9, k);
  }
}

/// MK, MTH and NEIGH of W3PARTMD (:88-92) with PTNGHB's early return (:694).
struct PtnghbTable {
  int mk = -1;
  int mth = -1;
  std::vector<int> neigh;

  /// Returns true when the table was rebuilt.
  bool update(int nk, int nth) {
    if (mk == nk && mth == nth) return false;
    neigh.assign(std::size_t{9} * static_cast<std::size_t>(nk * nth), 0);
    mk = nk;
    mth = nth;
    ptnghb(nk, nth, neigh.data());
    return true;
  }
};

}  // namespace ww::part
