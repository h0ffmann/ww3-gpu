// kokkos/src/ww_kokkos/updt_dzxy.hpp
// W3DZXY, ported to Kokkos: gradients DZZDX, DZZDY of a sea-point field ZZ on the
// rectilinear (IY,IX) grid, by centred differences in the (p,q) index space,
// one-sided next to land, mapped to (x,y) by DPDX, DPDY, DQDX, DQDY. Phase 1,
// translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3updtmd.F90:3211-3529 at NOAA-EMC/WW3 03adda91;
// author W. E. Rogers, NRL (:3215). Called from W3WAVE (w3wavemd.F90:1213-1214 and
// 1491 with contiguous arrays; :1507, under W3_REFRX, which the lab switch does not
// set, passes the strided section DCDX(IK,:,:) of DCDX(0:NK+1,NY,NX), which a
// LayoutLeft View cannot alias: that caller must copy). Only
// the switch_lab_shrd branch is ported: W3_S (STRACE) and W3_T (the PRTBLK test
// print, :3486-3513) are not compiled there, and a kernel never prints. ZUNIT is
// read only by the W3_T print, so it is not an argument here.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE of w3updtmd.F90 (WW3's own
// flags, switch_lab_shrd). Per point and per output the GIMPLE computes
//   tx = (ZZ(a) - ZZ(b)) * STX,  ty = (ZZ(c) - ZZ(d)) * STY,
//   DZZDX = ((tx * DPDX) + (ty * DQDX)) * DFAC,
//   DZZDY = ((tx * DPDY) + (ty * DQDY)) * DFAC,
// Fortran's left-to-right a*b*c = (a*b)*c, tx and ty shared between the two
// outputs (CSE, no bit change), no FMA (contraction off), the intermediate store
// of DZZDX before the *DFAC folded away (no bit change). Neither loop nest is
// vectorised (w3updtmd.vec.txt has no line in 3211-3529).
//
// Operand order in the assembly: for DZZDY the Fortran object multiplies DPDY*tx,
// DQDY*ty and gy*DFAC (RTL swapped the GIMPLE order); g++ emits tx*dpdy, ty*dqdy,
// dfac*gy whichever way the source is written (a VEX memory operand is always the
// second source). A product gives the same bits either way, except which NaN
// payload survives when both operands are NaN: DFAC is never NaN, a NaN grid
// metric DPDY/DQDY together with a NaN tx/ty is the only case, not checked ⚠.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::updt {

// ICLOSE values: w3gdatmd.F90:630-632 alias ICLO_* of w3gsrumd.F90:314-319.
inline constexpr int kIcloseNone = -1;
inline constexpr int kIcloseSmpl = 2;
inline constexpr int kIcloseTrpl = 8;

// DFAC = 1. / (DERA * RADIUS) (w3updtmd.F90:3346), all REAL PARAMETERs of
// constants.F90:72-80 (PI, TPI, TPIINV, DERA, RADIUS = 4.E7*TPIINV). gfortran folds
// it at compile time to 9.0000003183376975357532501220703125e-6 (the GIMPLE PHI of
// dfac), float bits 0x3716feb5. Written as that literal so no compiler re-folds it.
inline constexpr Real kDfacLL = 0x1.2dfd6ap-17f;

// Index maps (Fortran 1-based -> C 0-based), all 2-D arrays LayoutLeft (NY,NX):
//   A(IY,IX) is a(IY-1, IX-1);  ZZ(ISEA) is zz(ISEA-1);  ZZ(MAPFS(IY,IX)) is
//   zz(mapfs(IY-1,IX-1) - 1). The functions below take and use Fortran IY, IX.
template <class IView, class RView, class ZView>
KOKKOS_INLINE_FUNCTION Real dzxy_zz(const ZView& zz, const IView& mapfs, int iy, int ix) {
  return zz(mapfs(iy - 1, ix - 1) - 1);
}

/// One interior point (IY in 2..NY-1), w3updtmd.F90:3364-3416. Writes
/// dzzdx(IY,IX), dzzdy(IY,IX) only when MAPSTA(IY,IX) /= 0.
template <class IView, class RView, class ZView, class OView>
KOKKOS_INLINE_FUNCTION void w3dzxy_point(int iy, int ix, int nx, Real dfac, const ZView& zz,
                                         const IView& mapsta, const IView& mapfs,
                                         const RView& dpdx, const RView& dpdy,
                                         const RView& dqdx, const RView& dqdy,
                                         const OView& dzzdx, const OView& dzzdy) {
  if (mapsta(iy - 1, ix - 1) == 0) return;  // :3366
  Real stx = Real(0.5);
  const int ixps = (ix == nx) ? 1 : ix + 1;  // :3368-3372, wrap at the closure
  int ixp;
  if (mapsta(iy - 1, ixps - 1) == 0) {
    ixp = ix;
    stx = Real(1.0);
  } else {
    ixp = ixps;
  }
  const int ixms = (ix == 1) ? nx : ix - 1;  // :3381-3385
  int ixm;
  if (mapsta(iy - 1, ixms - 1) == 0) {
    ixm = ix;
    stx = Real(1.0);
  } else {
    ixm = ixms;
  }
  Real sty = Real(0.5);
  const int iyps = iy + 1;  // :3394
  int iyp;
  if (mapsta(iyps - 1, ix - 1) == 0) {
    iyp = iy;
    sty = Real(1.0);
  } else {
    iyp = iyps;
  }
  const int iyms = iy - 1;  // :3401
  int iym;
  if (mapsta(iyms - 1, ix - 1) == 0) {
    iym = iy;
    sty = Real(1.0);
  } else {
    iym = iyms;
  }
  // :3408-3413, association as the GIMPLE (header comment).
  const Real tx = (dzxy_zz<IView, RView>(zz, mapfs, iy, ixp) -
                   dzxy_zz<IView, RView>(zz, mapfs, iy, ixm)) * stx;
  const Real ty = (dzxy_zz<IView, RView>(zz, mapfs, iyp, ix) -
                   dzxy_zz<IView, RView>(zz, mapfs, iym, ix)) * sty;
  const Real gx = (tx * dpdx(iy - 1, ix - 1)) + (ty * dqdx(iy - 1, ix - 1));
  const Real gy = (tx * dpdy(iy - 1, ix - 1)) + (ty * dqdy(iy - 1, ix - 1));
  dzzdx(iy - 1, ix - 1) = gx * dfac;
  dzzdy(iy - 1, ix - 1) = gy * dfac;
}

/// One point of the tripole row IY = NY, w3updtmd.F90:3422-3484: the q+1
/// neighbour is the folded column NX-IX+1 of the same row.
template <class IView, class RView, class ZView, class OView>
KOKKOS_INLINE_FUNCTION void w3dzxy_trpl_point(int ix, int nx, int ny, Real dfac,
                                              const ZView& zz, const IView& mapsta,
                                              const IView& mapfs, const RView& dpdx,
                                              const RView& dpdy, const RView& dqdx,
                                              const RView& dqdy, const OView& dzzdx,
                                              const OView& dzzdy) {
  const int iy = ny;  // :3424
  if (mapsta(iy - 1, ix - 1) == 0) return;  // :3426
  Real stx = Real(0.5);
  const int ixps = (ix == nx) ? 1 : ix + 1;  // :3430-3434
  int ixp;
  if (mapsta(iy - 1, ixps - 1) == 0) {
    ixp = ix;
    stx = Real(1.0);
  } else {
    ixp = ixps;
  }
  const int ixms = (ix == 1) ? nx : ix - 1;  // :3442-3446
  int ixm;
  if (mapsta(iy - 1, ixms - 1) == 0) {
    ixm = ix;
    stx = Real(1.0);
  } else {
    ixm = ixms;
  }
  Real sty = Real(0.5);
  const int ixtrpls = nx - ix + 1;  // :3458
  int ixtrpl;
  if (mapsta(iy - 1, ixtrpls - 1) == 0) {
    ixtrpl = ix;
    sty = Real(1.0);
  } else {
    ixtrpl = ixtrpls;
  }
  const int iyms = iy - 1;  // :3466
  int iym;
  if (mapsta(iyms - 1, ix - 1) == 0) {
    iym = iy;
    sty = Real(1.0);
  } else {
    iym = iyms;
  }
  // :3475-3480
  const Real tx = (dzxy_zz<IView, RView>(zz, mapfs, iy, ixp) -
                   dzxy_zz<IView, RView>(zz, mapfs, iy, ixm)) * stx;
  const Real ty = (dzxy_zz<IView, RView>(zz, mapfs, iy, ixtrpl) -
                   dzxy_zz<IView, RView>(zz, mapfs, iym, ix)) * sty;
  const Real gx = (tx * dpdx(iy - 1, ix - 1)) + (ty * dqdx(iy - 1, ix - 1));
  const Real gy = (tx * dpdy(iy - 1, ix - 1)) + (ty * dqdy(iy - 1, ix - 1));
  dzzdx(iy - 1, ix - 1) = gx * dfac;
  dzzdy(iy - 1, ix - 1) = gy * dfac;
}

/// W3DZXY(ZZ, ZUNIT, DZZDX, DZZDY). zz is ZZ(NSEA); the 2-D Views are (NY,NX)
/// LayoutLeft, Fortran order. flagll, iclose are the module values of W3GDATMD.
///
/// Why parallel gives the serial bits: each (IY,IX) writes only DZZDX(IY,IX) and
/// DZZDY(IY,IX), reads only inputs (ZZ, MAPSTA, MAPFS, DPDX..DQDY), never an output,
/// and the interior rows 2..NY-1 and the tripole row NY are disjoint. There is no
/// reduction, so the order in which points run cannot change a result.
template <class ExecSpace>
void w3dzxy(const ExecSpace& exec, Kokkos::View<const Real*, ExecSpace> zz,
            Kokkos::View<const int**, Kokkos::LayoutLeft, ExecSpace> mapsta,
            Kokkos::View<const int**, Kokkos::LayoutLeft, ExecSpace> mapfs,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> dpdx,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> dpdy,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> dqdx,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> dqdy, bool flagll,
            int iclose, Kokkos::View<Real**, Kokkos::LayoutLeft, ExecSpace> dzzdx,
            Kokkos::View<Real**, Kokkos::LayoutLeft, ExecSpace> dzzdy) {
  using IView = Kokkos::View<const int**, Kokkos::LayoutLeft, ExecSpace>;
  using RView = Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace>;
  const int ny = static_cast<int>(dzzdx.extent(0));
  const int nx = static_cast<int>(dzzdx.extent(1));

  // :3339-3340, a memset in the GIMPLE: +0.0 everywhere, land points stay 0.
  Kokkos::deep_copy(exec, dzzdx, Real(0));
  Kokkos::deep_copy(exec, dzzdy, Real(0));

  const Real dfac = flagll ? kDfacLL : Real(1.0);  // :3345-3349

  // :3356-3362
  const int ixstart = (iclose == kIcloseNone) ? 2 : 1;
  const int ixend = (iclose == kIcloseNone) ? nx - 1 : nx;

  // :3364-3416, IY = 2..NY-1, IX = IXSTART..IXEND (Fortran bounds, inclusive).
  if (ny - 1 >= 2 && ixend >= ixstart) {
    Kokkos::parallel_for(
        "ww::updt::w3dzxy",
        Kokkos::MDRangePolicy<ExecSpace, Kokkos::Rank<2>>(exec, {2, ixstart},
                                                          {ny, ixend + 1}),
        KOKKOS_LAMBDA(const int iy, const int ix) {
          w3dzxy_point<IView, RView>(iy, ix, nx, dfac, zz, mapsta, mapfs, dpdx, dpdy, dqdx,
                                     dqdy, dzzdx, dzzdy);
        });
  }

  // :3422-3484
  if (iclose == kIcloseTrpl && ny >= 2) {
    Kokkos::parallel_for(
        "ww::updt::w3dzxy_trpl", Kokkos::RangePolicy<ExecSpace>(exec, 1, nx + 1),
        KOKKOS_LAMBDA(const int ix) {
          w3dzxy_trpl_point<IView, RView>(ix, nx, ny, dfac, zz, mapsta, mapfs, dpdx, dpdy,
                                          dqdx, dqdy, dzzdx, dzzdy);
        });
  }
}

}  // namespace ww::updt
