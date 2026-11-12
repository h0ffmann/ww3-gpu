// kokkos/src/ww_kokkos/prop_map2.hpp
// W3MAP2, ported to Kokkos: the propagation maps of the PR2 (third-order, UQ or UNO) scheme, MAPX2, MAPY2,
// MAPAXY, MAPXY with their counters NMX0..2, NMY0..2, NACT, NMXY, and the spectral
// maps MAPTH2, MAPWN2. MAPX2 and MAPY2 are the MAPBOU lists that W3UNO2, W3UNO2R and
// W3UNO2S read (uno2_w3uno2.hpp), MAPAXY their MAPACT. Phase 1, translated, not wired,
// not regtested.
//
// WW3 heritage: SUBROUTINE W3MAP2, WW3/model/src/w3pro2md.F90:135-492 at h0ffmann/WW3
// 96892c95 (byte-identical to NOAA-EMC/WW3 03adda91: git -C WW3 diff 03adda91 HEAD --
// model/src/w3pro2md.F90 is empty); author H. L. Tolman. Called from W3WAVE
// (w3wavemd.F90:1513, USE W3PRO2MD at :492) with the PR2 scheme, when FLMAP is set and GTYPE is not SMCTYPE (:1507-1508). Evidence:
// kokkos/port_report/W3MAP2_KOKKOS.md.
// SPDX-FileCopyrightText: 2009 National Weather Service (NWS), National Oceanic and Atmospheric Administration
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported as switch_PR2_UNO of mww3_test_01 compiles it: every statement of the routine.
// Not compiled and not ported: W3_S (STRACE) and W3_T (the MAPTXY/MAPTST test maps and
// their prints, NDST is not in the interface), as in prop_map3.hpp, which has the same
// body for sections 1 to 3 and 5. What W3MAP2 has and W3MAP3 does not: MAPXY (:390-404,
// the cells whose four corners are all sea, for the cross term of W3XYP2) in place of
// MAPCXY, and no GTYPE test, no zeroing of a map.
//
// Arithmetic: none in floating point. Checked against gfortran 16.2 -O3 and the object
// of w3pro2md.F90 (WW3's CMake Release flags, switch_PR2_UNO; the recompiled object's
// disassembly equals the CMake build's, 2026-11-12) (v): integer compares, adds and
// stores; the only SSE instructions of W3MAP2 are the integer vector ones (movd,
// movdqa, paddd, pmuludq, pshufd, ...) of the MAPTH2/MAPWN2 fills. Every value written is
// an integer fixed by the source, so the maps match the Fortran element for element if
// the counters run in the same order.
//
// Why it runs as one thread: the counters number the entries in loop order (IX outer,
// IY inner), and section 5 tests MAPTH2(1) before writing it; it runs only when FLMAP is
// set. See prop_map3.hpp.
//
// MAPSTA is read at the Fortran's column-major address (IX-1)*NY+IY-1 (flat View).
// Every index W3MAP2 uses is inside MAPSTA(NY,NX): IY runs 2..NY-1 or 1..NY-1 and
// reads rows IY, IY+1, and IX2 = 1+MOD(IX,NX) is 1..NX. (W3MAP3, unlike this, reads
// rows 0 and NY+1.)
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::prop {

/// Grid scalars of W3MAP2 (W3GDATMD NX, NY, NK, NTH).
struct Map2Scalars {
  int nx = 0;
  int ny = 0;
  int nk = 0;
  int nth = 0;
};

/// The W3ADATMD counters W3MAP2 writes, in the order of the counts view.
struct Map2Counts {
  int nmx0 = 0, nmx1 = 0, nmx2 = 0;
  int nmy0 = 0, nmy1 = 0, nmy2 = 0;
  int nact = 0;
  int nmxy = 0;
};
inline constexpr int kMap2NCounts = 8;

/// The arrays of W3MAP2, 0-based, values 1-based as in the Fortran:
///   MAPSTA(IY,IX), (NY,NX)        -> mapsta((IX-1)*NY + IY-1), flat, extent NY*NX
///   MAPX2, MAPY2, MAPAXY (NY*NX)  -> mapx2/mapy2/mapaxy(I-1) (w3adatmd.F90:1423-1425)
///   MAPXY(NSEA)                   -> mapxy(I-1) (w3adatmd.F90:1426)
///   MAPTH2((NK+2)*NTH)            -> mapth2(I-1) (w3adatmd.F90:1427, zeroed at
///                                    allocation, :1430)
///   MAPWN2(NSPEC+NTH)             -> mapwn2(I-1) (w3adatmd.F90:1428)
///   counts                        -> kMap2NCounts ints of caller-owned work, the
///                                    Map2Counts fields in order
template <class MemSpace>
struct Map2Views {
  using I1 = Kokkos::View<const int*, Kokkos::LayoutLeft, MemSpace>;
  using W1 = Kokkos::View<int*, Kokkos::LayoutLeft, MemSpace>;
  I1 mapsta;
  W1 mapx2, mapy2, mapaxy, mapxy, mapth2, mapwn2;
  W1 counts;
};

namespace map2_detail {
/// MAPSTA(IY,IX) at the column-major flat address of the Fortran.
template <class I1>
KOKKOS_INLINE_FUNCTION int sta(const I1& mapsta, int ny, int iy, int ix) {
  return mapsta((ix - 1) * ny + iy - 1);
}
}  // namespace map2_detail

/// :255-302, section 1: MAPX2 in three ranges, both (IX,IY) and (IX+1,IY) active,
/// only (IX,IY), only (IX+1,IY). IX2 wraps to 1 at IX = NX (:258).
template <class I1, class W1>
KOKKOS_INLINE_FUNCTION void w3map2_mapx2(const Map2Scalars& s, const I1& mapsta, const W1& mapx2,
                                         int& nmx0, int& nmx1, int& nmx2) {
  using map2_detail::sta;
  const int nx = s.nx;
  const int ny = s.ny;
  nmx0 = 0;  // :255-268
  for (int ix = 1; ix <= nx; ++ix) {
    const int ixy0 = (ix - 1) * ny;
    const int ix2 = 1 + ix % nx;
    for (int iy = 2; iy <= ny - 1; ++iy) {
      if (sta(mapsta, ny, iy, ix) == 1 && sta(mapsta, ny, iy, ix2) == 1) {
        nmx0 = nmx0 + 1;
        mapx2(nmx0 - 1) = ixy0 + iy;
      }
    }
  }
  nmx1 = nmx0;  // :272-285
  for (int ix = 1; ix <= nx; ++ix) {
    const int ixy0 = (ix - 1) * ny;
    const int ix2 = 1 + ix % nx;
    for (int iy = 2; iy <= ny - 1; ++iy) {
      if (sta(mapsta, ny, iy, ix) == 1 && sta(mapsta, ny, iy, ix2) != 1) {
        nmx1 = nmx1 + 1;
        mapx2(nmx1 - 1) = ixy0 + iy;
      }
    }
  }
  nmx2 = nmx1;  // :289-302
  for (int ix = 1; ix <= nx; ++ix) {
    const int ixy0 = (ix - 1) * ny;
    const int ix2 = 1 + ix % nx;
    for (int iy = 2; iy <= ny - 1; ++iy) {
      if (sta(mapsta, ny, iy, ix) != 1 && sta(mapsta, ny, iy, ix2) == 1) {
        nmx2 = nmx2 + 1;
        mapx2(nmx2 - 1) = ixy0 + iy;
      }
    }
  }
}

/// :320-367, section 2: MAPY2 likewise in IY, with IY = 1..NY-1 and IY2 = IY+1.
template <class I1, class W1>
KOKKOS_INLINE_FUNCTION void w3map2_mapy2(const Map2Scalars& s, const I1& mapsta, const W1& mapy2,
                                         int& nmy0, int& nmy1, int& nmy2) {
  using map2_detail::sta;
  const int nx = s.nx;
  const int ny = s.ny;
  nmy0 = 0;  // :320-333
  for (int ix = 1; ix <= nx; ++ix) {
    const int ixy0 = (ix - 1) * ny;
    for (int iy = 1; iy <= ny - 1; ++iy) {
      const int iy2 = iy + 1;
      if (sta(mapsta, ny, iy, ix) == 1 && sta(mapsta, ny, iy2, ix) == 1) {
        nmy0 = nmy0 + 1;
        mapy2(nmy0 - 1) = ixy0 + iy;
      }
    }
  }
  nmy1 = nmy0;  // :337-350
  for (int ix = 1; ix <= nx; ++ix) {
    const int ixy0 = (ix - 1) * ny;
    for (int iy = 1; iy <= ny - 1; ++iy) {
      const int iy2 = iy + 1;
      if (sta(mapsta, ny, iy, ix) == 1 && sta(mapsta, ny, iy2, ix) != 1) {
        nmy1 = nmy1 + 1;
        mapy2(nmy1 - 1) = ixy0 + iy;
      }
    }
  }
  nmy2 = nmy1;  // :354-367
  for (int ix = 1; ix <= nx; ++ix) {
    const int ixy0 = (ix - 1) * ny;
    for (int iy = 1; iy <= ny - 1; ++iy) {
      const int iy2 = iy + 1;
      if (sta(mapsta, ny, iy, ix) != 1 && sta(mapsta, ny, iy2, ix) == 1) {
        nmy2 = nmy2 + 1;
        mapy2(nmy2 - 1) = ixy0 + iy;
      }
    }
  }
}

/// :379-388, section 3: MAPAXY, the active points of rows 2..NY-1.
template <class I1, class W1>
KOKKOS_INLINE_FUNCTION void w3map2_mapaxy(const Map2Scalars& s, const I1& mapsta,
                                          const W1& mapaxy, int& nact) {
  const int nx = s.nx;
  const int ny = s.ny;
  nact = 0;
  for (int ix = 1; ix <= nx; ++ix) {
    const int iy0 = (ix - 1) * ny;
    for (int iy = 2; iy <= ny - 1; ++iy) {
      if (map2_detail::sta(mapsta, ny, iy, ix) == 1) {
        nact = nact + 1;
        mapaxy(nact - 1) = iy0 + iy;
      }
    }
  }
}

/// :390-404, section 4: MAPXY, the cells of rows 2..NY-1 whose four corners (IX,IY),
/// (IX2,IY), (IX,IY+1), (IX2,IY+1) all have MAPSTA >= 1. IX2 = IX+1, wrapped to 1 at
/// IX = NX (:393-394).
template <class I1, class W1>
KOKKOS_INLINE_FUNCTION void w3map2_mapxy(const Map2Scalars& s, const I1& mapsta, const W1& mapxy,
                                         int& nmxy) {
  using map2_detail::sta;
  const int nx = s.nx;
  const int ny = s.ny;
  nmxy = 0;
  for (int ix = 1; ix <= nx; ++ix) {
    const int ixy0 = (ix - 1) * ny;
    int ix2 = ix + 1;
    if (ix == nx) ix2 = 1;
    for (int iy = 2; iy <= ny - 1; ++iy) {
      if (sta(mapsta, ny, iy, ix) >= 1 && sta(mapsta, ny, iy, ix2) >= 1 &&
          sta(mapsta, ny, iy + 1, ix) >= 1 && sta(mapsta, ny, iy + 1, ix2) >= 1) {
        nmxy = nmxy + 1;
        mapxy(nmxy - 1) = ixy0 + iy;
      }
    }
  }
}

/// :408-465, section 5: MAPTH2 and MAPWN2, the index maps into the (NK+2,NTH) work
/// spectrum of W3KTP2. Built once: skipped when MAPTH2(1) is already set (:408), which
/// the zeroing at allocation (w3adatmd.F90:1430) makes the first call.
template <class W1>
KOKKOS_INLINE_FUNCTION void w3map2_spectral(const Map2Scalars& s, const W1& mapth2,
                                            const W1& mapwn2) {
  const int nk = s.nk;
  const int nth = s.nth;
  if (mapth2(0) != 0) return;  // :408
  for (int ik = 1; ik <= nk; ++ik) {  // :416-425
    for (int ith = 1; ith <= nth; ++ith) {
      const int isp = ith + (ik - 1) * nth;
      const int isp2 = (ik + 1) + (ith - 1) * (nk + 2);
      mapth2(isp - 1) = isp2;
    }
  }
  int isp0 = 0;  // :437-447
  for (int ik = 1; ik <= nk - 1; ++ik) {
    for (int ith = 1; ith <= nth; ++ith) {
      isp0 = isp0 + 1;
      const int isp2 = (ik + 1) + (ith - 1) * (nk + 2);
      mapwn2(isp0 - 1) = isp2;
    }
  }
  for (int ith = 1; ith <= nth; ++ith) {  // :449-456
    isp0 = isp0 + 1;
    const int isp2 = nk + 1 + (ith - 1) * (nk + 2);
    mapwn2(isp0 - 1) = isp2;
  }
  for (int ith = 1; ith <= nth; ++ith) {  // :458-465
    isp0 = isp0 + 1;
    const int isp2 = 1 + (ith - 1) * (nk + 2);
    mapwn2(isp0 - 1) = isp2;
  }
}

/// The whole of W3MAP2 (:255-474) in source order, for one thread.
template <class I1, class W1>
KOKKOS_INLINE_FUNCTION void w3map2_body(const Map2Scalars& s, const I1& mapsta, const W1& mapx2,
                                        const W1& mapy2, const W1& mapaxy, const W1& mapxy,
                                        const W1& mapth2, const W1& mapwn2, const W1& counts) {
  int nmx0 = 0, nmx1 = 0, nmx2 = 0, nmy0 = 0, nmy1 = 0, nmy2 = 0, nact = 0, nmxy = 0;
  w3map2_mapx2(s, mapsta, mapx2, nmx0, nmx1, nmx2);
  w3map2_mapy2(s, mapsta, mapy2, nmy0, nmy1, nmy2);
  w3map2_mapaxy(s, mapsta, mapaxy, nact);
  w3map2_mapxy(s, mapsta, mapxy, nmxy);
  counts(0) = nmx0;
  counts(1) = nmx1;
  counts(2) = nmx2;
  counts(3) = nmy0;
  counts(4) = nmy1;
  counts(5) = nmy2;
  counts(6) = nact;
  counts(7) = nmxy;
  w3map2_spectral(s, mapth2, mapwn2);  // :408 returns here, after the counters, if set
}

/// W3MAP2: fills v.mapx2, v.mapy2, v.mapaxy, v.mapxy and, when MAPTH2(1) is 0, v.mapth2
/// and v.mapwn2, and returns the counters in c. One kernel of one iteration (see the
/// header), then a fence and a copy of the eight counters to the host.
template <class ExecSpace>
void w3map2(const ExecSpace& exec, const Map2Scalars& s,
            const Map2Views<typename ExecSpace::memory_space>& v, Map2Counts& c) {
  const Map2Scalars sc = s;
  const auto mapsta = v.mapsta;
  const auto mapx2 = v.mapx2;
  const auto mapy2 = v.mapy2;
  const auto mapaxy = v.mapaxy;
  const auto mapxy = v.mapxy;
  const auto mapth2 = v.mapth2;
  const auto mapwn2 = v.mapwn2;
  const auto counts = v.counts;
  Kokkos::parallel_for(
      "w3map2", Kokkos::RangePolicy<ExecSpace>(exec, 0, 1), KOKKOS_LAMBDA(const int) {
        w3map2_body(sc, mapsta, mapx2, mapy2, mapaxy, mapxy, mapth2, mapwn2, counts);
      });
  exec.fence();
  int h[kMap2NCounts];
  Kokkos::View<int*, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>> hv(h,
                                                                                     kMap2NCounts);
  Kokkos::deep_copy(exec, hv, Kokkos::subview(counts, Kokkos::make_pair(0, kMap2NCounts)));
  exec.fence();
  c.nmx0 = h[0];
  c.nmx1 = h[1];
  c.nmx2 = h[2];
  c.nmy0 = h[3];
  c.nmy1 = h[4];
  c.nmy2 = h[5];
  c.nact = h[6];
  c.nmxy = h[7];
}

}  // namespace ww::prop
