// kokkos/src/ww_kokkos/prop_qck3.hpp
// W3QCK3, ported to Kokkos: ULTIMATE QUICKEST, 1-D propagation on a regular
// 2-D field with cell transparencies (W3QCK1 plus obstructions). Phase 1,
// translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3uqckmd.F90:925-1336 at NOAA-EMC/WW3 03adda91;
// author H. L. Tolman. W3_S and W3_T* output (STRACE, the test prints, the IENT
// SAVE) are not ported: nothing of them survives switch_lab_shrd, and a kernel
// never prints. The W3_OMPH directives are not defined under SHRD.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and assembly of
// w3uqckmd.F90 (WW3's own CMake Release flags, switch_lab_shrd): no libm call,
// no libmvec symbol, no FMA, no reduction; SIGN is __builtin_copysignf, ABS a
// mask, INT a cvttss2si truncation, `/6.` a true division, `**2` one multiply,
// `2.*x` emitted as x+x (the same bits as a multiply by 2). The central loop is
// vectorised four points at a time with scalar gathers, which changes no bits;
// the MIN/MAX operand orders below are the ones the maxss/minss and maxps/minps
// instructions emit (spec_W3QCK3.md, section 3).
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"  // fint, max_r, min_r

namespace ww::prop {

/// Reals of work storage w3qck3 needs: the automatic FLA(1-MY:MY*MX),
/// MY*(MX+1) entries (w3uqckmd.F90:1036), heap-allocated and freed by the
/// Fortran on every call. The port takes it from the caller.
KOKKOS_INLINE_FUNCTION constexpr int qck3_work_reals(int mx, int my) { return my * (mx + 1); }

/// W3QCK3(MX, MY, NX, NY, CFLL, TRANS, Q, CLOSE, INC, MAPACT, NACT, MAPBOU,
/// NB0, NB1, NB2, NDSE, NDST) for one field. NDSE and NDST are never read in
/// this build (w3uqckmd.F90:1017; their only uses are under W3_T) and are left
/// out.
///
/// Fortran arrays arrive 0-based; the explicit lower bounds of :1019-1021
/// become an offset:
///   cfll   -> CFLL(1 : MY*(MX+1)):    CFLL(i)    = cfll[i - 1]
///   trans  -> TRANS(MY*MX, -1:1):     TRANS(i,j) = trans[(j + 1) * (MY*MX) + i - 1]
///   q      -> Q(1-MY : MY*(MX+2)):    Q(i)       = q[i + my - 1]
///   mapact -> MAPACT(1:NACT), mapbou -> MAPBOU(1:NB2): values are 1-based IXY,
///             storage 0-based (only the first NACT and NB2 entries are read)
///   fla    -> FLA(1-MY : MY*MX):      FLA(i)     = fla[i + my - 1], qck3_work_reals
///             entries, caller-owned, zeroed here (:1085)
/// cfll and q are INOUT: with close, columns 0, NX+1 and NX+2 of Q and column
/// NX+1 of CFLL are overwritten in place, over whole columns IY = 1..NY, so a
/// later Fortran reader of those columns sees the same bytes (:1087-1112).
/// Without close, the ghost columns are read as the caller left them
/// (W3XYP3 zeroes VQ at w3pro3md.F90:1150 and VLCFLX/Y at :944-945).
///
/// No reduction: each MAPBOU or MAPACT iteration writes one element and reads
/// only Q, CFLL, TRANS and FLA, so the loops compute the same bits serial or
/// parallel when the maps are injective (W3MAP3's are, w3pro3md.F90:255-392).
/// Phase 1 keeps them serial, in source order, sections 2 to 5 before 6.
KOKKOS_INLINE_FUNCTION void w3qck3(int mx, int my, int nx, int ny, Real* cfll, const Real* trans,
                                   Real* q, bool close, int inc, const int* mapact, int nact,
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
  // The literals as the object's constant pool holds them (spec_W3QCK3.md, section 3).
  static_assert(r11 == 1.10000002384185791015625e+0f, "1.1 as REAL, 0x3f8ccccd");
  static_assert(rm01 == -1.00000001490116119384765625e-1f, "-0.1 as REAL, 0xbdcccccd");
  static_assert(r1em15 == 1.00000000362749372553872184710144210839644074440002441406e-15f,
                "1.E-15 as REAL, 0x26901d7d");
  static_assert(r1em10 == 1.00000001335143196001808973960578441619873046875e-10f,
                "1.E-10 as REAL, 0x2edbe6ff");

  // Offsets of the explicit lower bounds: Q and FLA start at 1-MY, CFLL at 1.
  const int qoff = my - 1;
  const int foff = my - 1;
  // TRANS(IXY, J) is read for J = -1, 0, 1, stride MY*MX per column (:1019).
  const int mymx = my * mx;

  // :1085, FLA = 0. over the whole 1-MY..MY*MX range (a memset in the object):
  // the propagation reads FLA(IXY-INC) of the lowest column, which stays 0
  // without closure.
  const int nfla = qck3_work_reals(mx, my);
  for (int i = 0; i < nfla; ++i) fla[i] = r0;

  // :1087-1112, closure part 1. Serial, statements in source order inside one
  // IY, because columns may alias when NX < 3. IADN1 and IADN2 are read here
  // only; IAD00 and IADN0 again in section 5.
  const int iad00 = -my;
  const int iad02 = my;
  const int iadn0 = iad00 + my * nx;
  const int iadn1 = my * nx;
  const int iadn2 = iad02 + my * nx;
  if (close) {
    for (int iy = 1; iy <= ny; ++iy) {
      q[iy + iad00 + qoff] = q[iy + iadn0 + qoff];  // :1102, 1 ghost column to the left
      q[iy + iadn1 + qoff] = q[iy + qoff];          // :1103, 1st ghost column to the right
      q[iy + iadn2 + qoff] = q[iy + iad02 + qoff];  // :1104, 2nd ghost column to the right
      cfll[iy + iadn1 - 1] = cfll[iy - 1];          // :1105, CFLL extension column, persists
    }
  }

  // :1130-1172, central points, 3rd order with the ULTIMATE limiter.
  for (int ip = 1; ip <= nb0; ++ip) {
    const int ixy = mapbou[ip - 1];
    // :1133. The sum is commutative; -O3 adds CFLL(IXY+INC) + CFLL(IXY), then * 0.5.
    const Real cfl = (cfll[ixy + inc - 1] + cfll[ixy - 1]) * r05;
    // :1134. SIGN(1.1,CFL) is copysign: CFL = -0.0 gives -1.1 and selects the
    // upwind cell on the other side; never a CFL < 0 test.
    const Real s = Kokkos::copysign(r11, cfl);
    // :1134. MIN_EXPR <s, 0.0>: minss 0,s in the vector body and remainder 1-2,
    // minss s,0 in remainder 3 and the scalar loop; s is exactly +-1.1, so all
    // four forms agree. INT truncates: 0 for +1.1, -1 for -1.1.
    const int ixyc = ixy - inc * fint(min_r(s, r0));
    // :1136-1137, in the GIMPLE's order: CFL**2 first, (1.-CFL)*Q(IXY+INC) +
    // (1.+CFL)*Q(IXY) (emitted as cfl+1, the same bits), times 0.5, then
    // (1.-CFL**2)/6. as a true division, times the three-point stencil.
    const Real c2 = cfl * cfl;
    const Real t1 = (r1 - cfl) * q[ixy + inc + qoff];
    const Real t2 = (cfl + r1) * q[ixy + qoff];
    const Real h1 = (t1 + t2) * r05;
    const Real h2 = (r1 - c2) / r6;
    const Real qixyc = q[ixyc + qoff];
    // 2.*Q(IXYC) is `_81 * 2.0e+0` in the GIMPLE and addss of a register with
    // itself in the object; both give the same bits for every float.
    const Real d2 = (q[ixyc - inc + qoff] - qixyc * r2) + q[ixyc + inc + qoff];
    const Real qb = h1 - h2 * d2;
    // :1138-1139. INT(+-1.1) = +-1; IXYD in 32-bit integer arithmetic (the
    // object re-associates the integers, exact).
    const int ixyu = ixyc - inc * fint(s);
    const int ixyd = 2 * ixyc - ixyu;
    const Real qixyu = q[ixyu + qoff];
    // :1140-1141. MAX_EXPR <ABS(DQ), 1.E-15>, maxss 1e-15,|dq|: NaN gives 1e-15.
    const Real dq = q[ixyd + qoff] - qixyu;
    const Real dqnz = Kokkos::copysign(max_r(Kokkos::fabs(dq), r1em15), dq);
    // :1142-1143. MAX_EXPR <qcn, -0.1> (maxss -0.1,qcn) then MIN_EXPR <m, 1.1>
    // (minss 1.1,m): a NaN quotient becomes -0.1.
    const Real qcn = min_r(max_r((qixyc - qixyu) / dqnz, rm01), r11);
    // :1148. The GIMPLE operand order varies by path; the object is maxss qcn,r in
    // all five (vector body, three remainders, scalar loop): r>qcn?r:qcn, so a NaN
    // r keeps QCN.
    const Real qbn0 = max_r((qb - qixyu) / dqnz, qcn);
    // :1149. -O3 reassociates MIN(QBN, 1., QCN/MAX(1.E-10,ABS(CFL))) into
    // MIN(QBN0, MIN(u, 1.)): maxss 1e-10,|cfl|; minss 1,u (NaN u -> 1);
    // minss u,qbn0 (NaN qbn0 -> u).
    const Real u = min_r(qcn / max_r(Kokkos::fabs(cfl), r1em10), r1);
    const Real qbn = min_r(qbn0, u);
    // :1150. QBN*DQ then + Q(IXYU); both operand orders give the same bits.
    const Real qbr = qixyu + qbn * dq;
    // :1151. CFAC = REAL(INT(2.*ABS(QCN-0.5))): 0. or 1. for QCN in [-0.1,1.1];
    // the vector body adds -0.5, the same bits as subtracting 0.5.
    const Real cfac = static_cast<Real>(fint(Kokkos::fabs(qcn - r05) * r2));
    // :1152. Two multiplies and one add even though CFAC is 0 or 1 (keeps -0.0
    // and NaN as the Fortran does); -O3 adds Q(IXYC)*CFAC + (1.-CFAC)*QBR.
    const Real qb2 = qixyc * cfac + (r1 - cfac) * qbr;
    // :1154.
    fla[ixy + foff] = cfl * qb2;
  }

  // :1187-1204, boundary above: CFL from CFLL(IXY). MIN_EXPR <s, 0.0>, minss 0,s.
  for (int ip = nb0 + 1; ip <= nb1; ++ip) {
    const int ixy = mapbou[ip - 1];
    const Real cfl = cfll[ixy - 1];
    const Real s = Kokkos::copysign(r11, cfl);
    const int ixyc = ixy - inc * fint(min_r(s, r0));
    fla[ixy + foff] = q[ixyc + qoff] * cfl;  // :1191, emitted Q*CFL; the same bits as CFL*Q
  }

  // :1217-1234, boundary below: CFL from CFLL(IXY+INC). MIN_EXPR as above.
  for (int ip = nb1 + 1; ip <= nb2; ++ip) {
    const int ixy = mapbou[ip - 1];
    const Real cfl = cfll[ixy + inc - 1];
    const Real s = Kokkos::copysign(r11, cfl);
    const int ixyc = ixy - inc * fint(min_r(s, r0));
    fla[ixy + foff] = q[ixyc + qoff] * cfl;  // :1221
  }

  // :1240-1247, closure part 2: FLA column 0 <- column NX, whole columns.
  if (close) {
    for (int iy = 1; iy <= ny; ++iy) fla[iy + iad00 + foff] = fla[iy + iadn0 + foff];
  }

  // :1258-1283, propagation with transparencies. JN and JP pick the TRANS
  // column: -1 for an incoming flux from below (FLA(IXY-INC) > 0), +1 for an
  // incoming flux from above (FLA(IXY) < 0), 0 otherwise; comiss, so a NaN
  // flux compares false and reads column 0 (:1261-1270).
  for (int ip = 1; ip <= nact; ++ip) {
    const int ixy = mapact[ip - 1];
    const Real fm = fla[ixy - inc + foff];
    const Real fp = fla[ixy + foff];
    const int jn = (fm > r0) ? -1 : 0;
    const int jp = (fp < r0) ? 1 : 0;
    // :1275-1276. Q + TRANS(IXY,JN)*FLA(IXY-INC), then - TRANS(IXY,JP)*FLA(IXY),
    // then MAX_EXPR <t, 0.0>, maxss 0,t: -0.0 and NaN both become +0.0.
    const Real t = q[ixy + qoff] + fm * trans[static_cast<long>(jn + 1) * mymx + ixy - 1];
    const Real u = t - fp * trans[static_cast<long>(jp + 1) * mymx + ixy - 1];
    q[ixy + qoff] = max_r(u, r0);
  }
}

}  // namespace ww::prop
