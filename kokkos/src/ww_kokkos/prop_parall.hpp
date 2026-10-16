// kokkos/src/ww_kokkos/prop_parall.hpp
// PROP_REFRACTION_PR3 and PROP_FREQ_SHIFT_M2 of W3PARALL, ported to Kokkos:
// the per-point refraction velocity in direction (CAD) and the frequency-shift
// velocity at the wavenumber band boundaries (CWNB_M2, with DWNI_M2) that the
// PDLIB implicit scheme puts into its matrix. Phase 1, translated, not
// regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3parall.F90:447-589 (PROP_REFRACTION_PR3)
// and :754-894 (PROP_FREQ_SHIFT_M2) at NOAA-EMC/WW3 03adda91 (the WW3/ fork has
// the same w3parall.F90 byte for byte, v, diff, 2026-10-16); authors A. Roland
// and M. Dutour-Sikiric. Called per local node IP = 1..np by
// calcARRAY_JACOBI_SPECTRAL_1 (w3profsmd_pdlib.F90:4515, :4542) and by
// calcARRAY_JACOBI_SPECTRAL_2 (:4670, :4699, the IMEM == 2 path), both only when
// IOBP_LOC(IP)==1 .AND. IOBDP_LOC(IP)==1 .AND. IOBPA_LOC(IP)==0; PR3 with
// DoLimiterRefraction = .FALSE. at both sites, M2 when FreqShiftMethod == 2.
// Compiled as regtests/ww3_tp2.17/input/switch_PDLIB builds it: W3_S (STRACE)
// and W3_REFRX (the @C/@x FRK of :560-565; the switch has no REFRX) are not
// compiled and not ported. Run-time branches are all translated: FLCUR
// both ways (:545-558), DoLimiter both ways (:577-581), LPDLIB both ways in M2
// (:836-852; PR3 reads DDDX(1,IP) unconditionally, :527-528).
//
// Dead code not translated, as -O3 removed it: in PR3, IX and IY (:525-526,
// MAPSF read, never used); in M2, sumDiff (:883, :886), whose MAX(CWNB_M2,
// ZERO) is never stored (no maxss on :886 in the object).
//
// Precision. Every variable is default REAL (float). CAD(ISP)=DBLE(VCFLT(ISP))
// (:584) and CWNB_M2(ISP)=DBLE(...) (:885), DWNI_M2(IK)=DBLE(...) (:889) store
// a REAL widened to double back into a REAL dummy: exact, and the object has
// no cvt there (a plain movss/movups copy). The literals 1.0, 5., 0., 0.5 are
// REAL. No REAL(8) arithmetic exists in either routine.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE (-fdump-tree-optimized)
// and the object of w3parall.F90 (WW3's CMake Release flags, switch_PDLIB; the
// -fdump object disassembles identically to the CMake one): no FMA (SSE
// baseline), no libm or libmvec call, no integer power; SIGN is copysign
// (__builtin_copysignf / .COPYSIGN, andps/andnps/orps in the object), ABS a
// mask, FACTH = 1.0/DTH and CTMAX_eff = CTMAX/DTG true divisions. The order of
// every product and sum, per site, is written at the line below. MIN/MAX in the
// operand order of the emitted maxss/minss (dest = dest OP src ? dest : src):
//   PR3 :533, M2 :866  maxss DMIN,DW:   DW > DMIN ? DW : DMIN    max_r(dw, dmin)
//   PR3 :536, M2 :869  maxss 0,t:       t > 0 ? t : 0            max_r(t, 0)
//   PR3 :578           minss/minps CTMAX_eff,|v|: |v| < C ? |v| : C  min_r(|v|, C)
// Vectorised (fopt-info): PR3 :541 (FRK, FRG), :553 (VCFLT with currents),
// :568 (VELNOFILT, both limiter branches), :583 (the CAD copy); M2 :862 (FKC),
// :888 (DWNI_M2). Every one element by element, no reduction: the vector lanes
// commute operands of + and * only, which changes no bits. The VCWN (:874-881)
// and CWNB_M2 (:884-887) loops are scalar.
//
// Why parallel calls give the Fortran's bits: each call reads module arrays
// nobody writes during calcARRAY_JACOBI_SPECTRAL_1 and writes only its own
// outputs and its own work buffer, so the per-IP loop of the caller can run
// one point per thread, provided each thread has its own work buffer.
//
// CUDA: see the port report; nvcc may turn a MIN/MAX against the constant 0 into
// max.f32, whose -0/NaN rule against this ternary is not checked here (⚠).
#pragma once

#include <Kokkos_Core.hpp>

#include "pdlib_params.hpp"  // the PDLIB conventions (1-based index values, LayoutLeft)
#include "real.hpp"
#include "st4_params.hpp"  // max_r(a,b) = a>b?a:b, min_r(a,b) = a<b?a:b

namespace ww::pdlib {

/// The module arrays and scalars the two routines read (USE lists at
/// w3parall.F90:495-499 and :803-806), as 0-based pointers at the Fortran's
/// first element; extents and index formulas below. Read-only here; on the
/// device every pointer must be device memory (a View's data()).
struct PropParallArrays {
  int nk = 0;     ///< W3GDATMD NK
  int nth = 0;    ///< W3GDATMD NTH
  int nspec = 0;  ///< W3GDATMD NSPEC = NK*NTH
  int msea = 0;   ///< first extent of MAPSF(MSEA,3) (w3gdatmd.F90:1736)
  /// First extent of DDDX, DDDY, DCXDX, DCXDY, DCYDX, DCYDY: 1 when LPDLIB,
  /// they are (1,NSEAL); NY otherwise, (NY,NX) (w3adatmd.F90:1374-1392).
  int ny = 1;
  int lpdlib = 1;  ///< CONSTANTS LPDLIB (constants.F90:102), 0/1; read by M2 only
  int flcur = 0;   ///< W3IDATMD FLCUR, 0/1; read by PR3 only
  Real dmin = 0;   ///< W3GDATMD DMIN
  Real ctmax = 0;  ///< W3GDATMD CTMAX; read by PR3 only
  Real dth = 0;    ///< W3GDATMD DTH; read by PR3 only

  /// SIG(0:MK+1), DSIP(0:MK+1) (w3gdatmd.F90:2066, :2068): SIG(IK) = sig[IK].
  const Real* sig = nullptr;
  const Real* dsip = nullptr;
  /// ECOS, ESIN, EC2, ESC, ES2 (MSPEC+MTH), MAPWN (MSPEC+MTH), integer 1-based
  /// IK (w3gdatmd.F90:2058-2065): X(ISP) = x[ISP-1]. M2 reads EC2/ESC/ES2 at
  /// ITH = 1..NTH only (:863), the first wavenumber row.
  const Real* ecos = nullptr;
  const Real* esin = nullptr;
  const Real* ec2 = nullptr;
  const Real* esc = nullptr;
  const Real* es2 = nullptr;
  const int* mapwn = nullptr;
  /// MAPSF(MSEA,3), integer: MAPSF(ISEA,J) = mapsf[(ISEA-1) + (J-1)*msea].
  /// Read by M2's non-PDLIB branch only.
  const int* mapsf = nullptr;
  /// CTHG0S(0:MSEA) (w3gdatmd.F90:1743): cthg0s[ISEA]. PR3 only.
  const Real* cthg0s = nullptr;
  /// CG(0:NK+1,0:NSEA), WN(0:NK+1,0:NSEA) (w3adatmd.F90:1312, :1325):
  /// X(IK,ISEA) = x[IK + ISEA*(NK+2)].
  const Real* cg = nullptr;
  const Real* wn = nullptr;
  /// DW(0:NSEA), CX(0:NSEA), CY(0:NSEA) (w3adatmd.F90:1004, :1008): x[ISEA].
  const Real* dw = nullptr;
  const Real* cx = nullptr;
  const Real* cy = nullptr;
  /// DDDX, DDDY, DCXDX, DCXDY, DCYDX, DCYDY, (ny, *): X(I,J) = x[(I-1) + (J-1)*ny],
  /// so X(1,IP) = x[IP-1] when LPDLIB.
  const Real* dddx = nullptr;
  const Real* dddy = nullptr;
  const Real* dcxdx = nullptr;
  const Real* dcxdy = nullptr;
  const Real* dcydx = nullptr;
  const Real* dcydy = nullptr;
};

/// Reals of work storage one call of either routine needs (the automatic
/// arrays the Fortran mallocs per call): PR3 DSDD(0:NK+1), FRK(NK), FRG(NK)
/// (VCFLT lives in CAD, see prop_refraction_pr3); M2 DSDD(0:NK+1), FKC(NTH),
/// VCWN(1-NTH:NSPEC+NTH). One buffer of this size serves both, one after the
/// other; each thread needs its own.
KOKKOS_INLINE_FUNCTION constexpr int prop_parall_work_reals(int nk, int nth, int nspec) {
  const int pr3 = (nk + 2) + nk + nk;
  const int m2 = (nk + 2) + nth + (nspec + 2 * nth);
  return pr3 > m2 ? pr3 : m2;
}

namespace parall_detail {
// DEPTH and DSDD(0:NK+1), :533-540 and :866-873, identical in both routines.
// DEPTH*WN < 5. is comiss on WN*DEPTH: NaN takes the ELSE branch, as .LT. does.
// (WN*CG - SIG*0.5), maxss 0, then / DEPTH.
KOKKOS_INLINE_FUNCTION Real dsdd_fill(const PropParallArrays& a, int isea, Real* dsdd) {
  constexpr Real r0 = static_cast<Real>(0.);
  constexpr Real r05 = static_cast<Real>(0.5);
  constexpr Real r5 = static_cast<Real>(5.);
  const int nk2 = a.nk + 2;
  const Real* cg = a.cg + static_cast<long>(isea) * nk2;
  const Real* wn = a.wn + static_cast<long>(isea) * nk2;
  const Real depth = ww::st4::max_r(a.dw[isea], a.dmin);  // maxss DMIN,DW
  for (int ik = 0; ik <= a.nk + 1; ++ik) {
    if (wn[ik] * depth < r5) {
      const Real t = wn[ik] * cg[ik] - a.sig[ik] * r05;
      dsdd[ik] = ww::st4::max_r(t, r0) / depth;  // maxss 0,t
    } else {
      dsdd[ik] = r0;
    }
  }
  return depth;
}
}  // namespace parall_detail

/// PROP_REFRACTION_PR3(IP, ISEA, DTG, CAD, DoLimiter), w3parall.F90:447-589.
/// ip is the local node (1..np), isea the sea point (MAPFS(1,iplg(IP))), both
/// 1-based as in Fortran. cad is CAD(NSPEC): cad[ISP-1], intent(out); it also
/// holds VCFLT during the call (VCFLT(ISP) is only ever read and rewritten at
/// the same ISP, so the bits are those of the separate array). work has at
/// least prop_parall_work_reals entries.
KOKKOS_INLINE_FUNCTION void prop_refraction_pr3(const PropParallArrays& a, int ip, int isea,
                                                Real dtg, Real* cad, bool do_limiter,
                                                Real* work) {
  using ww::st4::min_r;
  constexpr Real r0 = static_cast<Real>(0.);
  constexpr Real r1 = static_cast<Real>(1.0);
  const int nk = a.nk;
  const int nspec = a.nspec;
  Real* dsdd = work;          // DSDD(0:NK+1): DSDD(IK) = dsdd[IK]
  Real* frk = dsdd + nk + 2;  // FRK(NK):      FRK(IK)  = frk[IK-1]
  Real* frg = frk + nk;       // FRG(NK):      FRG(IK)  = frg[IK-1]
  Real* vcflt = cad;          // VCFLT(NSPEC): VCFLT(ISP) = vcflt[ISP-1]
  const Real* cg = a.cg + static_cast<long>(isea) * (nk + 2);
  const Real* wn = a.wn + static_cast<long>(isea) * (nk + 2);

  // :527-530. DDDX(1,IP), DDDY(1,IP): element (IP-1)*ny of a (ny,*) array, where ny is
  // the first extent, NY or 1 as LPDLIB allocates it (w3adatmd.F90:1375,1384).
  const Real edddx = a.dddx[static_cast<long>(ip - 1) * a.ny];
  const Real edddy = a.dddy[static_cast<long>(ip - 1) * a.ny];
  const Real ecthg0 = a.cthg0s[isea];
  const Real facth = r1 / a.dth;  // :530, divss
  const Real fdg = ecthg0 * facth;  // :532
  // :533-540, DEPTH and DSDD.
  parall_detail::dsdd_fill(a, isea, dsdd);
  // :541-544. (FACTH*DSDD)/WN, FDG*CG.
  for (int ik = 1; ik <= nk; ++ik) {
    frk[ik - 1] = (facth * dsdd[ik]) / wn[ik];
    frg[ik - 1] = fdg * cg[ik];
  }
  if (a.flcur != 0) {
    // :546-552. DCXDX(1,IP).. read only here, as the object does.
    const Real edcxdx = a.dcxdx[ip - 1];
    const Real edcxdy = a.dcxdy[ip - 1];
    const Real edcydx = a.dcydx[ip - 1];
    const Real edcydy = a.dcydy[ip - 1];
    const Real dcyx = facth * edcydx;
    const Real dcxxyy = facth * (edcxdx - edcydy);  // the parenthesis is kept, :551
    const Real dcxy = facth * edcxdy;
    // :553-555. (ES2*DCYX + ESC*DCXXYY) - EC2*DCXY.
    for (int isp = 1; isp <= nspec; ++isp) {
      vcflt[isp - 1] =
          (a.es2[isp - 1] * dcyx + a.esc[isp - 1] * dcxxyy) - a.ec2[isp - 1] * dcxy;
    }
  } else {
    for (int isp = 1; isp <= nspec; ++isp) vcflt[isp - 1] = r0;  // :557, VCFLT=0
  }
  // :568-582. (VCFLT + FRG*ECOS) + FRK*(ESIN*DDDX - ECOS*DDDY), then the CTMAX
  // limiter: copysign(minss C,|v|) with C = CTMAX/DTG (:567, divss).
  const Real ctmax_eff = a.ctmax / dtg;
  for (int isp = 1; isp <= nspec; ++isp) {
    const int mw = a.mapwn[isp - 1];
    const Real velnofilt =
        (vcflt[isp - 1] + frg[mw - 1] * a.ecos[isp - 1]) +
        frk[mw - 1] * (a.esin[isp - 1] * edddx - a.ecos[isp - 1] * edddy);
    if (do_limiter) {
      vcflt[isp - 1] = Kokkos::copysign(min_r(Kokkos::fabs(velnofilt), ctmax_eff), velnofilt);
    } else {
      vcflt[isp - 1] = velnofilt;
    }
  }
  // :583-585. CAD = DBLE(VCFLT) into REAL: the same bits, already in place.
}

/// PROP_FREQ_SHIFT_M2(IP, ISEA, CWNB_M2, DWNI_M2, DTG), w3parall.F90:754-894.
/// ip, isea as for prop_refraction_pr3. cwnb_m2 is CWNB_M2(1-NTH:NSPEC):
/// CWNB_M2(ISP) = cwnb_m2[ISP + NTH - 1], NSPEC+NTH entries; dwni_m2 is
/// DWNI_M2(NK): dwni_m2[IK-1]. Both intent(out). work has at least
/// prop_parall_work_reals entries.
KOKKOS_INLINE_FUNCTION void prop_freq_shift_m2(const PropParallArrays& a, int ip, int isea,
                                               Real* cwnb_m2, Real* dwni_m2, Real dtg,
                                               Real* work) {
  constexpr Real r05 = static_cast<Real>(0.5);
  const int nk = a.nk;
  const int nth = a.nth;
  const int nspec = a.nspec;
  Real* dsdd = work;          // DSDD(0:NK+1):          DSDD(IK) = dsdd[IK]
  Real* fkc = dsdd + nk + 2;  // FKC(NTH):              FKC(ITH) = fkc[ITH-1]
  Real* vcwn = fkc + nth;     // VCWN(1-NTH:NSPEC+NTH): VCWN(ISP) = vcwn[ISP+NTH-1]
  const Real* cg = a.cg + static_cast<long>(isea) * (nk + 2);
  const Real* wn = a.wn + static_cast<long>(isea) * (nk + 2);

  // :836-852.
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
  // :854-860. Three sign flips (xorps) of products; FKD = (CX*DDDX + CY*DDDY)*FACK.
  const Real ecx = a.cx[isea];
  const Real ecy = a.cy[isea];
  const Real fack = dtg;
  const Real dcxx = -(fack * edcxdx);
  const Real dcxyyx = -(fack * (edcxdy + edcydx));
  const Real dcyy = -(fack * edcydy);
  const Real fkd = fack * (ecx * edddx + ecy * edddy);
  // :862-864. (EC2*DCXX + ESC*DCXYYX) + ES2*DCYY.
  for (int ith = 1; ith <= nth; ++ith) {
    fkc[ith - 1] = (a.ec2[ith - 1] * dcxx + a.esc[ith - 1] * dcxyyx) + a.es2[ith - 1] * dcyy;
  }
  // :866-873.
  parall_detail::dsdd_fill(a, isea, dsdd);
  // :874-881. FKD0 = (FKD/CG)*DSDD; VCWN = WN*FKC + FKD0, ISP from 1-NTH.
  int isp = -nth;
  for (int ik = 0; ik <= nk + 1; ++ik) {
    const Real fkd0 = (fkd / cg[ik]) * dsdd[ik];
    for (int ith = 1; ith <= nth; ++ith) {
      isp = isp + 1;
      vcwn[isp + nth - 1] = wn[ik] * fkc[ith - 1] + fkd0;
    }
  }
  // :884-887. 0.5*(VCWN(ISP) + VCWN(ISP+NTH)); sumDiff is dead (header).
  for (int i = 1 - nth; i <= nspec; ++i) {
    cwnb_m2[i + nth - 1] = r05 * (vcwn[i + nth - 1] + vcwn[i + 2 * nth - 1]);
  }
  // :888-890. CG/DSIP.
  for (int ik = 1; ik <= nk; ++ik) dwni_m2[ik - 1] = cg[ik] / a.dsip[ik];
}

}  // namespace ww::pdlib
