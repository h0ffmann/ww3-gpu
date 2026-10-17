// kokkos/src/ww_kokkos/prop_ktp2.hpp
// W3KTP2, ported to Kokkos: propagation in spectral space (refraction in
// direction, wavenumber shift) for one sea point with the third-order QUICKEST
// scheme and the ULTIMATE limiter, the PR2 sibling of W3KTP3. Phase 1,
// translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3pro2md.F90:1306-1710 at NOAA-EMC/WW3
// 03adda91 (identical at the fork pin 357f575c); author H. L. Tolman. Only the
// W3_UQ branch exists under switch_lab_shrd with PR3 replaced by PR2: W3_S,
// W3_T, W3_T0 (STRACE, IENT, the test prints), W3_REFRX (the @C/@x refraction,
// which is why DCDX and DCDY are dead) and W3_UNO are not compiled and not
// ported; a kernel never prints.
//
// Against W3KTP3 (prop_ktp3.hpp): no CFL outputs, DEPTH arrives already
// limited (no MAX with DMIN here), VQ is zeroed only when FLCK and VCFLT never,
// the depth-refraction slope filter FDDMAX is live, and the theta velocity has
// no CTMAX cap and the k velocity no VELFAC.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and assembly of
// w3pro2md.F90 (WW3's own CMake Release flags, x86-64 baseline, so no FMA
// exists in the object): no libm or libmvec call, no integer power; ABS is a
// mask, MIN/MAX are maxss/maxps whose operand orders are written below per
// site. The FRK/FRG, FKC and CFLK loops are vectorised element by element,
// which changes no bits; the only vectorised reduction, FDDMAX, is translated
// with the object's grouping (see 3.a).
#pragma once

#include <Kokkos_Core.hpp>

#include "prop_qck1.hpp"
#include "prop_qck2.hpp"
#include "real.hpp"
#include "st4_params.hpp"  // max_r

namespace ww::prop {

/// The per-grid scalars and flags W3KTP2 reads (W3GDATMD, W3IDATMD; the USE
/// lists at w3pro2md.F90:1416-1420). ITSTEP (W3ADATMD) is an argument of
/// w3ktp2: it changes every time step. ITIME is in the USE list and never read.
struct Ktp2Params {
  int nk, nth, nspec, nk2;  // NK, NTH, NSPEC, NK2 = NK+2 (w3gdatmd.F90:2125)
  int flcth, flck, flcur;   // FLCTH, FLCK (w3gdatmd.F90:2389-2390), FLCUR (w3idatmd.F90:572); 0/1
  Real ctmax, fachfa;       // CTMAX (w3gdatmd.F90:2357), FACHFA = XFR**(-FACHF-2) (w3gridmd.F90:3564)
};

/// The per-grid arrays W3KTP2 reads, as the Fortran modules hold them: 0-based
/// pointers at the Fortran's first element, so a non-1 lower bound becomes an
/// offset in the index formula. Read-only here.
struct Ktp2Grid {
  /// SIG(0:NK+1), DSIP(0:NK+1) (w3gdatmd.F90:2066, 2068, allocated 0:MK+1):
  /// SIG(IK) = sig[IK]; NK+2 entries read.
  const Real* sig;
  const Real* dsip;
  /// ECOS, ESIN, EC2, ESC, ES2 (w3gdatmd.F90:2061-2065, allocated NSPEC+NTH,
  /// lower bound 1): X(ISP) = x[ISP-1]. ECOS and ESIN are read at
  /// ITH = 1..NTH/2 (:1511) and ISP = 1..NSPEC (:1523-1524); EC2, ESC, ES2 at
  /// ISP = 1..NSPEC (:1558) and, in 4.a, at ITH = 1..NTH (:1578-1579): in both
  /// ITH loops the first wavenumber row of the same arrays.
  const Real* ecos;
  const Real* esin;
  const Real* ec2;
  const Real* esc;
  const Real* es2;
  /// MAPWN(NSPEC+NTH) (w3gdatmd.F90:2058): MAPWN(ISP) = mapwn[ISP-1], 1-based IK.
  const int* mapwn;
  /// MAPTH2((NK+2)*NTH), MAPWN2(NSPEC+NTH) (w3adatmd.F90:1427-1428), filled by
  /// W3MAP2 (w3pro2md.F90:416-463): 1-based indices into VQ,
  /// MAPTH2(ISP) = (IK+1) + (ITH-1)*(NK+2) for ISP = ITH + (IK-1)*NTH; MAPWN2
  /// the same for IK = 1..NK-1, then the IK = NK row at VQ row NK+1, then
  /// VQ row 1. Both are passed to W3QCK1/W3QCK2 as MAPACT/MAPBOU.
  const int* mapth2;
  const int* mapwn2;
};

/// The nine automatic arrays of W3KTP2 (w3pro2md.F90:1453-1456), malloced and
/// freed by the Fortran on every call, plus FLA, the automatic array of W3QCK1
/// and W3QCK2 (one buffer, the two passes run one after the other). Caller
/// owned; ktp2_work carves them from one buffer of ktp2_work_reals entries.
/// Extents and the Fortran index each pointer maps, with NK2 = NK+2:
struct Ktp2Work {
  Real* dsdd;   // NK+2          DSDD(0:NK+1):            DSDD(IK)   = dsdd[IK]
  Real* frk;    // NK            FRK(NK):                 FRK(IK)    = frk[IK-1]
  Real* frg;    // NK            FRG(NK):                 FRG(IK)    = frg[IK-1]
  Real* fkc;    // NTH           FKC(NTH):                FKC(ITH)   = fkc[ITH-1]
  Real* vq;     // NK2*(NTH+3)   VQ(-NK-1:NK2*(NTH+2)):   VQ(I)      = vq[I + NK + 1]
  Real* db;     // NK2*(NTH+1)   DB(NK2,NTH+1):           DB(IK,ITH) = db[(IK-1) + (ITH-1)*NK2]
  Real* dm;     // NK2*(NTH+2)   DM(NK2,0:NTH+1):         DM(IK,ITH) = dm[(IK-1) + ITH*NK2]
  Real* vcflt;  // NK2*(NTH+1)   VCFLT(NK2*(NTH+1)):      VCFLT(I)   = vcflt[I-1]
  Real* cflk;   // NK2*(NTH+1)   CFLK(NK2,NTH):           CFLK(IK,ITH) = cflk[(IK-1) + (ITH-1)*NK2]
                //               The Fortran array has NK2*NTH entries but is passed to W3QCK2's
                //               VELO(MY*(MX+1)) = NK2*(NTH+1) (w3uqckmd.F90:614); with CLOSE false
                //               the largest index W3QCK2 reads is NK2*NTH. The extra column exists
                //               so the dummy's extent is real storage; nothing writes or reads it.
  Real* fla;    // NK2*(NTH+1)   FLA of W3QCK1 (w3uqckmd.F90:232) and W3QCK2 (:630)
};

/// Reals of work storage w3ktp2 needs for one point.
KOKKOS_INLINE_FUNCTION constexpr int ktp2_work_reals(int nk, int nth) {
  const int nk2 = nk + 2;
  return (nk + 2) + nk + nk + nth + nk2 * (nth + 3) + nk2 * (nth + 1) + nk2 * (nth + 2) +
         nk2 * (nth + 1) + nk2 * (nth + 1) + qck1_work_reals(nth, nk2);
}

/// Carve the ten arrays of Ktp2Work out of one buffer of ktp2_work_reals(nk, nth)
/// entries, in the order of the struct.
KOKKOS_INLINE_FUNCTION Ktp2Work ktp2_work(Real* base, int nk, int nth) {
  const int nk2 = nk + 2;
  Ktp2Work w{};
  w.dsdd = base;
  w.frk = w.dsdd + (nk + 2);
  w.frg = w.frk + nk;
  w.fkc = w.frg + nk;
  w.vq = w.fkc + nth;
  w.db = w.vq + nk2 * (nth + 3);
  w.dm = w.db + nk2 * (nth + 1);
  w.vcflt = w.dm + nk2 * (nth + 2);
  w.cflk = w.vcflt + nk2 * (nth + 1);
  w.fla = w.cflk + nk2 * (nth + 1);
  return w;
}

/// W3KTP2(ISEA, FACTH, FACK, CTHG0, CG, WN, DEPTH, DDDX, DDDY, CX, CY, DCXDX,
/// DCXDY, DCYDX, DCYDY, DCDX, DCDY, VA) for one sea point.
///
/// cg, wn, dcdx, dcdy are CG(0:NK+1), WN(0:NK+1), DCDX(0:NK+1), DCDY(0:NK+1)
/// (:1439-1441): x[IK] for IK = 0..NK+1, contiguous columns. depth is the
/// caller's MAX(DMIN, DW(ISEA)) (w3wavemd.F90:1819), used as is. va is
/// VA(1:NSPEC) of the point: va[ISP-1], read and written in place. itstep is
/// W3ADATMD ITSTEP, the same for both calls of one W3WAVE step
/// (w3wavemd.F90:1145). isea, dcdx and dcdy are never read in this build (ISEA
/// only under W3_T, DCDX/DCDY only under W3_REFRX) and are kept for the shim's
/// argument table.
///
/// Serial: every loop keeps the Fortran's order; the one reduction, FDDMAX
/// (:1511), keeps the object's four-lane grouping.
KOKKOS_INLINE_FUNCTION void w3ktp2(const Ktp2Params& p, const Ktp2Grid& g, int itstep,
                                   [[maybe_unused]] int isea, Real facth, Real fack, Real cthg0,
                                   const Real* cg, const Real* wn, Real depth, Real dddx,
                                   Real dddy, Real cx, Real cy, Real dcxdx, Real dcxdy,
                                   Real dcydx, Real dcydy, [[maybe_unused]] const Real* dcdx,
                                   [[maybe_unused]] const Real* dcdy, Real* va,
                                   const Ktp2Work& w) {
  using ww::st4::max_r;
  constexpr Real r0 = static_cast<Real>(0.);
  constexpr Real r05 = static_cast<Real>(0.5);
  constexpr Real r1 = static_cast<Real>(1.);
  constexpr Real r5 = static_cast<Real>(5.);

  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  const int nk2 = p.nk2;
  // VQ's lower bound is -NK-1 (:1454): VQ(I) = vq[I + vqoff]. The object takes
  // it from NK (~NK), the extent from NK2, as the declaration does.
  const int vqoff = nk + 1;

  // 1.a, :1467. VQ is zeroed (one memset) only when FLCK. Without FLCK the
  // entries no scatter writes, rows 1 and NK+2 of every column and columns 0,
  // NTH+1 and NTH+2, keep whatever the buffer held (malloc garbage in the
  // Fortran). W3QCK1 copies them in its closure but its stencil, shifted by
  // multiples of INC = NK2, reads only rows 2..NK+1 (w3uqckmd.F90:292-431), and
  // so does the store (v) by the fresh reviewer from those lines, not executed. VCFLT is
  // never zeroed either, unlike W3KTP3 (w3pro3md.F90:1689-1690): its rows 1 and
  // NK+2 and column NTH+1 keep the buffer's contents and are not read by the
  // same argument. CFLK is written whole in 4.b; DB(:,NTH+1), DM(:,0) and
  // DM(:,NTH+1) are never written.
  if (p.flck != 0) {
    for (int i = 0; i < nk2 * (nth + 3); ++i) w.vq[i] = r0;
  }

  // 2.a, :1478-1485. comiss 5.0 against DEPTH*WN: the zero branch is taken
  // when 5.0 <= DEPTH*WN or unordered, as the Fortran .LT. does; then
  // WN*CG - SIG*0.5, maxss 0,t: (t > 0) ? t : 0, divided by DEPTH.
  for (int ik = 0; ik <= nk + 1; ++ik) {
    if (depth * wn[ik] < r5) {
      const Real t = wn[ik] * cg[ik] - g.sig[ik] * r05;
      w.dsdd[ik] = max_r(t, r0) / depth;
    } else {
      w.dsdd[ik] = r0;
    }
  }

  // 2.b, :1497-1499. Scatter into the rotated, shifted space.
  for (int isp = 1; isp <= nspec; ++isp) w.vq[g.mapth2[isp - 1] + vqoff] = va[isp - 1];

  // 3. Refraction velocities, :1503-1563, only when FLCTH.
  if (p.flcth != 0) {
    // 3.a, :1507-1512. FDDMAX over the first NTH/2 directions. -O3 vectorises
    // the MAX when NTH/2 >= 4 (w3pro2md.o 0xe80-0xf00): four lanes from +0.0
    // over the first 4*((NTH/2)/4) directions, each `maxps |v|,lane`
    // (lane > |v| ? lane : |v|), folded as max(l2,l0), max(l3,l1), then
    // max(t1,t0); then one scalar `maxss |v|,acc` per remaining direction
    // (0xf84, 0xfcf, 0x100d). With NTH/2 <= 3 only the scalar form runs, from
    // +0.0 (0x1dc6). The same grouping here gives the Fortran's bits with a NaN
    // too, where a sequential max would not.
    const Real fdg = cthg0 * facth;  // :1508
    const int nh = nth / 2;
    auto fdd = [&](int ith) {
      return Kokkos::fabs((g.esin[ith - 1] * dddx) - (g.ecos[ith - 1] * dddy));
    };
    Real fddmax = r0;
    int ith = 1;
    if (nh >= 4) {
      Real l0 = r0, l1 = r0, l2 = r0, l3 = r0;
      const int n4 = (nh / 4) * 4;
      for (; ith <= n4; ith += 4) {
        l0 = max_r(l0, fdd(ith));
        l1 = max_r(l1, fdd(ith + 1));
        l2 = max_r(l2, fdd(ith + 2));
        l3 = max_r(l3, fdd(ith + 3));
      }
      const Real t0 = max_r(l2, l0);
      const Real t1 = max_r(l3, l1);
      fddmax = max_r(t1, t0);
    }
    for (; ith <= nh; ++ith) fddmax = max_r(fddmax, fdd(ith));

    // :1514-1518. (FACTH*DSDD)/WN, then divided by MAX(1., (FDDMAX*FRK)/CTMAX):
    // maxps/maxss 1.0,x: (x > 1) ? x : 1, so a NaN quotient gives 1 (0x111b,
    // 0x11c2, 0x1223, 0x1ce6); then FDG*CG. Vectorised element by element.
    for (int ik = 1; ik <= nk; ++ik) {
      const Real frk0 = (facth * w.dsdd[ik]) / wn[ik];
      w.frk[ik - 1] = frk0 / max_r((fddmax * frk0) / p.ctmax, r1);
      w.frg[ik - 1] = fdg * cg[ik];
    }

    // 3.b, :1522-1525. FRG*ECOS + FRK*((ESIN*DDDX) - (ECOS*DDDY)), scattered
    // through MAPTH2; no cap on the theta velocity in W3KTP2.
    for (int isp = 1; isp <= nspec; ++isp) {
      const int mw = g.mapwn[isp - 1];
      w.vcflt[g.mapth2[isp - 1] - 1] =
          (w.frg[mw - 1] * g.ecos[isp - 1]) +
          w.frk[mw - 1] * ((g.esin[isp - 1] * dddx) - (g.ecos[isp - 1] * dddy));
    }

    // 3.d, :1550-1561. Current refraction added in place.
    if (p.flcur != 0) {
      const Real dcyx = dcydx * facth;
      const Real dcxxyy = (dcxdx - dcydy) * facth;  // the parenthesis is kept, :1553
      const Real dcxy = dcxdy * facth;
      for (int isp = 1; isp <= nspec; ++isp) {
        const int m2 = g.mapth2[isp - 1];
        // :1557-1558. ((VCFLT + ES2*DCYX) + ESC*DCXXYY) - EC2*DCXY.
        w.vcflt[m2 - 1] = ((w.vcflt[m2 - 1] + g.es2[isp - 1] * dcyx) +
                           g.esc[isp - 1] * dcxxyy) -
                          g.ec2[isp - 1] * dcxy;
      }
    }
  }

  // 4. Wavenumber shift velocities, :1568-1607, only when FLCK.
  if (p.flck != 0) {
    // 4.a, :1572-1580. Three sign flips (xorps), FKD = CX*DDDX + CY*DDDY, then
    // FKC = (EC2*DCXX + ESC*DCXYYX) + ES2*DCYY over the first NTH entries of
    // the trig arrays; vectorised element by element.
    const Real dcxx = -dcxdx;
    const Real dcxyyx = -(dcxdy + dcydx);
    const Real dcyy = -dcydy;
    const Real fkd = (cx * dddx) + (cy * dddy);
    for (int ith = 1; ith <= nth; ++ith) {
      w.fkc[ith - 1] =
          ((g.ec2[ith - 1] * dcxx) + (g.esc[ith - 1] * dcxyyx)) + (g.es2[ith - 1] * dcyy);
    }

    // 4.b, :1584-1589. FKD0 = (FKD/CG)*DSDD, then FKD0 + WN*FKC with no limiter;
    // W3QCK2 turns it into a Courant number, DT*VEL/DX2 (w3uqckmd.F90:713-714).
    for (int ik = 0; ik <= nk + 1; ++ik) {
      const Real fkd0 = (fkd / cg[ik]) * w.dsdd[ik];  // :1585
      for (int ith = 1; ith <= nth; ++ith) {
        w.cflk[ik + (ith - 1) * nk2] = fkd0 + wn[ik] * w.fkc[ith - 1];  // CFLK(IK+1,ITH)
      }
    }

    // 4.c, :1593-1605. Band widths in column 1, copied to columns 2..NTH.
    for (int ik = 0; ik <= nk; ++ik) {
      w.db[ik] = g.dsip[ik] / cg[ik];            // DB(IK+1,1)
      w.dm[ik + nk2] = wn[ik + 1] - wn[ik];      // DM(IK+1,1)
    }
    w.db[nk + 1] = g.dsip[nk + 1] / cg[nk + 1];  // DB(NK+2,1)
    w.dm[nk + 1 + nk2] = r0;                     // DM(NK+2,1)
    for (int ith = 2; ith <= nth; ++ith) {
      for (int ik = 1; ik <= nk + 2; ++ik) {
        w.db[(ik - 1) + (ith - 1) * nk2] = w.db[ik - 1];
        w.dm[(ik - 1) + ith * nk2] = w.dm[(ik - 1) + nk2];
      }
    }
  }

  // 5. Propagate, :1611-1682. The k pass first on an even ITSTEP, the theta
  // pass first on an odd one (MOD(ITSTEP,2) is `and 1` in the object, which
  // agrees for negative ITSTEP too). Before each k pass the high-k boundary
  // row (IK = NK+1, VQ row NK+2) is the last wavenumber times the tail factor
  // (:1614, :1664); row 1 (IK = 0) stays 0 from the memset.
  // W3QCK2 (:1618, :1668): the rotated grid with INC = 1 (wavenumber is the
  // fast index), CLOSE false, DT = FACK, NB0 = NSPEC-NTH, NB1 = NSPEC,
  // NB2 = NSPEC+NTH. W3QCK1 (:1634, :1650): INC = NK2, CLOSE true (periodic in
  // direction), NB0 = NB1 = NB2 = NSPEC. NDSE and NDST are not read by either.
  // The pointer bases match the dummies' lower bounds: VQ and DM start at
  // 1-NK2 as Q and DX2 do; CFLK, DB and VCFLT at 1 as VELO, DX1 and CFLL do.
  if (itstep % 2 == 0) {
    if (p.flck != 0) {
      for (int ith = 1; ith <= nth; ++ith) {  // :1613-1615
        w.vq[nk + 2 + (ith - 1) * nk2 + vqoff] = p.fachfa * w.vq[nk + 1 + (ith - 1) * nk2 + vqoff];
      }
      w3qck2(nth, nk2, nth, nk2, w.cflk, fack, w.db, w.dm, w.vq, false, 1, g.mapth2, nspec,
             g.mapwn2, nspec - nth, nspec, nspec + nth, w.fla);  // :1618
    }
    if (p.flcth != 0) {
      w3qck1(nth, nk2, nth, nk2, w.vcflt, w.vq, true, nk2, g.mapth2, nspec, g.mapth2, nspec,
             nspec, nspec, w.fla);  // :1634
    }
  } else {
    if (p.flcth != 0) {
      w3qck1(nth, nk2, nth, nk2, w.vcflt, w.vq, true, nk2, g.mapth2, nspec, g.mapth2, nspec,
             nspec, nspec, w.fla);  // :1650
    }
    if (p.flck != 0) {
      for (int ith = 1; ith <= nth; ++ith) {  // :1663-1665
        w.vq[nk + 2 + (ith - 1) * nk2 + vqoff] = p.fachfa * w.vq[nk + 1 + (ith - 1) * nk2 + vqoff];
      }
      w3qck2(nth, nk2, nth, nk2, w.cflk, fack, w.db, w.dm, w.vq, false, 1, g.mapth2, nspec,
             g.mapwn2, nspec - nth, nspec, nspec + nth, w.fla);  // :1668
    }
  }

  // 6. Store, :1686-1688. Gather back from the rotated space.
  for (int isp = 1; isp <= nspec; ++isp) va[isp - 1] = w.vq[g.mapth2[isp - 1] + vqoff];
}

}  // namespace ww::prop
