// kokkos/src/ww_kokkos/wavset_preparation_fd_scheme.hpp
// PREPARATION_FD_SCHEME: builds the neighbour table, the edge list and the CSR pattern
// (PDLIB_IA/JA/I_DIAG) of the finite-difference wave-setup graph of a regular grid.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3wavset.F90:1777-1932 at NOAA-EMC/WW3 03adda91;
// authors M. Dutour-Sikiric, A. Roland.
// Evidence: kokkos/port_report/PREPARATION_FD_SCHEME_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdint>

namespace ww::wavset {

// The Fortran allocates NEIGH(NSEA,4), STAT_SeaLand(NX,NY), EDGES(NBEDGE,2), PDLIB_IA(NSEA+1),
// PDLIB_JA(PDLIB_NNZ), PDLIB_I_DIAG(NSEA). NBEDGE and PDLIB_NNZ are known only after the first
// two loops, so the routine is cut at the ALLOCATE of EDGES (:1893): the caller runs
// preparation_fd_scheme_count, allocates edges(2*nbedge) and pdlib_ja(pdlib_nnz), then runs
// preparation_fd_scheme_fill. Every array is column-major, Fortran base 1, passed 0-based; the
// values stored are Fortran (1-based) sea indices, as in WW3.

namespace detail {
// NeighMat(4,2), :1862-1869.
inline constexpr int prep_fd_neighmat[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

// STAT_SeaLand(IXN,JXN) with the descriptor of ALLOCATE(STAT_SeaLand(NX,NY)): stride MAX(NX,0),
// no bounds check (the object computes (IXN-1) + MAX(NX,0)*(JXN-1) in 64 bits).
inline int prep_fd_stat(const int* stat_sealand, std::int64_t nxs, int ixn, int jxn) {
  return stat_sealand[(static_cast<std::int64_t>(ixn) - 1) +
                      nxs * (static_cast<std::int64_t>(jxn) - 1)];
}
}  // namespace detail

/// :1853-1890. mapsf: MAPSF(MSEA,3), leading dimension ld_mapsf (MSEA, w3gdatmd.F90:1736).
/// neigh: GRIDS(IMOD)%NEIGH(NSEA,4), 4*nsea ints, zeroed here (:1854).
/// stat_sealand: work array STAT_SeaLand(NX,NY), max(nx,0)*max(ny,0) ints, zeroed here (:1856);
/// the caller keeps it for preparation_fd_scheme_fill.
/// nbedge -> GRIDS(IMOD)%NBEDGE (:1892), pdlib_nnz -> yowNodepool PDLIB_NNZ.
inline void preparation_fd_scheme_count(int nx, int ny, int nsea, const int* mapsf,
                                        std::int64_t ld_mapsf, int* neigh, int* stat_sealand,
                                        int& nbedge, int& pdlib_nnz) {
  const std::int64_t nseas = nsea > 0 ? nsea : 0;
  const std::int64_t nxs = nx > 0 ? nx : 0;
  const std::int64_t nys = ny > 0 ? ny : 0;
  for (std::int64_t k = 0; k < 4 * nseas; ++k) neigh[k] = 0;
  for (std::int64_t k = 0; k < nxs * nys; ++k) stat_sealand[k] = 0;
  for (int isea = 1; isea <= nsea; ++isea) {
    const std::int64_t s = static_cast<std::int64_t>(isea) - 1;
    const int ix = mapsf[s];
    const int iy = mapsf[s + ld_mapsf];
    stat_sealand[(static_cast<std::int64_t>(ix) - 1) + nxs * (static_cast<std::int64_t>(iy) - 1)] =
        isea;
  }
  int nbe = 0;
  int nnz = 0;
  for (int isea = 1; isea <= nsea; ++isea) {
    const std::int64_t s = static_cast<std::int64_t>(isea) - 1;
    // :1874 reads IY, which nothing below uses; the object drops the load.
    const int ix = mapsf[s];
    int idx = 0;
    for (int in = 0; in < 4; ++in) {
      const int ixn = ix + detail::prep_fd_neighmat[in][0];
      // :1878 JXN = IX + NeighMat(IN,2), IX and not IY, as written.
      const int jxn = ix + detail::prep_fd_neighmat[in][1];
      const int jsea = detail::prep_fd_stat(stat_sealand, nxs, ixn, jxn);
      if (jsea > 0) {
        ++idx;
        neigh[s + nseas * (static_cast<std::int64_t>(idx) - 1)] = jsea;
        if (jsea < isea) ++nbe;
        ++nnz;
      }
    }
    ++nnz;
  }
  nbedge = nbe;
  pdlib_nnz = nnz;
}

/// :1894-1931. mapsf: MAPSF(:,1). stat_sealand as left by preparation_fd_scheme_count.
/// edges: GRIDS(IMOD)%EDGES(NBEDGE,2), 2*max(nbedge,0) ints.
/// pdlib_ia(NSEA+1), pdlib_ja(PDLIB_NNZ), pdlib_i_diag(NSEA): yowNodepool arrays.
inline void preparation_fd_scheme_fill(int nx, int nsea, const int* mapsf,
                                       const int* stat_sealand, int nbedge, int* edges,
                                       int* pdlib_ia, int* pdlib_ja, int* pdlib_i_diag) {
  const std::int64_t nxs = nx > 0 ? nx : 0;
  const std::int64_t nbes = nbedge > 0 ? nbedge : 0;
  int ix = 0;
  int idx = 0;
  for (int isea = 1; isea <= nsea; ++isea) {
    // :1897 reads IY, unused (only MAPSF(:,1) is passed here).
    ix = mapsf[static_cast<std::int64_t>(isea) - 1];
    for (int in = 0; in < 4; ++in) {
      const int ixn = ix + detail::prep_fd_neighmat[in][0];
      const int jxn = ix + detail::prep_fd_neighmat[in][1];
      const int jsea = detail::prep_fd_stat(stat_sealand, nxs, ixn, jxn);
      if (jsea > 0) {
        if (jsea < isea) {
          ++idx;
          edges[static_cast<std::int64_t>(idx) - 1] = jsea;
          edges[static_cast<std::int64_t>(idx) - 1 + nbes] = isea;
        }
      }
    }
  }
  pdlib_ia[0] = 1;
  int j = 0;
  for (int isea = 1; isea <= nsea; ++isea) {
    // :1919-1921 use IX as the previous loop left it, MAPSF(NSEA,1), for every ISEA: the loop
    // never sets IX. Reproduced as written.
    for (int in = 0; in < 4; ++in) {
      const int ixn = ix + detail::prep_fd_neighmat[in][0];
      const int jxn = ix + detail::prep_fd_neighmat[in][1];
      const int jsea = detail::prep_fd_stat(stat_sealand, nxs, ixn, jxn);
      if (jsea > 0) {
        ++j;
        pdlib_ja[static_cast<std::int64_t>(j) - 1] = jsea;
      }
    }
    ++j;
    pdlib_ja[static_cast<std::int64_t>(j) - 1] = isea;
    pdlib_i_diag[static_cast<std::int64_t>(isea) - 1] = j;
    pdlib_ia[isea] = j + 1;
  }
}

}  // namespace ww::wavset
