// kokkos/src/fortran_iface/prop3_c.hpp
// The C ABI of the prop3 wiring unit (PATCH.md section 11): W3XYP3 for every spectral
// component of a propagation step, and W3KTP3 for every sea point of one intra-spectral part,
// each in one call. prop3_shim.cpp implements it; w3kokkos_prop3md.F90 on the fork branch
// ww3-gpu/kokkos-prop3 binds it, and on ww3-gpu/kokkos-prop3dev calls the _va forms
// (WFIP-0005 unit 3b, PATCH.md section 13), which work on the resident VA of resident_c.hpp. The rules of ww_kokkos_c.hpp hold: Fortran order, caller-owned
// arrays, float is REAL, nothing throws. The argument tables are above each definition in
// prop3_shim.cpp.
//
// SPDX-License-Identifier: MIT
#pragma once

#include "fortran_iface/ww_kokkos_c.hpp"

extern "C" {

/// Lengths of the RPAR/IPAR packets of ww_xyp3_batch (ww::prop::NRPAR, NIPAR of
/// prop_params.hpp) and of the map counts IMAPS (NMX0, NMX1, NMX2, NMY0, NMY1, NMY2, NACT,
/// NCENT, in that order) and the time scalars TSCAL (DSEC21 of the day, of TBPI0 to TIME, of
/// TBPI0 to TBPIN). The shim refuses other lengths.
enum { WW_XYP3_NRPAR = 5, WW_XYP3_NIPAR = 11, WW_XYP3_NIMAPS = 8, WW_XYP3_NTSCAL = 3 };

/// W3GATH, W3XYP3, W3SCAT (W3_SHRD) for every component ISPEC with on[ISPEC-1] /= 0, in one
/// call: w3wavemd.F90's ISPEC loop of section 3.6.3 on a regular or curvilinear grid.
int ww_xyp3_batch(const float* rpar, int nrpar, const int* ipar, int nipar, const int* imaps,
                  int nimaps, const float* tscal, int ntscal, float dtg, const int* mapsf,
                  const float* clats, const float* sig, const float* ecos, const float* esin,
                  const float* dpdx, const float* dpdy, const float* dqdx, const float* dqdy,
                  const float* gsqrt, const float* atrnx, const float* atrny, const int* mapx2,
                  const int* mapy2, const int* mapaxy, const int* mapcxy, const int* maptrn,
                  const int* mapsta, const float* cg, const float* cx, const float* cy,
                  int flbpi, int nbi, const int* isbpi, const float* bbpi0, const float* bbpin,
                  const int* on, int ncol, float* va);

/// ww_xyp3_batch on the resident VA of grid igrid (resident_c.hpp): the kernels read and write
/// d_va, synced first, and the state is DEVICE afterwards. va, nspec = SIZE(VA,1), nsealm =
/// UBOUND(VA,2) bind or check the context as for ww_va_host_read; ncol is nsealm + 1.
int ww_xyp3_batch_va(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar,
                     const int* imaps, int nimaps, const float* tscal, int ntscal, float dtg,
                     const int* mapsf, const float* clats, const float* sig, const float* ecos,
                     const float* esin, const float* dpdx, const float* dpdy, const float* dqdx,
                     const float* dqdy, const float* gsqrt, const float* atrnx,
                     const float* atrny, const int* mapx2, const int* mapy2, const int* mapaxy,
                     const int* mapcxy, const int* maptrn, const int* mapsta, const float* cg,
                     const float* cx, const float* cy, int flbpi, int nbi, const int* isbpi,
                     const float* bbpi0, const float* bbpin, const int* on, float* va, int nspec,
                     int nsealm);

/// Lengths of the RPAR/IPAR packets of ww_ktp3_batch (DMIN, CTMAX, FACHFA; NK, NTH, NSPEC,
/// NK2, FLCTH, FLCK, FLCUR) and of the per-point scalars PSCAL (CTHG0S, DEPTH, DDDX, DDDY, CX,
/// CY, DCXDX, DCXDY, DCYDX, DCYDY at the point, in that order).
enum { WW_KTP3_NRPAR = 3, WW_KTP3_NIPAR = 7, WW_KTP3_NPSCAL = 10 };

/// W3KTP3 NTIMES times on each of NPTS sea points, in one call: one intra-spectral part of
/// w3wavemd.F90 section 3.6.2 or 3.6.4 (the DO ITLOC loop and its JSEA loop).
int ww_ktp3_batch(const float* rpar, int nrpar, const int* ipar, int nipar, const float* sig,
                  const float* dsip, const float* ecos, const float* esin, const float* ec2,
                  const float* esc, const float* es2, const int* mapwn, const int* mapth2,
                  const int* mapwn2, int itstep, int ntimes, float facth, float fack, int nsea,
                  const float* cg, const float* wn, int npts, const int* jsea, const int* isea,
                  const float* pscal, int ncol, float* va, float* cflthmax, float* cflkmax);

/// ww_ktp3_batch on the resident VA of grid igrid, as ww_xyp3_batch_va; CFLTHMAX and CFLKMAX
/// stay host arrays.
int ww_ktp3_batch_va(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar,
                     const float* sig, const float* dsip, const float* ecos, const float* esin,
                     const float* ec2, const float* esc, const float* es2, const int* mapwn,
                     const int* mapth2, const int* mapwn2, int itstep, int ntimes, float facth,
                     float fack, int nsea, const float* cg, const float* wn, int npts,
                     const int* jsea, const int* isea, const float* pscal, float* va, int nspec,
                     int nsealm, float* cflthmax, float* cflkmax);

}  // extern "C"
