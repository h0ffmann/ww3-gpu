// kokkos/src/ww_kokkos/part_ptsort.hpp
// PTSORT: the counting sort of the digitised spectrum that orders W3PART's watershed, one
// spectrum per call. Phase 1, translated, not regtested. WW3 heritage:
// WW3-noaa/model/src/w3partmd.F90:511-613 at NOAA-EMC/WW3 03adda91; authors B. Tracy,
// H. L. Tolman. Evidence: kokkos/port_report/PTSORT_KOKKOS.md.
//
// Arrays are 0-based C pointers holding the Fortran's values; NSPEC = NK*NTH.
//
//   void ptsort(nspec, ihmax, imi, ind, numv, iaddr, iorder)
//     nspec   NSPEC of W3GDATMD (the module variable becomes an argument).
//     ihmax   IHMAX of W3ODATMD, OUTPTS(IMOD)%OUT6%IHMAX (w3odatmd.F90:1836).
//     imi     in, imi[nspec], levels 1..ihmax (W3PART clamps them, w3partmd.F90:349).
//     ind     out, ind[nspec], spectral indices 1..nspec by ascending level; equal levels
//             keep ascending index (stable), which PT_FLD's flooding order depends on.
//     numv, iaddr  scratch, ihmax ints each; iorder  scratch, nspec ints.
//   The five arrays must not overlap: in the Fortran they are a dummy pair and three
//   automatic arrays, and the object was compiled on that assumption.
//
// The Fortran writes IADDR(1) unconditionally (:587), so ihmax >= 1 is the caller's to
// guarantee; W3GRID makes it MAX(50, IHM) (w3gridmd.F90:3140). Integer only: no device math.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::part {

KOKKOS_INLINE_FUNCTION void ptsort(int nspec, int ihmax, const int* imi, int* ind, int* numv,
                                   int* iaddr, int* iorder) {
  for (int i = 1; i <= ihmax; ++i) numv[i - 1] = 0;
  for (int i = 1; i <= nspec; ++i) numv[imi[i - 1] - 1] = numv[imi[i - 1] - 1] + 1;

  iaddr[0] = 1;
  for (int i = 1; i <= ihmax - 1; ++i) iaddr[i] = iaddr[i - 1] + numv[i - 1];

  for (int i = 1; i <= nspec; ++i) {
    const int iv = imi[i - 1];
    const int in = iaddr[iv - 1];
    iorder[i - 1] = in;
    iaddr[iv - 1] = in + 1;
  }

  for (int i = 1; i <= nspec; ++i) ind[iorder[i - 1] - 1] = i;
}

}  // namespace ww::part
