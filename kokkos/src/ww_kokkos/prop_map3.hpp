// kokkos/src/ww_kokkos/prop_map3.hpp
// W3MAP3, ported to Kokkos: the propagation maps of the PR3 (ULTIMATE QUICKEST)
// scheme, MAPX2, MAPY2, MAPAXY, MAPCXY with their counters, and the spectral
// maps MAPTH2, MAPWN2. Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3pro3md.F90:139-518 at NOAA-EMC/WW3 03adda91;
// author H. L. Tolman. Called from W3WAVE (w3wavemd.F90:1463) before W3UTRN and
// W3MAPT, when FLMAP and GTYPE /= SMCTYPE. Only the switch_lab_shrd branch is
// ported: W3_S and W3_T (STRACE, the MAPTXY/MAPTST test maps and their prints)
// are not compiled, and a kernel never prints.
//
// Arithmetic: none in floating point. Checked against gfortran 16.2 -O3 GIMPLE and
// the object of w3pro3md.F90 (WW3's own CMake Release flags, switch_lab_shrd; the
// object is byte-identical to the CMake one): integer compares, adds and stores
// only, no SSE register touched, no loop vectorised. Every value written is an
// integer fixed by the source, so the maps match the Fortran element for element
// if the stores happen in the same order with the same counters.
//
// Why it runs as one thread: NMX0..NMX2, NMY0..NMY2, NACT, NCENT and NCENTC are
// running counters that number the entries in loop order (IX outer, IY inner,
// ISEA ascending), and MAPCXY is filled from both ends. A parallel scan would
// reproduce the same maps, but it is a reorganisation of the loop, not a
// translation; the routine runs only when the maps change (FLMAP), so the whole
// body runs serially in source order in a RangePolicy of one iteration. Section 5
// tests MAPTH2(1) before writing it, which is another reason to keep it serial.
//
// Out of bounds in the Fortran (:415-422): MAPSTA(IY0,..) with IY = 1 and
// MAPSTA(IY2,..) with IY = NY address the neighbouring column of the (NY,NX)
// array (MAPSTA(0,IX) is MAPSTA(NY,IX-1)), as the GIMPLE's address arithmetic
// does. The port uses the same column-major flat address, so it reads the same
// element. It is reached only for a sea point on row 1 or NY with MAPSTA 0, 1 or
// >= 3: W3GRID turns every active point of those rows into an excluded one,
// MAPSTA = -1 (w3gridmd.F90:4584, :4773-4776), and the first branch (:412)
// catches it. Below MAPSTA(1,1) (IX = 2, IY = 1) or past MAPSTA(NY,NX) (IX =
// NX-1, IY = NY) the Fortran reads memory outside the array; the port would too,
// and the bounds-checked Debug build aborts there.
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::prop {

/// Grid scalars of W3MAP3 (W3GDATMD NX, NY, NSEA, NK, NTH, GTYPE; the per-grid
/// GRIDS(IMOD) values W3SETG points at). GTYPE: RLGTYPE = 1, CLGTYPE = 2,
/// UNGTYPE = 3, SMCTYPE = 4 (w3gdatmd.F90).
struct Map3Scalars {
  int nx = 0;
  int ny = 0;
  int nsea = 0;
  int nk = 0;
  int nth = 0;
  int gtype = 0;
};

/// The W3ADATMD counters W3MAP3 writes (:263-400), in the order of the counts
/// view. Written only when GTYPE < 3 (:255); otherwise left as the caller gave them.
struct Map3Counts {
  int nmx0 = 0, nmx1 = 0, nmx2 = 0;
  int nmy0 = 0, nmy1 = 0, nmy2 = 0;
  int nact = 0;
  int ncent = 0;
};
inline constexpr int kMap3NCounts = 8;

/// The arrays of W3MAP3, 0-based, values 1-based as in the Fortran:
///   MAPSTA(IY,IX), (NY,NX)        -> mapsta((IX-1)*NY + IY-1), flat, extent NY*NX
///                                    (w3gdatmd.F90:1733; flat so that :415-422
///                                    address as the Fortran does)
///   MAPSF(ISEA,J), (NSEA,3)       -> mapsf(ISEA-1, J-1), LayoutLeft
///   MAPX2, MAPY2, MAPAXY (NY*NX)  -> mapx2/mapy2/mapaxy(I-1) (w3adatmd.F90:1439-1441)
///   MAPCXY(NSEA)                  -> mapcxy(ISEA-1) (w3adatmd.F90:1442)
///   MAPTH2((NK+2)*NTH)            -> mapth2(I-1) (w3adatmd.F90:1443, zeroed at
///                                    allocation, :1447)
///   MAPWN2(NSPEC+NTH)             -> mapwn2(I-1) (w3adatmd.F90:1444)
///   counts                        -> kMap3NCounts ints of caller-owned work, the
///                                    Map3Counts fields in order
template <class MemSpace>
struct Map3Views {
  using I1 = Kokkos::View<const int*, Kokkos::LayoutLeft, MemSpace>;
  using I2 = Kokkos::View<const int**, Kokkos::LayoutLeft, MemSpace>;
  using W1 = Kokkos::View<int*, Kokkos::LayoutLeft, MemSpace>;
  I1 mapsta;
  I2 mapsf;
  W1 mapx2, mapy2, mapaxy, mapcxy, mapth2, mapwn2;
  W1 counts;
};

namespace map3_detail {
/// MAPSTA(IY,IX) at the column-major flat address of the Fortran.
template <class I1>
KOKKOS_INLINE_FUNCTION int sta(const I1& mapsta, int ny, int iy, int ix) {
  return mapsta((ix - 1) * ny + iy - 1);
}
}  // namespace map3_detail

/// :263-310, section 1: MAPX2 in three ranges, both (IX,IY) and (IX+1,IY) active,
/// only (IX,IY), only (IX+1,IY). IX2 wraps to 1 at IX = NX (:266).
template <class I1, class W1>
KOKKOS_INLINE_FUNCTION void w3map3_mapx2(const Map3Scalars& s, const I1& mapsta, const W1& mapx2,
                                         int& nmx0, int& nmx1, int& nmx2) {
  using map3_detail::sta;
  const int nx = s.nx;
  const int ny = s.ny;
  nmx0 = 0;  // :263-276
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
  nmx1 = nmx0;  // :280-293
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
  nmx2 = nmx1;  // :297-310
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

/// :327-374, section 2: MAPY2 likewise in IY, with IY = 1..NY-1 and IY2 = IY+1.
template <class I1, class W1>
KOKKOS_INLINE_FUNCTION void w3map3_mapy2(const Map3Scalars& s, const I1& mapsta, const W1& mapy2,
                                         int& nmy0, int& nmy1, int& nmy2) {
  using map3_detail::sta;
  const int nx = s.nx;
  const int ny = s.ny;
  nmy0 = 0;  // :327-340
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
  nmy1 = nmy0;  // :344-357
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
  nmy2 = nmy1;  // :361-374
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

/// :386-395, section 3: MAPAXY, the active points of rows 2..NY-1.
template <class I1, class W1>
KOKKOS_INLINE_FUNCTION void w3map3_mapaxy(const Map3Scalars& s, const I1& mapsta,
                                          const W1& mapaxy, int& nact) {
  const int nx = s.nx;
  const int ny = s.ny;
  nact = 0;
  for (int ix = 1; ix <= nx; ++ix) {
    const int iy0 = (ix - 1) * ny;
    for (int iy = 2; iy <= ny - 1; ++iy) {
      if (map3_detail::sta(mapsta, ny, iy, ix) == 1) {
        nact = nact + 1;
        mapaxy(nact - 1) = iy0 + iy;
      }
    }
  }
}

/// :399-429, section 4: MAPCXY, the central sea points (all eight neighbours
/// MAPSTA >= 1) from the front in ISEA order, every other sea point from the back
/// (NCENTC = NSEA downwards). The conditions are tested in the Fortran's order;
/// the .AND. chain of :415-422 has no side effect, so its evaluation order does
/// not change the result.
template <class I1, class I2, class W1>
KOKKOS_INLINE_FUNCTION void w3map3_mapcxy(const Map3Scalars& s, const I1& mapsta, const I2& mapsf,
                                          const W1& mapcxy, int& ncent) {
  using map3_detail::sta;
  const int nx = s.nx;
  const int ny = s.ny;
  const int nsea = s.nsea;
  ncent = 0;
  int ncentc = nsea;
  for (int i = 0; i < static_cast<int>(mapcxy.extent(0)); ++i) mapcxy(i) = 0;  // :401
  for (int isea = 1; isea <= nsea; ++isea) {
    const int ix = mapsf(isea - 1, 0);
    int ix0 = ix - 1;
    int ix2 = ix + 1;
    const int iy = mapsf(isea - 1, 1);
    const int iy0 = iy - 1;
    const int iy2 = iy + 1;
    if (ix == nx) ix2 = 1;  // :410-411, in this order (NX = 1 sets both)
    if (ix == 1) ix0 = nx;
    const int st = sta(mapsta, ny, iy, ix);
    if (st == 2 || st < 0) {  // :412-414
      mapcxy(ncentc - 1) = isea;
      ncentc = ncentc - 1;
    } else if (sta(mapsta, ny, iy0, ix0) >= 1 && sta(mapsta, ny, iy0, ix) >= 1 &&
               sta(mapsta, ny, iy0, ix2) >= 1 && sta(mapsta, ny, iy, ix0) >= 1 &&
               sta(mapsta, ny, iy, ix2) >= 1 && sta(mapsta, ny, iy2, ix0) >= 1 &&
               sta(mapsta, ny, iy2, ix) >= 1 && sta(mapsta, ny, iy2, ix2) >= 1) {  // :415-424
      ncent = ncent + 1;
      mapcxy(ncent - 1) = isea;
    } else {  // :425-427
      mapcxy(ncentc - 1) = isea;
      ncentc = ncentc - 1;
    }
  }
}

/// :434-491, section 5: MAPTH2 and MAPWN2, the index maps into the (NK+2,NTH)
/// work spectrum of W3KTP3. Built once: skipped when MAPTH2(1) is already set
/// (:434), which the zeroing at allocation (w3adatmd.F90:1447) makes the first call.
template <class W1>
KOKKOS_INLINE_FUNCTION void w3map3_spectral(const Map3Scalars& s, const W1& mapth2,
                                            const W1& mapwn2) {
  const int nk = s.nk;
  const int nth = s.nth;
  if (mapth2(0) != 0) return;  // :434
  for (int ik = 1; ik <= nk; ++ik) {  // :442-451
    for (int ith = 1; ith <= nth; ++ith) {
      const int isp = ith + (ik - 1) * nth;
      const int isp2 = (ik + 1) + (ith - 1) * (nk + 2);
      mapth2(isp - 1) = isp2;
    }
  }
  int isp0 = 0;  // :463-473
  for (int ik = 1; ik <= nk - 1; ++ik) {
    for (int ith = 1; ith <= nth; ++ith) {
      isp0 = isp0 + 1;
      const int isp2 = (ik + 1) + (ith - 1) * (nk + 2);
      mapwn2(isp0 - 1) = isp2;
    }
  }
  for (int ith = 1; ith <= nth; ++ith) {  // :475-482
    isp0 = isp0 + 1;
    const int isp2 = nk + 1 + (ith - 1) * (nk + 2);
    mapwn2(isp0 - 1) = isp2;
  }
  for (int ith = 1; ith <= nth; ++ith) {  // :484-491
    isp0 = isp0 + 1;
    const int isp2 = 1 + (ith - 1) * (nk + 2);
    mapwn2(isp0 - 1) = isp2;
  }
}

/// The whole of W3MAP3 (:255-500) in source order, for one thread.
template <class I1, class I2, class W1>
KOKKOS_INLINE_FUNCTION void w3map3_body(const Map3Scalars& s, const I1& mapsta, const I2& mapsf,
                                        const W1& mapx2, const W1& mapy2, const W1& mapaxy,
                                        const W1& mapcxy, const W1& mapth2, const W1& mapwn2,
                                        const W1& counts) {
  if (s.gtype < 3) {  // :255
    int nmx0 = 0, nmx1 = 0, nmx2 = 0, nmy0 = 0, nmy1 = 0, nmy2 = 0, nact = 0, ncent = 0;
    w3map3_mapx2(s, mapsta, mapx2, nmx0, nmx1, nmx2);
    w3map3_mapy2(s, mapsta, mapy2, nmy0, nmy1, nmy2);
    w3map3_mapaxy(s, mapsta, mapaxy, nact);
    w3map3_mapcxy(s, mapsta, mapsf, mapcxy, ncent);
    counts(0) = nmx0;
    counts(1) = nmx1;
    counts(2) = nmx2;
    counts(3) = nmy0;
    counts(4) = nmy1;
    counts(5) = nmy2;
    counts(6) = nact;
    counts(7) = ncent;
  }
  w3map3_spectral(s, mapth2, mapwn2);
}

/// W3MAP3: fills v.mapx2, v.mapy2, v.mapaxy, v.mapcxy (when GTYPE < 3) and
/// v.mapth2, v.mapwn2 (when MAPTH2(1) is 0), and returns the counters in c, which
/// keeps the caller's values when GTYPE >= 3. One kernel of one iteration (see
/// the header), then a fence and a copy of the eight counters to the host.
template <class ExecSpace>
void w3map3(const ExecSpace& exec, const Map3Scalars& s,
            const Map3Views<typename ExecSpace::memory_space>& v, Map3Counts& c) {
  const Map3Scalars sc = s;
  const auto mapsta = v.mapsta;
  const auto mapsf = v.mapsf;
  const auto mapx2 = v.mapx2;
  const auto mapy2 = v.mapy2;
  const auto mapaxy = v.mapaxy;
  const auto mapcxy = v.mapcxy;
  const auto mapth2 = v.mapth2;
  const auto mapwn2 = v.mapwn2;
  const auto counts = v.counts;
  Kokkos::parallel_for(
      "w3map3", Kokkos::RangePolicy<ExecSpace>(exec, 0, 1), KOKKOS_LAMBDA(const int) {
        w3map3_body(sc, mapsta, mapsf, mapx2, mapy2, mapaxy, mapcxy, mapth2, mapwn2, counts);
      });
  exec.fence();
  if (sc.gtype >= 3) return;
  int h[kMap3NCounts];
  Kokkos::View<int*, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>> hv(h,
                                                                                     kMap3NCounts);
  Kokkos::deep_copy(exec, hv, Kokkos::subview(counts, Kokkos::make_pair(0, kMap3NCounts)));
  exec.fence();
  c.nmx0 = h[0];
  c.nmx1 = h[1];
  c.nmx2 = h[2];
  c.nmy0 = h[3];
  c.nmy1 = h[4];
  c.nmy2 = h[5];
  c.nact = h[6];
  c.ncent = h[7];
}

}  // namespace ww::prop
