// kokkos/src/ww_kokkos/wavset_differentiate_xydir_native.hpp
// DIFFERENTIATE_XYDIR_NATIVE: x and y derivatives of a nodal field on the unstructured
// (PDLIB) grid, linear shape functions, area-weighted over the triangles around each node.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3wavset.F90:105-212 at NOAA-EMC/WW3 03adda91;
// authors Aron Roland, Mathieu Dutour-Sikiric.
// Evidence: kokkos/port_report/DIFFERENTIATE_XYDIR_NATIVE_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <utility>

namespace ww::wavset {

/// DIFFERENTIATE_XYDIR_NATIVE (:105-212). REAL(rkind) is float (PDLIB/yowdatapool.F90:48).
/// Arrays 0-based here:
///   var, dvdx, dvdy  VAR(npa), DVDX(npa), DVDY(npa), Fortran base 1;
///   ine              yowElementpool INE(3,NE), column-major, node numbers 1-based;
///   pdlib_ien        yowNodepool PDLIB_IEN(6,NE), column-major (yownodepool.F90:80);
///   pdlib_tria       yowNodepool PDLIB_TRIA(NE), Fortran base 1 (yownodepool.F90:80);
///   wei              caller-provided work array of npa floats, the automatic WEI(npa) (:179).
/// PDLIB_exchange1Dreal (:210-211) is an MPI halo exchange, taken as a callable invoked as
/// pdlib_exchange1dreal(float*) on dvdx, then on dvdy. MAPSTA is USEd (:157) and never read.
template <class PdlibExchange1dreal>
inline void differentiate_xydir_native(const float* var, float* dvdx, float* dvdy, int npa,
                                       int ne, const int* ine, const float* pdlib_ien,
                                       const float* pdlib_tria, float* wei,
                                       PdlibExchange1dreal&& pdlib_exchange1dreal) {
  for (int ix = 0; ix < npa; ++ix) {
    wei[ix] = 0.0f;
    dvdx[ix] = 0.0f;
    dvdy[ix] = 0.0f;
  }
  for (int ie = 0; ie < ne; ++ie) {
    const int ni[3] = {ine[3 * ie] - 1, ine[3 * ie + 1] - 1, ine[3 * ie + 2] - 1};
    // WEI(NI) = WEI(NI) + ..., DVDX(NI) = DVDX(NI) + ...: the object updates each of the
    // three nodes in place, in order, with no temporary, so a repeated node adds twice.
    const float tw = 2.0f * pdlib_tria[ie];
    for (int k = 0; k < 3; ++k) wei[ni[k]] = wei[ni[k]] + tw;
    const float* ien = pdlib_ien + 6 * ie;
    const float dedx[3] = {ien[0], ien[2], ien[4]};
    const float dedy[3] = {ien[1], ien[3], ien[5]};
    // DOT_PRODUCT (:200-201) as gfortran inlines it: a sum started at 0.0, element order;
    // the 0.0 is kept (it turns a -0.0 product into +0.0).
    float dvdxie = 0.0f;
    float dvdyie = 0.0f;
    for (int k = 0; k < 3; ++k) dvdxie = dvdxie + var[ni[k]] * dedx[k];
    for (int k = 0; k < 3; ++k) dvdyie = dvdyie + var[ni[k]] * dedy[k];
    for (int k = 0; k < 3; ++k) dvdx[ni[k]] = dvdx[ni[k]] + dvdxie;
    for (int k = 0; k < 3; ++k) dvdy[ni[k]] = dvdy[ni[k]] + dvdyie;
  }
  for (int ix = 0; ix < npa; ++ix) {
    const float ew = wei[ix];
    dvdx[ix] = dvdx[ix] / ew;
    dvdy[ix] = dvdy[ix] / ew;
  }
  pdlib_exchange1dreal(dvdx);
  pdlib_exchange1dreal(dvdy);
}

}  // namespace ww::wavset
