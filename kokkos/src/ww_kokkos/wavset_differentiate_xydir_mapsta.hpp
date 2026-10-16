// kokkos/src/ww_kokkos/wavset_differentiate_xydir_mapsta.hpp
// DIFFERENTIATE_XYDIR_MAPSTA: x and y derivatives of a nodal field on the unstructured
// (PDLIB) grid, linear shape functions, area-weighted over the triangles whose three nodes
// are all active in MAPSTA; zero at inactive nodes.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3wavset.F90:225-350 at NOAA-EMC/WW3 03adda91;
// authors Aron Roland, Mathieu Dutour-Sikiric.
// Evidence: kokkos/port_report/DIFFERENTIATE_XYDIR_MAPSTA_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstddef>

namespace ww::wavset {

/// DIFFERENTIATE_XYDIR_MAPSTA (:225-350). REAL(rkind) is float (PDLIB/yowdatapool.F90:48).
/// Arrays 0-based here:
///   var, dvdx, dvdy  VAR(npa), DVDX(npa), DVDY(npa), Fortran base 1;
///   ine              yowElementpool INE(3,NE), column-major, node numbers 1-based;
///   iplg             yowNodepool iplg(npa), local to global node, values 1-based;
///   pdlib_ien        yowNodepool PDLIB_IEN(6,NE), column-major;
///   pdlib_tria       yowNodepool PDLIB_TRIA(NE), Fortran base 1;
///   mapsta           W3GDATMD MAPSTA(NY,NX) from MAPSTA(1,1), column-major, leading dimension
///                    mapsta_ld; only row 1 is read, MAPSTA(1,IX) = mapsta[(IX-1)*mapsta_ld];
///   wei              caller-provided work array of npa floats, the automatic WEI(npa) (:299).
/// PDLIB_exchange1Dreal (:348-349) is an MPI halo exchange, taken as a callable invoked as
/// pdlib_exchange1dreal(float*) on dvdx, then on dvdy.
template <class PdlibExchange1dreal>
inline void differentiate_xydir_mapsta(const float* var, float* dvdx, float* dvdy, int npa,
                                       int ne, const int* ine, const int* iplg,
                                       const float* pdlib_ien, const float* pdlib_tria,
                                       const int* mapsta, int mapsta_ld, float* wei,
                                       PdlibExchange1dreal&& pdlib_exchange1dreal) {
  const auto mapsta1 = [&](int ix) {
    return mapsta[static_cast<std::ptrdiff_t>(ix - 1) * mapsta_ld];
  };
  for (int ip = 0; ip < npa; ++ip) {
    wei[ip] = 0.0f;
    dvdx[ip] = 0.0f;
    dvdy[ip] = 0.0f;
  }
  for (int ie = 0; ie < ne; ++ie) {
    const int ni[3] = {ine[3 * ie] - 1, ine[3 * ie + 1] - 1, ine[3 * ie + 2] - 1};
    const int ix1 = iplg[ni[0]];
    const int ix2 = iplg[ni[1]];
    const int ix3 = iplg[ni[2]];
    if (mapsta1(ix1) > 0 && mapsta1(ix2) > 0 && mapsta1(ix3) > 0) {
      // WEI(NI) = WEI(NI) + ..., DVDX(NI) = DVDX(NI) + ...: the object updates each of the
      // three nodes in place, in order, with no temporary, so a repeated node adds twice.
      const float tw = pdlib_tria[ie] * 2.0f;
      for (int k = 0; k < 3; ++k) wei[ni[k]] = wei[ni[k]] + tw;
      const float* ien = pdlib_ien + 6 * ie;
      const float dedx[3] = {ien[0], ien[2], ien[4]};
      const float dedy[3] = {ien[1], ien[3], ien[5]};
      // DOT_PRODUCT (:324-325) as gfortran inlines it: a sum started at 0.0, element order;
      // the 0.0 is kept (it turns a -0.0 product into +0.0).
      float dvdxie = 0.0f;
      float dvdyie = 0.0f;
      for (int k = 0; k < 3; ++k) dvdxie = dvdxie + var[ni[k]] * dedx[k];
      for (int k = 0; k < 3; ++k) dvdyie = dvdyie + var[ni[k]] * dedy[k];
      for (int k = 0; k < 3; ++k) dvdx[ni[k]] = dvdx[ni[k]] + dvdxie;
      for (int k = 0; k < 3; ++k) dvdy[ni[k]] = dvdy[ni[k]] + dvdyie;
    }
  }
  for (int ip = 0; ip < npa; ++ip) {
    const int ix = iplg[ip];
    const float ew = wei[ip];
    if (ew > 0.0f && mapsta1(ix) > 0) {
      dvdx[ip] = dvdx[ip] / ew;
      dvdy[ip] = dvdy[ip] / ew;
    } else {
      dvdx[ip] = 0.0f;
      dvdy[ip] = 0.0f;
    }
  }
  for (int ip = 0; ip < npa; ++ip) {
    const int ix = iplg[ip];
    if (mapsta1(ix) < 0) {
      dvdx[ip] = 0.0f;
      dvdy[ip] = 0.0f;
    }
  }
  pdlib_exchange1dreal(dvdx);
  pdlib_exchange1dreal(dvdy);
}

}  // namespace ww::wavset
