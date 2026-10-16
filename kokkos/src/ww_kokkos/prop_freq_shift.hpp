// kokkos/src/ww_kokkos/prop_freq_shift.hpp
// PROP_FREQ_SHIFT of W3PARALL, ported to Kokkos: the per-point frequency-shift
// velocity CAS(NSPEC) and the wavenumber steps DMM(0:NK2) that the PDLIB
// implicit scheme puts into its matrix when FreqShiftMethod == 1. Phase 1,
// translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3parall.F90:604-739 at NOAA-EMC/WW3
// 03adda91 (the WW3/ fork has the same w3parall.F90 byte for byte, v, cmp,
// 2026-10-16); authors A. Roland and M. Dutour-Sikiric. Called per local node
// by CALCARRAY_JACOBI_SPECTRAL_1 (w3profsmd_pdlib.F90:4496) under the same
// IOBP/IOBDP/IOBPA condition as its siblings in prop_parall.hpp, whose types
// (PropParallArrays) and DEPTH/DSDD helper this header reuses. Compiled as
// regtests/ww3_tp2.17/input/switch_PDLIB builds it: W3_S (STRACE) is not
// compiled and not ported. The run-time branch LPDLIB is translated both ways
// (:679-695); the IX, IY of the ELSE branch are read there, as the object does.
//
// Adapter. pdlib_jacobi_spectral.hpp calls
//   prop.freq_shift(ip, isea, cas, dmm, dtg)
// with 1-based IP and ISEA, cas = CAS(1:NSPEC), dmm = DMM(0:NK2) (dmm[0] is
// DMM(0)). Its Prop member forwards to
//   prop_freq_shift(a, ip, isea, cas, dmm, dtg, work)
// with this thread's work buffer, prop_freq_shift_work_reals(nk, nth) reals,
// which is never more than prop_parall_work_reals(nk, nth, nspec), so one
// buffer serves the three routines.
//
// What is stored. CFLK(NK2,NTH) (:673) is not kept: the Fortran reads only
// CFLK(1:NK,:) back (:730-735, CAS(ITH+(IK-1)*NTH) = CFLK(IK,ITH)), so the
// CFLK loop (:722-729) runs over IK = 0..NK+1 as written and stores row IK+1
// straight into CAS when IK+1 <= NK. Rows NK+1 and NK+2 are computed and not
// stored, as nothing reads them; no other value changes.
//
// Precision. Every variable is default REAL (float); ZERO is the REAL
// PARAMETER 0.0d0 (:94). DMM(IK+1) = DBLE(WN(IK+1)-WN(IK)) (:708) and
// CAS(ISP) = DBLE(CFLK(IK,ITH)) (:733) widen a REAL difference or value and
// store it in a REAL dummy: exact, and the GIMPLE has no conversion (the subss
// result is stored). Literals 5., 0., 0.5 are REAL. No REAL(8) arithmetic.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE (-fdump-tree-optimized)
// and the object of w3parall.F90 (WW3's CMake Release flags, switch_PDLIB; the
// dump object disassembles identically to the CMake one, v, objdump/cmp
// 2026-10-16): no FMA, no libm call, no reciprocal multiply, every division a
// divss/divps. Per line:
//   :698-700  DCXX = -DCXDX, DCXYYX = -(DCXDY+DCYDX), DCYY = -DCYDY: xorps
//             sign flips, no FACK factor here (unlike M2);
//   :701      FKD = CX*DDDX + CY*DDDY (mulss, mulss, addss);
//   :703-705  FKC = (EC2*DCXX + ESC*DCXYYX) + ES2*DCYY;
//   :706-712  DB(IK+1) = DSIP(IK)/CG(IK), DMM(IK+1) = WN(IK+1)-WN(IK);
//   :714-721  parall_detail::dsdd_fill: maxss DMIN,DW is DW > DMIN ? DW : DMIN,
//             comiss 5.,WN*DEPTH (NaN takes the ELSE), (WN*CG - SIG*0.5),
//             maxss 0,t is t > 0 ? t : 0, then divss DEPTH; the same
//             instructions as in M2, the helper's source;
//   :722-729  FKD0 = (FKD/CG)*DSDD, VELFAC = FACK/DB(IK+1),
//             CFLK = (VELFAC*(FKD0 + WN*FKC)) / VELFAC, not simplified.
// Vectorised (fopt-info): :703 (FKC) and :725 (the ITH loop of CFLK), element
// by element, no reduction. Vector body and scalar tail differ only in the
// order of the operands of a commutative + or *, which gives the same bits for
// every input except two NaNs of different payload, where SSE returns the
// first (destination) operand's (⚠, not traced to a regtest field; no MIN/MAX
// depends on the lane here). Order of the loops: the Fortran's.
//
// Why parallel calls give the Fortran's bits: each call reads module arrays
// nobody writes during CALCARRAY_JACOBI_SPECTRAL_1 and writes only CAS, DMM
// and its own work buffer; no reduction or atomic, so any execution space and
// thread count gives the same bits on the host.
//
// CUDA (sm_89, --fmad=false, compile only): the PTX has no fma, every float
// op .rn, divisions div.rn.f32, and the two ternary MAXes become max.f32. For
// one NaN operand max.f32 returns the other, which is what the ternary gives
// here (DW NaN gives DMIN, t NaN gives 0) (⚠, PTX ISA rule from memory); for
// t = -0 the ternary gives +0, and that max.f32 does too is not checked (⚠).
#pragma once

#include <Kokkos_Core.hpp>

#include "prop_parall.hpp"  // PropParallArrays, parall_detail::dsdd_fill
#include "real.hpp"

namespace ww::pdlib {

/// Reals of work storage one call of prop_freq_shift needs: DSDD(0:NK+1),
/// DB(NK2), FKC(NTH) (the automatic arrays of :669 and :673 without CFLK, see the
/// header). Each thread needs its own.
KOKKOS_INLINE_FUNCTION constexpr int prop_freq_shift_work_reals(int nk, int nth) {
  return (nk + 2) + (nk + 2) + nth;
}

/// PROP_FREQ_SHIFT(IP, ISEA, CAS, DMM, DTG), w3parall.F90:604-739. ip is the
/// local node (1..np), isea the sea point, both 1-based as in Fortran. cas is
/// CAS(NSPEC): cas[ISP-1]; dmm is DMM(0:NK2): dmm[K] = DMM(K), NK+3 entries;
/// both intent(out), written in full. work has at least
/// prop_freq_shift_work_reals entries.
KOKKOS_INLINE_FUNCTION void prop_freq_shift(const PropParallArrays& a, int ip, int isea,
                                            Real* cas, Real* dmm, Real dtg, Real* work) {
  const int nk = a.nk;
  const int nth = a.nth;
  Real* dsdd = work;          // DSDD(0:NK+1): DSDD(IK) = dsdd[IK]
  Real* db = dsdd + nk + 2;   // DB(NK2):      DB(J)    = db[J-1]
  Real* fkc = db + nk + 2;    // FKC(NTH):     FKC(ITH) = fkc[ITH-1]
  const Real* cg = a.cg + static_cast<long>(isea) * (nk + 2);
  const Real* wn = a.wn + static_cast<long>(isea) * (nk + 2);

  // :679-695.
  long j;
  if (a.lpdlib != 0) {
    j = ip - 1;  // X(1,IP)
  } else {
    const int ix = a.mapsf[(isea - 1) + 0 * a.msea];
    const int iy = a.mapsf[(isea - 1) + 1 * a.msea];
    j = (iy - 1) + static_cast<long>(ix - 1) * a.ny;  // X(IY,IX)
  }
  const Real edcxdx = a.dcxdx[j];
  const Real edcxdy = a.dcxdy[j];
  const Real edcydx = a.dcydx[j];
  const Real edcydy = a.dcydy[j];
  const Real edddx = a.dddx[j];
  const Real edddy = a.dddy[j];
  // :696-702.
  const Real ecx = a.cx[isea];
  const Real ecy = a.cy[isea];
  const Real dcxx = -edcxdx;
  const Real dcxyyx = -(edcxdy + edcydx);
  const Real dcyy = -edcydy;
  const Real fkd = ecx * edddx + ecy * edddy;
  const Real fack = dtg;
  // :703-705.
  for (int ith = 1; ith <= nth; ++ith) {
    fkc[ith - 1] = (a.ec2[ith - 1] * dcxx + a.esc[ith - 1] * dcxyyx) + a.es2[ith - 1] * dcyy;
  }
  // :706-712.
  for (int ik = 0; ik <= nk; ++ik) {
    db[ik] = a.dsip[ik] / cg[ik];       // DB(IK+1)
    dmm[ik + 1] = wn[ik + 1] - wn[ik];  // DMM(IK+1)
  }
  db[nk + 1] = a.dsip[nk + 1] / cg[nk + 1];  // DB(NK+2)
  dmm[nk + 2] = kZero;  // DMM(NK+2) = ZERO (pdlib_params.hpp)
  dmm[0] = dmm[1];
  // :714-721.
  parall_detail::dsdd_fill(a, isea, dsdd);
  // :722-729, with :730-735 folded in (header): CFLK(IK+1,ITH) is CAS(ISP),
  // ISP = ITH + IK*NTH, for IK+1 <= NK.
  for (int ik = 0; ik <= nk + 1; ++ik) {
    const Real fkd0 = (fkd / cg[ik]) * dsdd[ik];
    const Real velfac = fack / db[ik];
    for (int ith = 1; ith <= nth; ++ith) {
      const Real velnofilt = (fkd0 + wn[ik] * fkc[ith - 1]) * velfac;
      const Real cflk = velnofilt / velfac;
      if (ik + 1 <= nk) cas[(ith - 1) + ik * nth] = cflk;
    }
  }
}

}  // namespace ww::pdlib
