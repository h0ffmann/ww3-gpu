// kokkos/src/ww_kokkos/prop_qck1.hpp
// W3QCK1, ported to Kokkos: ULTIMATE QUICKEST, 1-D propagation on a regular
// 2-D field (Leonard 1979, 1991). Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3uqckmd.F90:117-480 at NOAA-EMC/WW3 03adda91;
// author H. L. Tolman. W3_S and W3_T* output (STRACE, the test prints, the IENT
// SAVE) are not ported: nothing of them survives switch_lab_shrd, and a kernel
// never prints.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and assembly of
// w3uqckmd.F90 (WW3's own CMake Release flags, switch_lab_shrd): no libm call, no
// libmvec symbol, no FMA, no reduction; SIGN is __builtin_copysignf, ABS a mask,
// INT a cvttss2si truncation, `/6.` a true division, `**2` one multiply. The
// central loop is vectorised four points at a time with scalar gathers, which
// changes no bits; the MIN/MAX operand orders below are the ones the maxss/minss
// and maxps/minps instructions emit (spec_W3QCK1.md, section 3).
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"  // fint, max_r, min_r

namespace ww::prop {

/// Reals of work storage w3qck1 needs: the automatic FLA(1-MY:MY*MX),
/// MY*(MX+1) entries (w3uqckmd.F90:171), heap-allocated and freed by the
/// Fortran on every call. The port takes it from the caller.
KOKKOS_INLINE_FUNCTION constexpr int qck1_work_reals(int mx, int my) { return my * (mx + 1); }

/// W3QCK1(MX, MY, NX, NY, CFLL, Q, CLOSE, INC, MAPACT, NACT, MAPBOU, NB0, NB1,
/// NB2, NDSE, NDST) for one field. NDSE and NDST are never read in this build
/// (w3uqckmd.F90:124; their only uses are under W3_T) and are left out.
///
/// Fortran arrays arrive 0-based; the explicit lower bounds of :164-165 become
/// an offset:
///   cfll   -> CFLL(1 : MY*(MX+1)):   CFLL(i) = cfll[i - 1]
///   q      -> Q(1-MY : MY*(MX+2)):   Q(i)    = q[i + my - 1]
///   mapact -> MAPACT(1:NACT), mapbou -> MAPBOU(1:NB2): values are 1-based IXY,
///             storage 0-based (only the first NACT and NB2 entries are read)
///   fla    -> FLA(1-MY : MY*MX):     FLA(i)  = fla[i + my - 1], qck1_work_reals
///             entries, caller-owned, zeroed here (:281)
/// cfll and q are INOUT: with close, columns 0, NX+1 and NX+2 of Q and column
/// NX+1 of CFLL are overwritten in place, over whole columns IY = 1..NY, so a
/// later Fortran reader of those columns sees the same bytes (:283-297).
/// The three MAPBOU loops keep their Fortran meaning even though the lab caller
/// (W3KTP3, w3pro3md.F90:1885 and :1901) passes nb0 = nb1 = nb2 = NSPEC.
///
/// No reduction: each MAPBOU or MAPACT iteration writes one element and reads
/// only Q, CFLL and FLA, so the loops compute the same bits serial or parallel
/// when the maps are injective (MAPTH2 is). Phase 1 keeps them serial.
KOKKOS_INLINE_FUNCTION void w3qck1(int mx, int my, int nx, int ny, Real* cfll, Real* q,
                                   bool close, int inc, const int* mapact, int nact,
                                   const int* mapbou, int nb0, int nb1, int nb2, Real* fla) {
  using ww::st4::fint;
  using ww::st4::max_r;
  using ww::st4::min_r;
  constexpr Real r0 = static_cast<Real>(0.);
  constexpr Real r05 = static_cast<Real>(0.5);
  constexpr Real r1 = static_cast<Real>(1.);
  constexpr Real r2 = static_cast<Real>(2.);
  constexpr Real r6 = static_cast<Real>(6.);
  constexpr Real r11 = static_cast<Real>(1.1);
  constexpr Real rm01 = static_cast<Real>(-0.1);
  constexpr Real r1em15 = static_cast<Real>(1.E-15);
  constexpr Real r1em10 = static_cast<Real>(1.E-10);

  // Offsets of the explicit lower bounds: Q and FLA start at 1-MY, CFLL at 1.
  const int qoff = my - 1;
  const int foff = my - 1;

  // :281, FLA = 0. over the whole 1-MY..MY*MX range: the propagation reads
  // FLA(IXY-INC) of the lowest column, which stays 0 without closure.
  const int nfla = qck1_work_reals(mx, my);
  for (int i = 0; i < nfla; ++i) fla[i] = r0;

  // :283-297, closure part 1. Serial, statements in source order inside one IY,
  // because columns may alias when NX < 3.
  const int iad00 = -my;
  const int iad02 = my;
  const int iadn0 = iad00 + my * nx;
  const int iadn1 = my * nx;
  const int iadn2 = iad02 + my * nx;
  if (close) {
    for (int iy = 1; iy <= ny; ++iy) {
      q[iy + iad00 + qoff] = q[iy + iadn0 + qoff];
      q[iy + iadn1 + qoff] = q[iy + qoff];
      q[iy + iadn2 + qoff] = q[iy + iad02 + qoff];
      cfll[iy + iadn1 - 1] = cfll[iy - 1];
    }
  }

  // :308-333, central points.
  for (int ip = 1; ip <= nb0; ++ip) {
    const int ixy = mapbou[ip - 1];
    // :311. The sum is commutative; -O3 adds CFLL(IXY+INC) + CFLL(IXY).
    const Real cfl = (cfll[ixy + inc - 1] + cfll[ixy - 1]) * r05;
    // :312. SIGN(1.1,CFL) is copysign: CFL = -0.0 gives -1.1 and selects the
    // upwind cell on the other side; never a CFL < 0 test.
    const Real s = Kokkos::copysign(r11, cfl);
    // M1: MIN_EXPR <s, 0.0>, minss 0,s in the vector body and epilogue 1-2; the
    // scalar fallback (NB0 < 4) and epilogue 3 swap the operands, which could
    // only matter for a NaN s, and s is +-1.1 for every CFL, NaN included.
    // INT truncates: 0 for +1.1, -1 for -1.1.
    const int ixyc = ixy - inc * fint(min_r(s, r0));
    // :314-315: ((1.-CFL)*Q(IXY+INC) + (1.+CFL)*Q(IXY)) * 0.5, minus
    // (1.-CFL**2)/6. times the three-point stencil; the adds are commutative
    // and the vector body and the scalar epilogue swap them.
    const Real c2 = cfl * cfl;
    const Real t2 = (r1 - cfl) * q[ixy + inc + qoff];
    const Real t4 = (cfl + r1) * q[ixy + qoff];
    const Real t6 = (t4 + t2) * r05;
    const Real t8 = (r1 - c2) / r6;
    const Real qixyc = q[ixyc + qoff];
    const Real t9 = qixyc * r2;
    const Real t11 = q[ixyc + inc + qoff] + (q[ixyc - inc + qoff] - t9);
    const Real qb = t6 - t11 * t8;
    // :316-317. IXYD in 32-bit integer arithmetic.
    const int ixyu = ixyc - inc * fint(s);
    const int ixyd = 2 * ixyc - ixyu;
    const Real qixyu = q[ixyu + qoff];
    // :318-319. M2: MAX_EXPR <ABS(DQ), 1.E-15>, maxss 1e-15,abs.
    const Real dq = q[ixyd + qoff] - qixyu;
    const Real dqnz = Kokkos::copysign(max_r(Kokkos::fabs(dq), r1em15), dq);
    // :320-321. M3a: MAX_EXPR <qcn, -0.1>; M3b: MIN_EXPR <m, 1.1>.
    const Real qcn = min_r(max_r((qixyc - qixyu) / dqnz, rm01), r11);
    // :326. M4: MAX_EXPR <r2, qcn>, maxss qcn,r2: a NaN quotient keeps QCN.
    const Real qbn0 = max_r((qb - qixyu) / dqnz, qcn);
    // :327. -O3 reassociates MIN(QBN, 1., c) into MIN(QBN0, MIN(c, 1.)):
    // M5a: MAX_EXPR <ABS(CFL), 1.E-10>; M5b: MIN_EXPR <c, 1.0>; M5c:
    // MIN_EXPR <t13, qbn0>, minss t13,qbn0.
    const Real t13 = min_r(qcn / max_r(Kokkos::fabs(cfl), r1em10), r1);
    const Real qbn = min_r(qbn0, t13);
    // :328. QBN*DQ then + Q(IXYU).
    const Real qbr = qbn * dq + qixyu;
    // :329. CFAC = REAL(INT(2.*ABS(QCN-0.5))): 0. or 1. for QCN in [-0.1,1.1].
    const Real cfac = static_cast<Real>(fint(Kokkos::fabs(qcn - r05) * r2));
    // :330. Two multiplies and one add even though CFAC is 0 or 1 (keeps -0.0
    // and NaN as the Fortran does); -O3 adds CFAC*Q(IXYC) + (1.-CFAC)*QBR.
    const Real qb2 = cfac * qixyc + (r1 - cfac) * qbr;
    // :332.
    fla[ixy + foff] = qb2 * cfl;
  }

  // :359-364, boundary above: CFL from CFLL(IXY). M7: MIN_EXPR <s, 0.0>.
  for (int ip = nb0 + 1; ip <= nb1; ++ip) {
    const int ixy = mapbou[ip - 1];
    const Real cfl = cfll[ixy - 1];
    const Real s = Kokkos::copysign(r11, cfl);
    const int ixyc = ixy - inc * fint(min_r(s, r0));
    fla[ixy + foff] = q[ixyc + qoff] * cfl;
  }

  // :385-390, boundary below: CFL from CFLL(IXY+INC). M7 as above.
  for (int ip = nb1 + 1; ip <= nb2; ++ip) {
    const int ixy = mapbou[ip - 1];
    const Real cfl = cfll[ixy + inc - 1];
    const Real s = Kokkos::copysign(r11, cfl);
    const int ixyc = ixy - inc * fint(min_r(s, r0));
    fla[ixy + foff] = q[ixyc + qoff] * cfl;
  }

  // :406-412, closure part 2: FLA column 0 <- column NX, whole columns.
  if (close) {
    for (int iy = 1; iy <= ny; ++iy) fla[iy + iad00 + foff] = fla[iy + iadn0 + foff];
  }

  // :420-426, propagation. (Q + FLA(IXY-INC)) - FLA(IXY), then M6:
  // MAX_EXPR <x, 0.0>, maxss 0,x: -0.0 and NaN both become +0.0.
  for (int ip = 1; ip <= nact; ++ip) {
    const int ixy = mapact[ip - 1];
    const Real u = (q[ixy + qoff] + fla[ixy - inc + foff]) - fla[ixy + foff];
    q[ixy + qoff] = max_r(u, r0);
  }
}

}  // namespace ww::prop
