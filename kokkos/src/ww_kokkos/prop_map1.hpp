// kokkos/src/ww_kokkos/prop_map1.hpp
// W3MAP1, ported to Kokkos: the propagation maps of the PR1 (first-order, upwind) scheme:
// the factors FACVX and FACVY, the reciprocal of the number of wet points among the two
// that bound a cell face in X and in Y (0 when none), and the spectral index maps IS0
// and IS2 (the previous and the next direction bin, cyclic in direction). Phase 1,
// translated, not wired, not regtested.
//
// WW3 heritage: SUBROUTINE W3MAP1, WW3/model/src/w3pro1md.F90:107-286 at h0ffmann/WW3
// 96892c95 (byte-identical to NOAA-EMC/WW3 03adda91: git -C WW3 diff 03adda91 HEAD --
// model/src/w3pro1md.F90 is empty); author H. L. Tolman. Called from W3WAVE
// (w3wavemd.F90:1510, USE W3PRO1MD at :489) with the PR1 scheme, when FLMAP is set and GTYPE is not SMCTYPE (:1507-1508). Evidence:
// kokkos/port_report/W3MAP1_KOKKOS.md.
// SPDX-FileCopyrightText: 2009 National Weather Service (NWS), National Oceanic and Atmospheric Administration
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported as WW3's Release build compiles ww3_ts3's switch (switch_ST4_PR1_FLD1): every
// statement of the routine. Not compiled and not ported: W3_S (STRACE, :189-196). The
// dummy MAPSTA is the 1-D explicit-shape MAPSTA(NY*NX) of the caller, which is the
// column-major storage of W3GDATMD's MAPSTA(NY,NX).
//
// Arithmetic: two floating-point operations, both exact (v, gfortran 16.2 -O3 GIMPLE
// and object of w3pro1md.F90, WW3's CMake Release flags with the switch above, the
// recompiled object's disassembly equal to the CMake build's, 2026-11-12): FACV? + 1.
// (addss with 1.0, at most four times for a cell: a small integer in REAL) and
// 1. / FACV? (divss, only where FACV? .NE. 0.), tested with ucomiss, so the compare is
// the C++ !=. Neither loop of the object is vectorised in W3MAP1. Because every counter
// is an exact small integer, the order of the additions is not a rounding order.
//
// Why it runs as one thread: it runs only when FLMAP is set, and it is a few loops over
// the grid in the Fortran's order; see prop_map3.hpp, which does the same.
//
// ⚠ FACVX and FACVY are POINTER arrays of W3ADATMD (w3adatmd.F90:510 and :639, allocated
// NY*NX at :1417-1418); the object reads their stride from the descriptor, which for
// an array allocated there is 1. The C++ takes it as a contiguous View, and zeroes
// facvx.extent(0) elements for FACVX = 0.: the same as the Fortran only when the View
// is sized NY*NX.
#pragma once

#include <Kokkos_Core.hpp>

#include "prop_params.hpp"
#include "real.hpp"

namespace ww::prop {

/// Grid scalars of W3MAP1 (W3GDATMD NTH, NSPEC, NX, NY, ICLOSE).
struct Map1Scalars {
  int nth = 0;
  int nspec = 0;
  int nx = 0;
  int ny = 0;
  int iclose = 0;
};

/// The arrays of W3MAP1, 0-based, values 1-based as in the Fortran:
///   MAPSTA(NY*NX)        -> mapsta(IXY-1), extent NY*NX
///   FACVX, FACVY(NY*NX)  -> facvx/facvy(IXY-1) (w3adatmd.F90:1417-1418)
///   IS0, IS2(NSPEC)      -> is0/is2(ISP-1) (w3adatmd.F90:1415-1416), values 1-based ISP
template <class MemSpace>
struct Map1Views {
  using I1 = Kokkos::View<const int*, Kokkos::LayoutLeft, MemSpace>;
  using R1 = Kokkos::View<Real*, Kokkos::LayoutLeft, MemSpace>;
  using W1 = Kokkos::View<int*, Kokkos::LayoutLeft, MemSpace>;
  I1 mapsta;
  R1 facvx, facvy;
  W1 is0, is2;
};

/// The whole of W3MAP1 (:201-280) in source order, for one thread.
template <class I1, class R1, class W1>
KOKKOS_INLINE_FUNCTION void w3map1_body(const Map1Scalars& s, const I1& mapsta, const R1& facvx,
                                        const R1& facvy, const W1& is0, const W1& is2) {
  const int nx = s.nx;
  const int ny = s.ny;
  const int nth = s.nth;
  const int nspec = s.nspec;
  const Real one = 1.0F;

  for (int i = 0; i < static_cast<int>(facvx.extent(0)); ++i) facvx(i) = 0.0F;  // :201
  for (int i = 0; i < static_cast<int>(facvy.extent(0)); ++i) facvy(i) = 0.0F;  // :202

  for (int ix = 1; ix <= nx; ++ix) {  // :207-214
    for (int iy = 1; iy <= ny - 1; ++iy) {
      const int ixy = iy + (ix - 1) * ny;
      if (mapsta(ixy - 1) != 0) facvy(ixy - 1) = facvy(ixy - 1) + one;      // :210
      if (mapsta(ixy + 1 - 1) != 0) facvy(ixy - 1) = facvy(ixy - 1) + one;  // :212
    }
  }

  if (s.iclose == kIcloseTrpl) {  // :217
    const int iy = ny;            // :218
    for (int ix = 1; ix <= nx; ++ix) {  // :219-226
      int ixy = iy + (ix - 1) * ny;
      if (mapsta(ixy - 1) != 0) facvy(ixy - 1) = facvy(ixy - 1) + one;  // :221
      const int ixnext = nx - ix + 1;                                    // :223
      ixy = iy + (ixnext - 1) * ny;                                      // :224
      if (mapsta(ixy - 1) != 0) facvy(ixy - 1) = facvy(ixy - 1) + one;  // :225
    }
    // :231-235, still on row IY = NY: the seam along X
    for (int ix = 1; ix <= nx - 1; ++ix) {
      const int ixy = iy + (ix - 1) * ny;
      if (mapsta(ixy - 1) != 0) facvx(ixy - 1) = facvx(ixy - 1) + one;            // :233
      if (mapsta(ixy + ny - 1) != 0) facvx(ixy - 1) = facvx(ixy - 1) + one;       // :234
    }
  }

  for (int ix = 1; ix <= nx - 1; ++ix) {  // :240-247
    for (int iy = 2; iy <= ny - 1; ++iy) {
      const int ixy = iy + (ix - 1) * ny;
      if (mapsta(ixy - 1) != 0) facvx(ixy - 1) = facvx(ixy - 1) + one;       // :243
      if (mapsta(ixy + ny - 1) != 0) facvx(ixy - 1) = facvx(ixy - 1) + one;  // :245
    }
  }

  if (s.iclose != kIcloseNone) {  // :250
    for (int iy = 2; iy <= ny - 1; ++iy) {  // :251-257
      const int ixy = iy + (nx - 1) * ny;
      if (mapsta(ixy - 1) != 0) facvx(ixy - 1) = facvx(ixy - 1) + one;  // :253
      if (mapsta(iy - 1) != 0) facvx(ixy - 1) = facvx(ixy - 1) + one;   // :256
    }
  }

  for (int ixy = 1; ixy <= nx * ny; ++ixy) {  // :262-265
    if (facvx(ixy - 1) != 0.0F) facvx(ixy - 1) = one / facvx(ixy - 1);
    if (facvy(ixy - 1) != 0.0F) facvy(ixy - 1) = one / facvy(ixy - 1);
  }

  for (int isp = 1; isp <= nspec; ++isp) {  // :269-272
    is2(isp - 1) = isp + 1;
    is0(isp - 1) = isp - 1;
  }
  for (int isp = nth; isp <= nspec; isp += nth) is2(isp - 1) = is2(isp - 1) - nth;  // :274-276
  for (int isp = 1; isp <= nspec; isp += nth) is0(isp - 1) = is0(isp - 1) + nth;    // :278-280
}

/// W3MAP1: fills v.facvx, v.facvy, v.is0 and v.is2 from v.mapsta. One kernel of one
/// iteration (see the header), then a fence.
template <class ExecSpace>
void w3map1(const ExecSpace& exec, const Map1Scalars& s,
            const Map1Views<typename ExecSpace::memory_space>& v) {
  const Map1Scalars sc = s;
  const auto mapsta = v.mapsta;
  const auto facvx = v.facvx;
  const auto facvy = v.facvy;
  const auto is0 = v.is0;
  const auto is2 = v.is2;
  Kokkos::parallel_for(
      "w3map1", Kokkos::RangePolicy<ExecSpace>(exec, 0, 1), KOKKOS_LAMBDA(const int) {
        w3map1_body(sc, mapsta, facvx, facvy, is0, is2);
      });
  exec.fence();
}

}  // namespace ww::prop
