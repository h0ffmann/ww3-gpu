// kokkos/src/ww_kokkos/prop_qck2.hpp
// W3QCK2, ported to Kokkos: ULTIMATE QUICKEST, 1-D propagation on a regular
// 2-D field with variable grid spacing (W3QCK1 with DX1, DX2 band widths and a
// velocity instead of a Courant number). Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3uqckmd.F90:517-891 at NOAA-EMC/WW3 03adda91;
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
// and maxps/minps instructions emit (spec_W3QCK2.md, section 3).
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"  // fint, max_r, min_r

namespace ww::prop {

/// Reals of work storage w3qck2 needs: the automatic FLA(1-MY:MY*MX),
/// MY*(MX+1) entries (w3uqckmd.F90:630), heap-allocated and freed by the
/// Fortran on every call. The port takes it from the caller.
KOKKOS_INLINE_FUNCTION constexpr int qck2_work_reals(int mx, int my) { return my * (mx + 1); }

/// W3QCK2(MX, MY, NX, NY, VELO, DT, DX1, DX2, Q, CLOSE, INC, MAPACT, NACT,
/// MAPBOU, NB0, NB1, NB2, NDSE, NDST) for one field. NDSE and NDST are never
/// read in this build (their only uses are under W3_T) and are left out.
///
/// Fortran arrays arrive 0-based; the explicit lower bounds of :614-615 become
/// an offset:
///   velo   -> VELO(1 : MY*(MX+1)):   VELO(i) = velo[i - 1]; the lab caller passes
///             CFLK(NK2,NTH), MY*MX reals, with CLOSE false, so column MX+1 is
///             never touched (w3pro3md.F90:1677)
///   dx1    -> DX1(1 : MY*(MX+1)):    DX1(i)  = dx1[i - 1]
///   dx2    -> DX2(1-MY : MY*(MX+1)): DX2(i)  = dx2[i + my - 1]
///   q      -> Q(1-MY : MY*(MX+2)):   Q(i)    = q[i + my - 1]
///   mapact -> MAPACT(1:NACT), mapbou -> MAPBOU(1:NB2): values are 1-based IXY,
///             storage 0-based (only the first NACT and NB2 entries are read)
///   fla    -> FLA(1-MY : MY*MX):     FLA(i)  = fla[i + my - 1], qck2_work_reals
///             entries, caller-owned, zeroed here (:680)
/// velo, dx1, dx2 and q are INOUT: with close, columns 0, NX+1 and NX+2 of Q,
/// column NX+1 of VELO and DX1, and columns 0 and NX+1 of DX2 are overwritten
/// in place over whole columns IY = 1..NY (:686-699). The lab caller (W3KTP3,
/// w3pro3md.F90:1868 and :1919) passes close false, inc 1, dt = FACK,
/// nb0 = NSPEC-NTH, nb1 = NSPEC, nb2 = NSPEC+NTH, so all three MAPBOU loops run.
///
/// No reduction: each MAPBOU or MAPACT iteration writes one element and reads
/// only Q, VELO, DX1, DX2 and FLA, so the loops compute the same bits serial or
/// parallel when the maps are injective (MAPWN2 and MAPTH2 are). Phase 1 keeps
/// them serial.
KOKKOS_INLINE_FUNCTION void w3qck2(int mx, int my, int nx, int ny, Real* velo, Real dt,
                                   Real* dx1, Real* dx2, Real* q, bool close, int inc,
                                   const int* mapact, int nact, const int* mapbou, int nb0,
                                   int nb1, int nb2, Real* fla) {
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

  // Offset of the explicit lower bound 1-MY shared by DX2, Q and FLA; VELO and
  // DX1 start at 1.
  const int qoff = my - 1;

  // :680, FLA = 0. over the whole 1-MY..MY*MX range: the propagation reads
  // FLA(IXY-INC) of the lowest row, which stays 0 where no loop writes it.
  const int nfla = qck2_work_reals(mx, my);
  for (int i = 0; i < nfla; ++i) fla[i] = r0;

  // :682-700, closure part 1. Serial, statements in source order inside one IY,
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
      velo[iy + iadn1 - 1] = velo[iy - 1];
      dx1[iy + iadn1 - 1] = dx1[iy - 1];
      dx2[iy + iad00 + qoff] = dx1[iy + iadn0 - 1];
      dx2[iy + iadn1 + qoff] = dx1[iy - 1];
    }
  }

  // :710-756, central points.
  for (int ip = 1; ip <= nb0; ++ip) {
    const int ixy = mapbou[ip - 1];
    // :713. The sum is commutative; -O3 adds VELO(IXY+INC) + VELO(IXY).
    const Real vel = (velo[ixy + inc - 1] + velo[ixy - 1]) * r05;
    // :714. DT*VEL first, then the division by DX2(IXY).
    const Real dx2ixy = dx2[ixy + qoff];
    const Real cfl = (dt * vel) / dx2ixy;
    // :715. SIGN(1.1,CFL) is copysign: CFL = -0.0 gives -1.1 and selects the
    // upwind cell on the other side; never a CFL < 0 test.
    const Real s = Kokkos::copysign(r11, cfl);
    // M1: MIN_EXPR <s, 0.0>, minps 0,s in the vector body; the three epilogue
    // iterations and the scalar fallback (NB0 < 4) swap the operands, which could
    // only matter for a NaN s, and s is +-1.1 for every CFL, NaN included.
    // INT truncates: 0 for +1.1, -1 for -1.1.
    const int ixyc = ixy - inc * fint(min_r(s, r0));
    // :716-719, in the GIMPLE's order: 0.5*((1.+CFL)*Q(IXY) + (1.-CFL)*Q(IXY+INC))
    // minus ((DX2(IXY)**2 / DX1(IXYC)) * (1.-CFL**2)) / 6. times the
    // three-point curvature; the powers are one multiply each, /6. a division.
    const Real d2 = dx2ixy * dx2ixy;
    const Real c2 = cfl * cfl;
    const Real t2 = q[ixy + inc + qoff] * (r1 - cfl);
    const Real t4 = q[ixy + qoff] * (cfl + r1);
    const Real t6 = (t4 + t2) * r05;
    const Real t8 = ((r1 - c2) * (d2 / dx1[ixyc - 1])) / r6;
    const Real qixyc = q[ixyc + qoff];
    const Real u1 = (q[ixyc + inc + qoff] - qixyc) / dx2[ixyc + qoff];
    const Real u2 = (qixyc - q[ixyc - inc + qoff]) / dx2[ixyc - inc + qoff];
    const Real qb = t6 - (u1 - u2) * t8;
    // :721-722. IXYD in 32-bit integer arithmetic.
    const int ixyu = ixyc - inc * fint(s);
    const int ixyd = 2 * ixyc - ixyu;
    const Real qixyu = q[ixyu + qoff];
    // :723-724. M2: MAX_EXPR <ABS(DQ), 1.E-15>, maxss 1e-15,abs.
    const Real dq = q[ixyd + qoff] - qixyu;
    const Real dqnz = Kokkos::copysign(max_r(Kokkos::fabs(dq), r1em15), dq);
    // :725-726. M3a: MAX_EXPR <qcn, -0.1>; M3b: MIN_EXPR <m, 1.1>.
    const Real qcn = min_r(max_r((qixyc - qixyu) / dqnz, rm01), r11);
    // :731. M4: MAX_EXPR <r2, qcn>, maxss qcn,r2: a NaN quotient keeps QCN.
    const Real qbn0 = max_r((qb - qixyu) / dqnz, qcn);
    // :732. -O3 reassociates MIN(QBN, 1., c) into MIN(QBN0, MIN(c, 1.)):
    // M5a: MAX_EXPR <ABS(CFL), 1.E-10>; M5b: MIN_EXPR <c, 1.0>; M5c:
    // MIN_EXPR <t13, qbn0>, minss t13,qbn0.
    const Real t13 = min_r(qcn / max_r(Kokkos::fabs(cfl), r1em10), r1);
    const Real qbn = min_r(qbn0, t13);
    // :733. QBN*DQ then + Q(IXYU).
    const Real qbr = qbn * dq + qixyu;
    // :734. CFAC = REAL(INT(2.*ABS(QCN-0.5))): 0. or 1. for QCN in [-0.1,1.1].
    const Real cfac = static_cast<Real>(fint(Kokkos::fabs(qcn - r05) * r2));
    // :735. Two multiplies and one add even though CFAC is 0 or 1 (keeps -0.0
    // and NaN as the Fortran does); -O3 adds CFAC*Q(IXYC) + (1.-CFAC)*QBR.
    const Real qb2 = cfac * qixyc + (r1 - cfac) * qbr;
    // :737.
    fla[ixy + qoff] = qb2 * vel;
  }

  // :765-783, boundary above: VEL from VELO(IXY). M7: MIN_EXPR <s, 0.0>.
  for (int ip = nb0 + 1; ip <= nb1; ++ip) {
    const int ixy = mapbou[ip - 1];
    const Real vel = velo[ixy - 1];
    const Real s = Kokkos::copysign(r11, vel);
    const int ixyc = ixy - inc * fint(min_r(s, r0));
    fla[ixy + qoff] = q[ixyc + qoff] * vel;
  }

  // :792-810, boundary below: VEL from VELO(IXY+INC). M7 as above.
  for (int ip = nb1 + 1; ip <= nb2; ++ip) {
    const int ixy = mapbou[ip - 1];
    const Real vel = velo[ixy + inc - 1];
    const Real s = Kokkos::copysign(r11, vel);
    const int ixyc = ixy - inc * fint(min_r(s, r0));
    fla[ixy + qoff] = q[ixyc + qoff] * vel;
  }

  // :814-821, closure part 2: FLA column 0 <- column NX, whole columns.
  if (close) {
    for (int iy = 1; iy <= ny; ++iy) fla[iy + iad00 + qoff] = fla[iy + iadn0 + qoff];
  }

  // :828-841, propagation: Q + (DT/DX1(IXY)) * (FLA(IXY-INC) - FLA(IXY)), the
  // quotient first, then M6: MAX_EXPR <x, 0.0>, maxss 0,x: -0.0 and NaN both
  // become +0.0.
  for (int ip = 1; ip <= nact; ++ip) {
    const int ixy = mapact[ip - 1];
    const Real f = dt / dx1[ixy - 1];
    const Real u = q[ixy + qoff] + f * (fla[ixy - inc + qoff] - fla[ixy + qoff]);
    q[ixy + qoff] = max_r(u, r0);
  }
}

}  // namespace ww::prop
