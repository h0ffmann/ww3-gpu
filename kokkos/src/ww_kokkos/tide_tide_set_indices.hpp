// kokkos/src/ww_kokkos/tide_tide_set_indices.hpp
// TIDE_SET_INDICES: index tables from the analysed constituents and the shallow-water
// constituents into the full constituent list, for SETVUF_FAST. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3tidemd.F90:494-527 at NOAA-EMC/WW3 03adda91;
// authors M. Foreman (tidal package), adapted to WW3 by F. Ardhuin.
// Evidence: kokkos/port_report/TIDE_SET_INDICES_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstring>

namespace ww::tide {

/// TIDE_SET_INDICES, host; no dummies, all module state of W3TIDEMD.
/// Names are CHARACTER*5, stored as consecutive 5-byte records (no terminator):
/// tidecon_allnames (NTOTAL_CON), tidecon_name (TIDE_MF), konco_con (NKONCO).
/// nj (NTOTAL_CON). Arrays 0-based; every value written is a 1-based Fortran index, as
/// SETVUF_FAST (:604-662) reads them.
/// tide_index2 (MC = 70): an entry whose name matches nothing keeps its previous value.
/// tide_indexj, tide_indexjk: zeroed over their whole allocated extents n_indexj, n_indexjk
/// (:506-507, NTOTAL_CON and NKONCO at :1467), not only over the entries then written.
inline void tide_set_indices(int tide_mf, int ntidal_con, int ntotal_con,
                             const char* tidecon_allnames, const char* tidecon_name,
                             const int* nj, const char* konco_con, int* tide_index2,
                             int* tide_indexj, long n_indexj, int* tide_indexjk, long n_indexjk) {
  // :500-504. No EXIT: the last match wins. Equal-length CHARACTER .EQ. is memcmp_eq in the object.
  for (int l = 1; l <= tide_mf; ++l) {
    for (int k = 1; k <= ntotal_con; ++k) {
      if (std::memcmp(tidecon_allnames + 5 * (k - 1), tidecon_name + 5 * (l - 1), 5) == 0)
        tide_index2[l - 1] = k;
    }
  }
  // :506-507.
  for (long i = 0; i < n_indexj; ++i) tide_indexj[i] = 0;
  for (long i = 0; i < n_indexjk; ++i) tide_indexjk[i] = 0;
  // :508-525.
  int jbase = 0;
  const int k1 = ntidal_con + 1;
  for (int k = k1; k <= ntotal_con; ++k) {
    const int j1 = jbase + 1;
    tide_indexj[k - 1] = j1;
    const int jl = jbase + nj[k - 1];
    for (int j = j1; j <= jl; ++j) {
      const int km1 = k - 1;
      // :517 sets L2=0 before the DO, a dead store. No EXIT: the last match wins.
      for (int l2 = 1; l2 <= km1; ++l2) {
        if (std::memcmp(tidecon_allnames + 5 * (l2 - 1), konco_con + 5 * (j - 1), 5) == 0)
          tide_indexjk[j - 1] = l2;
      }
    }
    jbase = jl;
  }
}

}  // namespace ww::tide
