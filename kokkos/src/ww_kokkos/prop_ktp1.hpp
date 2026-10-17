// kokkos/src/ww_kokkos/prop_ktp1.hpp
// W3KTP1, ported to Kokkos: propagation in spectral space (refraction in
// direction, wavenumber shift) for one sea point with the first-order upwind
// scheme. Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3pro1md.F90:900-1230 at NOAA-EMC/WW3
// 03adda91 (identical in WW3/ at the fork pin 357f575c); author H. L. Tolman.
// Compiled as switches/switch_lab_shrd with PR3 UQ replaced by PR1: W3_S, W3_T
// (STRACE, IENT, the test prints), W3_REFRX (the @C/@x refraction, which is why
// DCDX and DCDY are dead) and W3_DEBUG are not compiled and not ported; a kernel
// never prints.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and assembly of
// w3pro1md.F90 (WW3's own CMake Release flags; x86-64 baseline, so no FMA
// exists in the object): no libm or libmvec call, no integer power; ABS is a
// mask, the sign flips of :1143-1145 are xorps, MIN/MAX are minss/maxss (and
// minps/maxps) whose operand orders are written below per site. The VAA copy,
// FRK/FRG, VCTH, FKC, DWNI, the theta fluxes and both VA updates are vectorised
// element by element, which changes no bits; the only vectorised reduction,
// FDDMAX, is translated with the object's lane grouping (see 2.a).
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"  // max_r, min_r

namespace ww::prop {

/// The per-grid scalars and flags W3KTP1 reads (W3GDATMD, W3IDATMD; the USE
/// lists at w3pro1md.F90:997-1001).
struct Ktp1Params {
  int nk, nth, nspec;      // NK, NTH, NSPEC
  int flcth, flck, flcur;  // FLCTH, FLCK (w3gdatmd.F90:2389-2390), FLCUR (w3idatmd.F90:973); 0/1
  Real ctmax, fachfa;      // CTMAX (w3gdatmd.F90:2357), FACHFA = XFR**(-FACHF-2) (w3gridmd.F90:3564)
};

/// The per-grid arrays W3KTP1 reads, as the Fortran modules hold them: 0-based
/// pointers at the Fortran's first element, so a non-1 lower bound becomes an
/// offset in the index formula. Read-only here.
struct Ktp1Grid {
  /// SIG(0:NK+1), DSIP(0:NK+1) (w3gdatmd.F90:2066, 2068, allocated 0:MK+1):
  /// SIG(IK) = sig[IK]. SIG is read at IK = 0..NK+1 (:1052), DSIP at 1..NK (:1169).
  const Real* sig;
  const Real* dsip;
  /// ECOS, ESIN, EC2, ESC, ES2 (w3gdatmd.F90:2061-2065, allocated NSPEC+NTH,
  /// lower bound 1): X(ISP) = x[ISP-1]. ESIN and ECOS are read at ITH = 1..NTH
  /// in the FDDMAX loop (:1083), the first wavenumber row, and at ISP = 1..NSPEC
  /// (:1096); EC2, ESC, ES2 at ISP = 1..NSPEC (:1132) and at ITH = 1..NTH (:1150).
  const Real* ecos;
  const Real* esin;
  const Real* ec2;
  const Real* esc;
  const Real* es2;
  /// MAPWN(NSPEC+NTH) (w3gdatmd.F90:2058): MAPWN(ISP) = mapwn[ISP-1], 1-based IK.
  const int* mapwn;
  /// IS0(NSPEC), IS2(NSPEC) (w3adatmd.F90:1415-1416), filled by W3MAP1
  /// (w3pro1md.F90:269-280): 1-based ISP of the previous and the next direction
  /// at the same wavenumber, periodic in direction. IS0(ISP) = is0[ISP-1].
  const int* is0;
  const int* is2;
};

/// The ten automatic arrays of W3KTP1 (w3pro1md.F90:1029-1032), malloced and
/// freed by the Fortran on every call (the ten __builtin_malloc of the GIMPLE).
/// Caller owned; ktp1_work carves them from one buffer of ktp1_work_reals
/// entries. Extents and the Fortran index each pointer maps:
struct Ktp1Work {
  Real* vcth;   // NSPEC          VCTH(NSPEC):              VCTH(ISP)  = vcth[ISP-1]
  Real* vcwn;   // NSPEC+2*NTH    VCWN(1-NTH:NSPEC+NTH):    VCWN(ISP)  = vcwn[ISP+NTH-1]
  Real* vaa;    // NSPEC+2*NTH    VAA(1-NTH:NSPEC+NTH):     VAA(ISP)   = vaa[ISP+NTH-1]
  Real* vflth;  // NSPEC          VFLTH(NSPEC):             VFLTH(ISP) = vflth[ISP-1]
  Real* vflwn;  // NSPEC+NTH      VFLWN(1-NTH:NSPEC):       VFLWN(ISP) = vflwn[ISP+NTH-1]
  Real* dsdd;   // NK+2           DSDD(0:NK+1):             DSDD(IK)   = dsdd[IK]
  Real* frk;    // NK             FRK(NK):                  FRK(IK)    = frk[IK-1]
  Real* frg;    // NK             FRG(NK):                  FRG(IK)    = frg[IK-1]
  Real* fkc;    // NTH            FKC(NTH):                 FKC(ITH)   = fkc[ITH-1]
  Real* dwni;   // NK             DWNI(NK):                 DWNI(IK)   = dwni[IK-1]
};

/// Reals of work storage w3ktp1 needs for one point.
KOKKOS_INLINE_FUNCTION constexpr int ktp1_work_reals(int nk, int nth, int nspec) {
  return nspec + (nspec + 2 * nth) + (nspec + 2 * nth) + nspec + (nspec + nth) + (nk + 2) + nk +
         nk + nth + nk;
}

/// Carve the ten arrays of Ktp1Work out of one buffer of
/// ktp1_work_reals(nk, nth, nspec) entries, in the order of the struct.
KOKKOS_INLINE_FUNCTION Ktp1Work ktp1_work(Real* base, int nk, int nth, int nspec) {
  Ktp1Work w{};
  w.vcth = base;
  w.vcwn = w.vcth + nspec;
  w.vaa = w.vcwn + (nspec + 2 * nth);
  w.vflth = w.vaa + (nspec + 2 * nth);
  w.vflwn = w.vflth + nspec;
  w.dsdd = w.vflwn + (nspec + nth);
  w.frk = w.dsdd + (nk + 2);
  w.frg = w.frk + nk;
  w.fkc = w.frg + nk;
  w.dwni = w.fkc + nth;
  return w;
}

/// W3KTP1(ISEA, FACTH, FACK, CTHG0, CG, WN, DEPTH, DDDX, DDDY, CX, CY, DCXDX,
/// DCXDY, DCYDX, DCYDY, DCDX, DCDY, VA) for one sea point.
///
/// cg, wn, dcdx, dcdy are CG(0:NK+1,ISEA), WN(0:NK+1,ISEA), DCDX(0:NK+1,IY,IX),
/// DCDY(...) (:1016-1019): x[IK] for IK = 0..NK+1, contiguous columns. va is
/// VA(1:NSPEC) of the point: va[ISP-1], read and written in place. depth is
/// used as passed: W3KTP1 applies no DMIN, the caller passes MAX(DMIN, DW(ISEA))
/// (w3wavemd.F90:1819, 2131). isea, dcdx and dcdy are never read in this build
/// (ISEA only under W3_T, DCDX/DCDY only under W3_REFRX) and are kept for the
/// shim's argument table.
///
/// Serial: every loop keeps the Fortran's order. The one reduction, FDDMAX
/// (:1083), follows the object's grouping. Both the theta fluxes (section 4)
/// and the k fluxes (section 5) are taken from VAA, the spectrum as it entered;
/// only the updates of VA are sequential (:1189, :1210), as in the Fortran.
KOKKOS_INLINE_FUNCTION void w3ktp1(const Ktp1Params& p, const Ktp1Grid& g,
                                   [[maybe_unused]] int isea, Real facth, Real fack, Real cthg0,
                                   const Real* cg, const Real* wn, Real depth, Real dddx,
                                   Real dddy, Real cx, Real cy, Real dcxdx, Real dcxdy,
                                   Real dcydx, Real dcydy, [[maybe_unused]] const Real* dcdx,
                                   [[maybe_unused]] const Real* dcdy, Real* va,
                                   const Ktp1Work& w) {
  using ww::st4::max_r;
  using ww::st4::min_r;
  constexpr Real r0 = static_cast<Real>(0.);
  constexpr Real r05 = static_cast<Real>(0.5);
  constexpr Real r1 = static_cast<Real>(1.);
  constexpr Real r5 = static_cast<Real>(5.);

  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  // VAA, VCWN and VFLWN start at 1-NTH (:1029-1031): X(I) = x[I + off].
  const int off = nth - 1;

  // 1.a, :1049-1056. comiss 5.0 against DEPTH*WN: the zero branch is taken
  // when 5.0 <= DEPTH*WN or unordered, as the Fortran .LT. does; then
  // CG*WN - 0.5*SIG, maxss 0,t: (t > 0) ? t : 0, divided by DEPTH.
  for (int ik = 0; ik <= nk + 1; ++ik) {
    if (depth * wn[ik] < r5) {
      const Real t = cg[ik] * wn[ik] - r05 * g.sig[ik];
      w.dsdd[ik] = max_r(t, r0) / depth;
    } else {
      w.dsdd[ik] = r0;
    }
  }

  // 1.b, :1068-1070. VAA(1-NTH:0) and VAA(NSPEC+1:) are only set in 3, under
  // FLCK, and only read in 5, under FLCK.
  for (int isp = 1; isp <= nspec; ++isp) w.vaa[isp + off] = va[isp - 1];

  // 2. Refraction velocities, :1074-1137, only when FLCTH.
  if (p.flcth != 0) {
    // 2.a, :1078-1079. FDG = FACTH*CTHG0 (mulss, commutative).
    Real fddmax = r0;
    const Real fdg = facth * cthg0;

    // :1081-1084. MAX(FDDMAX, ABS(ESIN*DDDX - ECOS*DDDY)) over the first NTH
    // entries of the trig arrays. -O3 vectorises it (w3pro1md.o 0x4b1-0x6b0)
    // only when NTH >= 8: four lanes started from 0 over the first 4*(NTH/4)
    // directions, lane k taking ITH = 4m+k+1, each `maxps |v|,lane`
    // (lane > |v| ? lane : |v|), folded as t0 = max(l2,l0), t1 = max(l3,l1),
    // then max(t1,t0); then a scalar `maxss |v|,acc` (acc > |v| ? acc : |v|)
    // for the at most three directions left. NTH < 8 is the scalar loop from 0
    // (0x2230-0x22a5). The grouping matters only for a NaN, which it keeps
    // where the object keeps it; there is no two-lane block here, unlike
    // W3KTP3's CFLKMAX.
    auto vdd = [&](int ith) {
      return Kokkos::fabs(g.esin[ith - 1] * dddx - g.ecos[ith - 1] * dddy);
    };
    int ith = 1;
    if (nth >= 8) {
      Real l0 = r0, l1 = r0, l2 = r0, l3 = r0;
      const int n4 = (nth / 4) * 4;
      for (; ith <= n4; ith += 4) {
        l0 = max_r(l0, vdd(ith));
        l1 = max_r(l1, vdd(ith + 1));
        l2 = max_r(l2, vdd(ith + 2));
        l3 = max_r(l3, vdd(ith + 3));
      }
      const Real t0 = max_r(l2, l0);
      const Real t1 = max_r(l3, l1);
      fddmax = max_r(t1, t0);
    }
    for (; ith <= nth; ++ith) fddmax = max_r(fddmax, vdd(ith));

    // :1086-1090. (FACTH*DSDD)/WN, then FRK / MAX(1, (FRK*FDDMAX)/CTMAX) with
    // maxss 1,t: (t > 1) ? t : 1, so a NaN or a CTMAX of 0 with FRK*FDDMAX of
    // 0 gives the divisor 1; FRK stays in a register between the two
    // statements, which stores and reloads the same float.
    for (int ik = 1; ik <= nk; ++ik) {
      Real frk = facth * w.dsdd[ik] / wn[ik];
      frk = frk / max_r(frk * fddmax / p.ctmax, r1);
      w.frk[ik - 1] = frk;
      w.frg[ik - 1] = fdg * cg[ik];
    }

    // 2.b, :1094-1097. FRG*ECOS + FRK*((ESIN*DDDX) - (ECOS*DDDY)).
    for (int isp = 1; isp <= nspec; ++isp) {
      const int mw = g.mapwn[isp - 1];
      w.vcth[isp - 1] = w.frg[mw - 1] * g.ecos[isp - 1] +
                        w.frk[mw - 1] * (g.esin[isp - 1] * dddx - g.ecos[isp - 1] * dddy);
    }

    // 2.d, :1124-1135. Current refraction, ((VCTH + ES2*DCYX) + ESC*DCXXYY)
    // - EC2*DCXY, left to right as written.
    if (p.flcur != 0) {
      const Real dcyx = facth * dcydx;
      const Real dcxxyy = facth * (dcxdx - dcydy);
      const Real dcxy = facth * dcxdy;
      for (int isp = 1; isp <= nspec; ++isp) {
        w.vcth[isp - 1] = w.vcth[isp - 1] + g.es2[isp - 1] * dcyx +
                          g.esc[isp - 1] * dcxxyy - g.ec2[isp - 1] * dcxy;
      }
    }
  }

  // 3. Wavenumber shift velocities, :1141-1172, only when FLCK.
  if (p.flck != 0) {
    // :1143-1146. -(FACK*x) is xorps after mulss, the same bits as (-FACK)*x.
    const Real dcxx = -(fack * dcxdx);
    const Real dcxyyx = -(fack * (dcxdy + dcydx));
    const Real dcyy = -(fack * dcydy);
    const Real fkd = fack * (cx * dddx + cy * dddy);

    // :1148-1151. (EC2*DCXX + ESC*DCXYYX) + ES2*DCYY.
    for (int ith = 1; ith <= nth; ++ith) {
      w.fkc[ith - 1] =
          g.ec2[ith - 1] * dcxx + g.esc[ith - 1] * dcxyyx + g.es2[ith - 1] * dcyy;
    }

    // :1153-1159. ISP runs from 1-NTH to NSPEC+NTH, filling VCWN whole: rows
    // IK = 0 and NK+1 use WN, CG and DSDD at the bounds of the column.
    int isp = -nth;
    for (int ik = 0; ik <= nk + 1; ++ik) {
      const Real fkd0 = fkd / cg[ik] * w.dsdd[ik];  // (FKD/CG)*DSDD
      for (int ith = 1; ith <= nth; ++ith) {
        isp = isp + 1;
        w.vcwn[isp + off] = fkd0 + wn[ik] * w.fkc[ith - 1];
      }
    }

    // :1161-1165. High-k tail from the last wavenumber, low-k row zero.
    const int ith0 = nspec - nth;
    for (int ith = 1; ith <= nth; ++ith) {
      w.vaa[ith + nspec + off] = p.fachfa * w.vaa[ith + ith0 + off];
      w.vaa[ith - nth + off] = r0;
    }

    // :1167-1169.
    for (int ik = 1; ik <= nk; ++ik) w.dwni[ik - 1] = cg[ik] / g.dsip[ik];
  }

  // 4. Refraction, :1176-1192, only when FLCTH.
  if (p.flcth != 0) {
    // 4.a, :1180-1184. maxss 0,c: (c > 0) ? c : 0 and minss 0,c:
    // (c < 0) ? c : 0, so a NaN CTHB gives zero flux.
    for (int isp = 1; isp <= nspec; ++isp) {
      const int i2 = g.is2[isp - 1];
      const Real cthb = r05 * (w.vcth[isp - 1] + w.vcth[i2 - 1]);
      w.vflth[isp - 1] = max_r(cthb, r0) * w.vaa[isp + off] + min_r(cthb, r0) * w.vaa[i2 + off];
    }
    // 4.b, :1188-1190. (VA + VFLTH(IS0)) - VFLTH.
    for (int isp = 1; isp <= nspec; ++isp) {
      va[isp - 1] = va[isp - 1] + w.vflth[g.is0[isp - 1] - 1] - w.vflth[isp - 1];
    }
  }

  // 5. Wavenumber shifts, :1196-1214, only when FLCK.
  if (p.flck != 0) {
    // 5.a, :1200-1204. Same MIN/MAX as 4.a.
    for (int isp = 1 - nth; isp <= nspec; ++isp) {
      const Real cwnb = r05 * (w.vcwn[isp + off] + w.vcwn[isp + nth + off]);
      w.vflwn[isp + off] =
          max_r(cwnb, r0) * w.vaa[isp + off] + min_r(cwnb, r0) * w.vaa[isp + nth + off];
    }
    // 5.c, :1208-1211. VA + DWNI*(VFLWN(ISP-NTH) - VFLWN(ISP)); VA already
    // carries the refraction update of 4.b.
    for (int isp = 1; isp <= nspec; ++isp) {
      va[isp - 1] = va[isp - 1] + w.dwni[g.mapwn[isp - 1] - 1] *
                                      (w.vflwn[isp - nth + off] - w.vflwn[isp + off]);
    }
  }
}

}  // namespace ww::prop
