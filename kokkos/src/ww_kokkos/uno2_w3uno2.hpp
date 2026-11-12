// kokkos/src/ww_kokkos/uno2_w3uno2.hpp
// W3UNO2, W3UNO2R and W3UNO2S, ported to Kokkos: one-dimensional second-order UNO2
// propagation of a quantity Q along the lines of a 2-D field (W3UNO2 for any
// distance between points, W3UNO2R and W3UNO2S for a regular grid, W3UNO2S with the
// sub-grid transparencies TRANS). A flux FLA at each cell face (3rd-order central
// scheme with a minmod-type limiter for the interior points, 1st-order for the points
// next to a boundary), then Q = MAX(0., Q + FLA(IXY-INC) - FLA(IXY)...). Phase 1,
// translated, not wired, not regtested.
//
// WW3 heritage: SUBROUTINE W3UNO2 (WW3/model/src/w3uno2md.F90:102-446), W3UNO2r
// (:472-805) and W3UNO2s (:836-1224) at h0ffmann/WW3 96892c95 (byte-identical to the
// NOAA-EMC/WW3 03adda91 source: git -C WW3 diff 03adda91 HEAD --
// model/src/w3uno2md.F90 is empty); author Jain-Guo Li (W3UNO2, 1-Jul-2013), the
// others by the same scheme on a regular grid. Called by W3XYP2 (w3pro2md.F90:1020-1048, W3UNO2s) and
// W3KTP2 (w3pro2md.F90:1625 and :1675 W3UNO2, :1640 and :1656 W3UNO2r). Evidence:
// kokkos/port_report/W3UNO2_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement the switch of mww3_test_01 (switch_PR3_UNO, and
// switch_PR2_UNO: the preprocessed module is the same, only W3_PR2/W3_PR3 and W3_UNO
// differ and the module does not test them) compiles. Not compiled and not ported:
// W3_S (STRACE), W3_T, W3_T0, W3_T1, W3_T2 (test prints and their locals) and
// W3_OMPH (empty #ifdef pairs, no statement: :998, :1009, :1023, :1065, :1138, :1173).
// NDSE and NDST are only used by the test prints and are not in the argument lists.
//
// Index conventions. The Fortran dummies have lower bounds other than 1; a Kokkos
// View is 0-based, so an element with Fortran index i is:
//   VELO(MY*(MX+1)), DX1(MY*(MX+1)), CFLL(MY*(MX+1))   velo(i-1)
//   DX2(1-MY:MY*(MX+1)), Q(1-MY:MY*(MX+2)), FLA(1-MY:MY*MX)   x(i+MY-1)
//   TRANS(MY*MX,-1:1)                                  trans((i-1) + (j+1)*MY*MX)
//   MAPACT(MY*MX), MAPBOU(MY*MX)                       map(ip-1); the values are
//                                                      1-based IXY, used as is
// FLA is an automatic array of the Fortran (malloc in the object); here the caller's
// work View, MY*MX+MY long, zeroed by the launcher (FLA = 0.).
//
// Arithmetic, as gfortran 16.2 -O3 compiles it (WW3's Release flags, -ffp-contract=off,
// switch_PR2_UNO; the recompiled object's disassembly equals the CMake build's),
// read from the GIMPLE (-fdump-tree-optimized-lineno) and the assembly (v, 2026-11-12):
//  - SIGN(1.1,X) is copysignf(1.10000002384185791015625, X), by the sign bit alone;
//    INT(MIN(0.,SIGN(1.1,X))) is minss (S<0 ? S : 0) then cvttss2si: -1 for a negative
//    sign bit (including -0. and a NaN with the sign bit), else 0. INT(SIGN(1.1,X)) is
//    +1 or -1. Both are exact small integers.
//  - MIN(A,B) of two REALs is minss with A first: A<B ? A : B, so a NaN or a tie gives
//    B (W3UNO2 :286-288, W3UNO2R :652 and W3UNO2S :1037 on the two |dQ|). MAX(0.,X) is maxss
//    with X first: X>0. ? X : 0., so a NaN or -0. gives +0. (the last loop of each).
//  - W3UNO2 :286-288: QB = Q(IXYC) + ((SIGN(0.5,Q(IXYD)-Q(IXYC))*(DX1(IXYC)-ABS(CFL))) *
//    MIN(ABS(DQ+)/DX2(IXYC), ABS(DQ-)/DX2(IXYC-INC))): two divss, one subss, two mulss,
//    one addss. W3UNO2R and W3UNO2S the same with (1.0-ABS(CFL)) and no division.
//  - VEL = 0.5*(VELO(IXY)+VELO(IXY+INC)) (:275), the sum kept by the parentheses
//    (PAREN_EXPR); CFL = DT*VEL; FLA = CFL*QB (:294). The one-sided fluxes are
//    (VEL*DT)*Q(IXYC) in W3UNO2 (:326, :353) and CFL*Q in W3UNO2R/S.
//  - Last loop, W3UNO2 (:391): MAX(0., Q + (FLA(IXY-INC)-FLA(IXY))/DX1(IXY)), the
//    division first, then the add; W3UNO2R (:752): (Q+FLA(IXY-INC))-FLA(IXY); W3UNO2S
//    (:1163): (Q+TRANS(IXY,JN)*FLA(IXY-INC))-TRANS(IXY,JP)*FLA(IXY), JN = -1 if FLA(IXY-INC)>0.
//    else 0, JP = 1 if FLA(IXY)<0. else 0 (ordered compares: a NaN gives 0).
//  - No FMA in any of the three objects (vfmadd: 0).
//
// Why the parallel launch gives the serial bits. A loop over the list IP reads Q, VELO,
// DX1, DX2 and CFLL, which no statement of the loop writes, and writes FLA(IXY) once
// for each entry of MAPBOU; the last loop reads FLA, DX1 and TRANS and writes Q(IXY)
// of its own entry only. The loops run one after the other on one execution space
// instance, in the Fortran's order, so a point in two lists keeps the last write.
// ⚠ Preconditions the Fortran does not state and the C++ does not check: the entries
// of MAPACT are all different (W3MAP2 builds MAPAXY with one entry per cell, a repeated
// entry would apply the update twice in the Fortran and race here), and the entries
// of one MAPBOU list are different or write the same value (the value depends only on
// Q, which the loop does not write). The closure loop over IY (BCLOSE) does its
// statements in the Fortran's order for each IY, and distinct IY touch distinct
// elements (checked for NX = 1 as well, where Q(IY+IADN1) is read by the next
// statement of the same IY).
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::uno2 {

namespace uno2_detail {

/// MIN(A,B) as minss with A first: a NaN or a tie gives B.
KOKKOS_INLINE_FUNCTION Real min_ss(Real a, Real b) { return (a < b) ? a : b; }

/// MAX(A,B) as maxss with A first: a NaN or a tie gives B.
KOKKOS_INLINE_FUNCTION Real max_ss(Real a, Real b) { return (a > b) ? a : b; }

/// SIGN(1.1,X): the REAL 1.1 with X's sign bit.
KOKKOS_INLINE_FUNCTION Real sign11(Real x) { return Kokkos::copysign(1.10000002384185791015625F, x); }

/// INT( MIN ( 0. , SIGN(1.1,X) ) ): -1 or 0.
KOKKOS_INLINE_FUNCTION int int_min0_sign11(Real x) {
  const Real s = sign11(x);
  return static_cast<int>(min_ss(s, 0.0F));
}

/// INT( SIGN (1.1,X) ): -1 or +1.
KOKKOS_INLINE_FUNCTION int int_sign11(Real x) { return static_cast<int>(sign11(x)); }

/// FLA = 0. over the whole FLA(1-MY:MY*MX).
template <class ExecSpace>
void zero_fla(const ExecSpace& exec, int mx, int my, Kokkos::View<Real*, ExecSpace> fla) {
  const int n = my * mx + my;
  if (n <= 0) return;
  Kokkos::parallel_for(
      "ww::uno2::fla_zero", Kokkos::RangePolicy<ExecSpace>(exec, 0, n),
      KOKKOS_LAMBDA(const int i) { fla(i) = 0.0F; });
}

}  // namespace uno2_detail

/// W3UNO2 (w3uno2md.F90:102-446). Views are 0-based, see the header for the offsets.
/// velo and q are INOUT only for the closure (BCLOSE); dx1 and dx2 too.
template <class ExecSpace>
void w3uno2(const ExecSpace& exec, int mx, int my, int nx, int ny, Kokkos::View<Real*, ExecSpace> velo,
            Real dt, Kokkos::View<Real*, ExecSpace> dx1, Kokkos::View<Real*, ExecSpace> dx2,
            Kokkos::View<Real*, ExecSpace> q, bool bclose, int inc,
            Kokkos::View<const int*, ExecSpace> mapact, int nact,
            Kokkos::View<const int*, ExecSpace> mapbou, int nb0, int nb1, int nb2,
            Kokkos::View<Real*, ExecSpace> fla) {
  using namespace uno2_detail;
  const int o = my - 1;  // offset of the (1-MY:...) arrays

  zero_fla(exec, mx, my, fla);  // :242

  const int iad00 = -my;  // :248-252
  const int iad02 = my;
  const int iadn0 = iad00 + my * nx;
  const int iadn1 = my * nx;
  const int iadn2 = iad02 + my * nx;
  if (bclose && ny > 0) {  // :244-262
    Kokkos::parallel_for(
        "ww::uno2::w3uno2_close1", Kokkos::RangePolicy<ExecSpace>(exec, 1, ny + 1),
        KOKKOS_LAMBDA(const int iy) {
          q(iy + iad00 + o) = q(iy + iadn0 + o);
          q(iy + iadn1 + o) = q(iy + o);
          q(iy + iadn2 + o) = q(iy + iad02 + o);
          velo(iy + iadn1 - 1) = velo(iy - 1);
          dx1(iy + iadn1 - 1) = dx1(iy - 1);
          dx2(iy + iad00 + o) = dx1(iy + iadn0 - 1);
          dx2(iy + iadn1 + o) = dx1(iy - 1);
        });
  }

  if (nb0 > 0) {  // :272-296
    Kokkos::parallel_for(
        "ww::uno2::w3uno2_central", Kokkos::RangePolicy<ExecSpace>(exec, 0, nb0),
        KOKKOS_LAMBDA(const int ip) {
          const int ixy = mapbou(ip);
          const Real vel = 0.5F * (velo(ixy - 1) + velo(ixy + inc - 1));
          const Real cfl = dt * vel;
          const int ixyc = ixy - inc * int_min0_sign11(cfl);
          const int ixyd = ixyc + inc * int_sign11(cfl);
          const Real qc = q(ixyc + o);
          const Real dqp = Kokkos::abs(q(ixyc + inc + o) - qc) / dx2(ixyc + o);
          const Real dqm = Kokkos::abs(qc - q(ixyc - inc + o)) / dx2(ixyc - inc + o);
          const Real lim = min_ss(dqp, dqm);
          const Real qb = qc + (Kokkos::copysign(0.5F, q(ixyd + o) - qc) *
                                (dx1(ixyc - 1) - Kokkos::abs(cfl))) *
                                   lim;
          fla(ixy + o) = cfl * qb;
        });
  }

  if (nb1 > nb0) {  // :322-340
    Kokkos::parallel_for(
        "ww::uno2::w3uno2_above", Kokkos::RangePolicy<ExecSpace>(exec, nb0, nb1),
        KOKKOS_LAMBDA(const int ip) {
          const int ixy = mapbou(ip);
          const Real vel = velo(ixy - 1);
          const int ixyc = ixy - inc * int_min0_sign11(vel);
          fla(ixy + o) = vel * dt * q(ixyc + o);
        });
  }

  if (nb2 > nb1) {  // :349-367
    Kokkos::parallel_for(
        "ww::uno2::w3uno2_below", Kokkos::RangePolicy<ExecSpace>(exec, nb1, nb2),
        KOKKOS_LAMBDA(const int ip) {
          const int ixy = mapbou(ip);
          const Real vel = velo(ixy + inc - 1);
          const int ixyc = ixy - inc * int_min0_sign11(vel);
          fla(ixy + o) = vel * dt * q(ixyc + o);
        });
  }

  if (bclose && ny > 0) {  // :371-377
    Kokkos::parallel_for(
        "ww::uno2::w3uno2_close2", Kokkos::RangePolicy<ExecSpace>(exec, 1, ny + 1),
        KOKKOS_LAMBDA(const int iy) { fla(iy + iad00 + o) = fla(iy + iadn0 + o); });
  }

  if (nact > 0) {  // :385-392
    Kokkos::parallel_for(
        "ww::uno2::w3uno2_propagate", Kokkos::RangePolicy<ExecSpace>(exec, 0, nact),
        KOKKOS_LAMBDA(const int ip) {
          const int ixy = mapact(ip);
          q(ixy + o) = max_ss(q(ixy + o) + (fla(ixy - inc + o) - fla(ixy + o)) / dx1(ixy - 1), 0.0F);
        });
  }
}

/// W3UNO2r (w3uno2md.F90:472-805), the regular-grid scheme: the Courant number CFLL
/// replaces VELO*DT/DX1 and the limiter works on the differences of Q alone.
template <class ExecSpace>
void w3uno2r(const ExecSpace& exec, int mx, int my, int nx, int ny, Kokkos::View<Real*, ExecSpace> cfll,
             Kokkos::View<Real*, ExecSpace> q, bool bclose, int inc,
             Kokkos::View<const int*, ExecSpace> mapact, int nact,
             Kokkos::View<const int*, ExecSpace> mapbou, int nb0, int nb1, int nb2,
             Kokkos::View<Real*, ExecSpace> fla) {
  using namespace uno2_detail;
  const int o = my - 1;

  zero_fla(exec, mx, my, fla);  // :619

  const int iad00 = -my;  // :625-629
  const int iad02 = my;
  const int iadn0 = iad00 + my * nx;
  const int iadn1 = my * nx;
  const int iadn2 = iad02 + my * nx;
  if (bclose && ny > 0) {  // :621-635
    Kokkos::parallel_for(
        "ww::uno2::w3uno2r_close1", Kokkos::RangePolicy<ExecSpace>(exec, 1, ny + 1),
        KOKKOS_LAMBDA(const int iy) {
          q(iy + iad00 + o) = q(iy + iadn0 + o);
          q(iy + iadn1 + o) = q(iy + o);
          q(iy + iadn2 + o) = q(iy + iad02 + o);
          cfll(iy + iadn1 - 1) = cfll(iy - 1);
        });
  }

  if (nb0 > 0) {  // :646-660
    Kokkos::parallel_for(
        "ww::uno2::w3uno2r_central", Kokkos::RangePolicy<ExecSpace>(exec, 0, nb0),
        KOKKOS_LAMBDA(const int ip) {
          const int ixy = mapbou(ip);
          const Real cfl = 0.5F * (cfll(ixy - 1) + cfll(ixy + inc - 1));
          const int ixyc = ixy - inc * int_min0_sign11(cfl);
          const int ixyd = ixyc + inc * int_sign11(cfl);
          const Real qc = q(ixyc + o);
          const Real dqp = Kokkos::abs(q(ixyc + inc + o) - qc);
          const Real dqm = Kokkos::abs(qc - q(ixyc - inc + o));
          const Real lim = min_ss(dqp, dqm);
          const Real qb = qc + (Kokkos::copysign(0.5F, q(ixyd + o) - qc) *
                                (1.0F - Kokkos::abs(cfl))) *
                                   lim;
          fla(ixy + o) = cfl * qb;
        });
  }

  if (nb1 > nb0) {  // :686-703
    Kokkos::parallel_for(
        "ww::uno2::w3uno2r_above", Kokkos::RangePolicy<ExecSpace>(exec, nb0, nb1),
        KOKKOS_LAMBDA(const int ip) {
          const int ixy = mapbou(ip);
          const Real cfl = cfll(ixy - 1);
          const int ixyc = ixy - inc * int_min0_sign11(cfl);
          fla(ixy + o) = cfl * q(ixyc + o);
        });
  }

  if (nb2 > nb1) {  // :712-729
    Kokkos::parallel_for(
        "ww::uno2::w3uno2r_below", Kokkos::RangePolicy<ExecSpace>(exec, nb1, nb2),
        KOKKOS_LAMBDA(const int ip) {
          const int ixy = mapbou(ip);
          const Real cfl = cfll(ixy + inc - 1);
          const int ixyc = ixy - inc * int_min0_sign11(cfl);
          fla(ixy + o) = cfl * q(ixyc + o);
        });
  }

  if (bclose && ny > 0) {  // :733-739
    Kokkos::parallel_for(
        "ww::uno2::w3uno2r_close2", Kokkos::RangePolicy<ExecSpace>(exec, 1, ny + 1),
        KOKKOS_LAMBDA(const int iy) { fla(iy + iad00 + o) = fla(iy + iadn0 + o); });
  }

  if (nact > 0) {  // :747-758
    Kokkos::parallel_for(
        "ww::uno2::w3uno2r_propagate", Kokkos::RangePolicy<ExecSpace>(exec, 0, nact),
        KOKKOS_LAMBDA(const int ip) {
          const int ixy = mapact(ip);
          q(ixy + o) = max_ss((q(ixy + o) + fla(ixy - inc + o)) - fla(ixy + o), 0.0F);
        });
  }
}

/// W3UNO2s (w3uno2md.F90:836-1224): W3UNO2r with the sub-grid transparencies
/// TRANS(MY*MX,-1:1) on the two fluxes of the last loop, trans = TRANS(IXY,J) at
/// trans((IXY-1) + (J+1)*MY*MX).
template <class ExecSpace>
void w3uno2s(const ExecSpace& exec, int mx, int my, int nx, int ny, Kokkos::View<Real*, ExecSpace> cfll,
             Kokkos::View<const Real*, ExecSpace> trans, Kokkos::View<Real*, ExecSpace> q,
             bool bclose, int inc, Kokkos::View<const int*, ExecSpace> mapact, int nact,
             Kokkos::View<const int*, ExecSpace> mapbou, int nb0, int nb1, int nb2,
             Kokkos::View<Real*, ExecSpace> fla) {
  using namespace uno2_detail;
  const int o = my - 1;
  const int nmxy = my * mx;  // the first extent of TRANS

  zero_fla(exec, mx, my, fla);  // :986

  const int iad00 = -my;  // :992-996
  const int iad02 = my;
  const int iadn0 = iad00 + my * nx;
  const int iadn1 = my * nx;
  const int iadn2 = iad02 + my * nx;
  if (bclose && ny > 0) {  // :988-1008
    Kokkos::parallel_for(
        "ww::uno2::w3uno2s_close1", Kokkos::RangePolicy<ExecSpace>(exec, 1, ny + 1),
        KOKKOS_LAMBDA(const int iy) {
          q(iy + iad00 + o) = q(iy + iadn0 + o);
          q(iy + iadn1 + o) = q(iy + o);
          q(iy + iadn2 + o) = q(iy + iad02 + o);
          cfll(iy + iadn1 - 1) = cfll(iy - 1);
        });
  }

  if (nb0 > 0) {  // :1031-1046
    Kokkos::parallel_for(
        "ww::uno2::w3uno2s_central", Kokkos::RangePolicy<ExecSpace>(exec, 0, nb0),
        KOKKOS_LAMBDA(const int ip) {
          const int ixy = mapbou(ip);
          const Real cfl = 0.5F * (cfll(ixy - 1) + cfll(ixy + inc - 1));
          const int ixyc = ixy - inc * int_min0_sign11(cfl);
          const int ixyd = ixyc + inc * int_sign11(cfl);
          const Real qc = q(ixyc + o);
          const Real dqp = Kokkos::abs(q(ixyc + inc + o) - qc);
          const Real dqm = Kokkos::abs(qc - q(ixyc - inc + o));
          const Real lim = min_ss(dqp, dqm);
          const Real qb = qc + (Kokkos::copysign(0.5F, q(ixyd + o) - qc) *
                                (1.0F - Kokkos::abs(cfl))) *
                                   lim;
          fla(ixy + o) = cfl * qb;
        });
  }

  if (nb1 > nb0) {  // :1076-1093
    Kokkos::parallel_for(
        "ww::uno2::w3uno2s_above", Kokkos::RangePolicy<ExecSpace>(exec, nb0, nb1),
        KOKKOS_LAMBDA(const int ip) {
          const int ixy = mapbou(ip);
          const Real cfl = cfll(ixy - 1);
          const int ixyc = ixy - inc * int_min0_sign11(cfl);
          fla(ixy + o) = cfl * q(ixyc + o);
        });
  }

  if (nb2 > nb1) {  // :1102-1119
    Kokkos::parallel_for(
        "ww::uno2::w3uno2s_below", Kokkos::RangePolicy<ExecSpace>(exec, nb1, nb2),
        KOKKOS_LAMBDA(const int ip) {
          const int ixy = mapbou(ip);
          const Real cfl = cfll(ixy + inc - 1);
          const int ixyc = ixy - inc * int_min0_sign11(cfl);
          fla(ixy + o) = cfl * q(ixyc + o);
        });
  }

  if (bclose && ny > 0) {  // :1123-1129
    Kokkos::parallel_for(
        "ww::uno2::w3uno2s_close2", Kokkos::RangePolicy<ExecSpace>(exec, 1, ny + 1),
        KOKKOS_LAMBDA(const int iy) { fla(iy + iad00 + o) = fla(iy + iadn0 + o); });
  }

  if (nact > 0) {  // :1146-1168
    Kokkos::parallel_for(
        "ww::uno2::w3uno2s_propagate", Kokkos::RangePolicy<ExecSpace>(exec, 0, nact),
        KOKKOS_LAMBDA(const int ip) {
          const int ixy = mapact(ip);
          const Real fn = fla(ixy - inc + o);
          const Real fp = fla(ixy + o);
          const int jn = (fn > 0.0F) ? -1 : 0;  // :1149-1153
          const int jp = (fp < 0.0F) ? 1 : 0;   // :1154-1158
          q(ixy + o) = max_ss((q(ixy + o) + trans((ixy - 1) + (jn + 1) * nmxy) * fn) -
                                  trans((ixy - 1) + (jp + 1) * nmxy) * fp,
                              0.0F);
        });
  }
}

}  // namespace ww::uno2
