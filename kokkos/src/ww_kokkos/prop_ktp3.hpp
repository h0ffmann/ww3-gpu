// kokkos/src/ww_kokkos/prop_ktp3.hpp
// W3KTP3, ported to Kokkos: propagation in spectral space (refraction in
// direction, wavenumber shift) for one sea point with the third-order QUICKEST
// scheme and the ULTIMATE limiter. Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3pro3md.F90:1519-1961 at NOAA-EMC/WW3
// 03adda91; author H. L. Tolman, the CFL output and the k limiter by F. Ardhuin.
// Only the W3_UQ branch exists under switches/switch_lab_shrd: W3_S, W3_T, W3_T0
// (STRACE, IENT, the test prints), W3_REFRX (the @C/@x refraction, which is why
// DCDX and DCDY are dead) and W3_UNO are not compiled and not ported; a kernel
// never prints.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and assembly of
// w3pro3md.F90 (WW3's own CMake Release flags, switch_lab_shrd; x86-64 baseline,
// so no FMA exists in the object): no libm or libmvec call, no integer power;
// SIGN is __builtin_copysignf, ABS a mask, MIN/MAX are minss/maxss whose operand
// orders are written below per site (spec_W3KTP3.md, section 3). The FRK/FRG,
// FKC and band-width copy loops are vectorised element by element, which changes
// no bits; the only vectorised reduction, CFLKMAX, is translated as a sequential
// max (see 4.c).
#pragma once

#include <Kokkos_Core.hpp>

#include "prop_qck1.hpp"
#include "prop_qck2.hpp"
#include "real.hpp"
#include "st4_params.hpp"  // max_r, min_r

namespace ww::prop {

/// The per-grid scalars and flags W3KTP3 reads (W3GDATMD, W3IDATMD; the USE
/// lists at w3pro3md.F90:1636-1640). ITSTEP (W3ADATMD) is an argument of
/// w3ktp3: it changes every time step.
struct Ktp3Params {
  int nk, nth, nspec, nk2;   // NK, NTH, NSPEC, NK2 = NK+2 (w3gdatmd.F90:2125)
  int flcth, flck, flcur;    // FLCTH, FLCK (W3GDATMD), FLCUR (W3IDATMD INFLAGS1(2)); 0/1
  Real dmin, ctmax, fachfa;  // DMIN, CTMAX (= CFLTM), FACHFA = XFR**(-FACHF-2)
};

/// The per-grid arrays W3KTP3 reads, as the Fortran modules hold them: 0-based
/// pointers at the Fortran's first element, so a non-1 lower bound becomes an
/// offset in the index formula. Read-only here.
struct Ktp3Grid {
  /// SIG(0:NK+1), DSIP(0:NK+1) (w3gdatmd.F90:2066, 2068, allocated 0:MK+1):
  /// SIG(IK) = sig[IK]; NK+2 entries read.
  const Real* sig;
  const Real* dsip;
  /// ECOS, ESIN, EC2, ESC, ES2 (w3gdatmd.F90:2061-2065, allocated NSPEC+NTH,
  /// lower bound 1): X(ISP) = x[ISP-1]. ECOS and ESIN are read at ISP = 1..NSPEC
  /// (:1788); EC2, ESC, ES2 at ISP = 1..NSPEC (:1762) and, in 4.a, at
  /// ITH = 1..NTH (:1822), the first wavenumber row of the same arrays.
  const Real* ecos;
  const Real* esin;
  const Real* ec2;
  const Real* esc;
  const Real* es2;
  /// MAPWN(NSPEC+NTH) (w3gdatmd.F90:2058): MAPWN(ISP) = mapwn[ISP-1], 1-based IK.
  const int* mapwn;
  /// MAPTH2((NK+2)*NTH), MAPWN2(NSPEC+NTH) (w3adatmd.F90:1427-1428), filled by
  /// W3MAP3: 1-based indices into VQ, MAPTH2(ISP) = (IK+1) + (ITH-1)*NK2 for
  /// ISP = ITH + (IK-1)*NTH; MAPWN2 the same for IK = 1..NK-1 then the NK+1 row.
  /// Both are passed to W3QCK1/W3QCK2 as MAPACT/MAPBOU.
  const int* mapth2;
  const int* mapwn2;
};

/// The nine automatic arrays of W3KTP3 (w3pro3md.F90:1673-1677), malloced and
/// freed by the Fortran on every call, plus FLA, the automatic array of W3QCK1
/// and W3QCK2 (one buffer, the two passes run one after the other). Caller
/// owned; ktp3_work carves them from one buffer of ktp3_work_reals entries.
/// Extents and the Fortran index each pointer maps, with NK2 = NK+2:
struct Ktp3Work {
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
                //               VELO(MY*(MX+1)) = NK2*(NTH+1); the largest index W3QCK2 reads with
                //               CLOSE false is NK2*NTH (spec_W3KTP3.md, quirk 6). The extra column
                //               exists so a bounds checker never trips, and is zeroed.
  Real* fla;    // NK2*(NTH+1)   FLA of W3QCK1 (w3uqckmd.F90:171) and W3QCK2 (:630)
};

/// Reals of work storage w3ktp3 needs for one point.
KOKKOS_INLINE_FUNCTION constexpr int ktp3_work_reals(int nk, int nth) {
  const int nk2 = nk + 2;
  return (nk + 2) + nk + nk + nth + nk2 * (nth + 3) + nk2 * (nth + 1) + nk2 * (nth + 2) +
         nk2 * (nth + 1) + nk2 * (nth + 1) + qck1_work_reals(nth, nk2);
}

/// Carve the ten arrays of Ktp3Work out of one buffer of ktp3_work_reals(nk, nth)
/// entries, in the order of the struct.
KOKKOS_INLINE_FUNCTION Ktp3Work ktp3_work(Real* base, int nk, int nth) {
  const int nk2 = nk + 2;
  Ktp3Work w{};
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

/// W3KTP3(ISEA, FACTH, FACK, CTHG0, CG, WN, DW, DDDX, DDDY, CX, CY, DCXDX, DCXDY,
/// DCYDX, DCYDY, DCDX, DCDY, VA, CFLTHMAX, CFLKMAX) for one sea point.
///
/// cg, wn, dcdx, dcdy are CG(0:NK+1,ISEA), WN(0:NK+1,ISEA), DCDX(0:NK+1,IY,IX),
/// DCDY(...) (:1655-1658): x[IK] for IK = 0..NK+1, contiguous columns. va is
/// VA(1:NSPEC) of the point: va[ISP-1], read and written in place. cflthmax and
/// cflkmax are the OUT scalars of :1660. itstep is W3ADATMD ITSTEP, the same for
/// both calls of one W3WAVE step (w3wavemd.F90:1145), so the k/theta order of
/// section 5 is the same in part 1 and part 2 of the step. isea, dcdx and dcdy
/// are never read in this build (ISEA only under W3_T, DCDX/DCDY only under
/// W3_REFRX) and are kept for the shim's argument table.
///
/// Serial: every loop keeps the Fortran's order; the two reductions, CFLTHMAX
/// (:1797) and CFLKMAX (:1851), are in-order maxima of absolute values, which
/// any association reproduces bit for bit for inputs without NaN.
KOKKOS_INLINE_FUNCTION void w3ktp3(const Ktp3Params& p, const Ktp3Grid& g, int itstep,
                                   [[maybe_unused]] int isea, Real facth, Real fack, Real cthg0,
                                   const Real* cg, const Real* wn, Real dw, Real dddx,
                                   Real dddy, Real cx, Real cy, Real dcxdx, Real dcxdy,
                                   Real dcydx, Real dcydy, [[maybe_unused]] const Real* dcdx,
                                   [[maybe_unused]] const Real* dcdy, Real* va, Real& cflthmax,
                                   Real& cflkmax, const Ktp3Work& w) {
  using ww::st4::max_r;
  using ww::st4::min_r;
  constexpr Real r0 = static_cast<Real>(0.);
  constexpr Real r05 = static_cast<Real>(0.5);
  constexpr Real r5 = static_cast<Real>(5.);

  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  const int nk2 = p.nk2;
  // VQ's lower bound is -NK-1 = 1-NK2 (:1675): VQ(I) = vq[I + vqoff]. NK2 is
  // NK+2 in WW3 (w3gdatmd.F90); the offset is taken from NK2 like every extent.
  const int vqoff = nk2 - 1;

  // 1.a, :1688. maxss DW,DMIN: (DMIN > DW) ? DMIN : DW. The caller already
  // passes MAX(DMIN, DW(ISEA)) (w3wavemd.F90:1820), so this is idempotent on
  // finite values.
  const Real depth = max_r(p.dmin, dw);
  // :1689-1691. VQ is zeroed whole (one memset); VCFLT only when FLCTH, CFLK
  // only when FLCK, each over the Fortran extent. The extra column of cflk
  // (its NK2*(NTH+1) dummy, quirk 6) is zeroed with it. DB(:,NTH+1), DM(:,0)
  // and DM(:,NTH+1) are never written, as in the Fortran (malloc garbage
  // there), and never read by W3QCK2 with CLOSE false (quirk 5) ⚠ reasoned
  // from the index ranges, not executed.
  for (int i = 0; i < nk2 * (nth + 3); ++i) w.vq[i] = r0;
  if (p.flcth != 0) {
    for (int i = 0; i < nk2 * (nth + 1); ++i) w.vcflt[i] = r0;
  }
  if (p.flck != 0) {
    for (int i = 0; i < nk2 * (nth + 1); ++i) w.cflk[i] = r0;
  }
  // :1692-1693. Both accumulators live in registers and are stored once at
  // the end of their loops; +0.0 is also the value when the loop is skipped.
  Real acc_th = r0;
  Real acc_k = r0;

  // 2.a, :1704-1710. comiss 5.0 against DEPTH*WN: the zero branch is taken
  // when 5.0 <= DEPTH*WN or unordered, as the Fortran .LT. does; then
  // WN*CG - SIG*0.5, maxss 0,t: (t > 0) ? t : 0, divided by DEPTH.
  for (int ik = 0; ik <= nk + 1; ++ik) {
    if (wn[ik] * depth < r5) {
      const Real t = wn[ik] * cg[ik] - g.sig[ik] * r05;
      w.dsdd[ik] = max_r(t, r0) / depth;
    } else {
      w.dsdd[ik] = r0;
    }
  }

  // 2.b, :1723-1725. Scatter into the rotated, shifted space.
  for (int isp = 1; isp <= nspec; ++isp) w.vq[g.mapth2[isp - 1] + vqoff] = va[isp - 1];

  // 3. Refraction velocities, :1729-1805, only when FLCTH.
  if (p.flcth != 0) {
    // 3.a, :1737. FDDMAX and its loop (:1736, :1740-1742) are dead code: their
    // only consumer (:1747) is commented out, and -O3 removed them (no :1740
    // line marker in the object). Not translated.
    const Real fdg = cthg0 * facth;
    // :1744, :1749. (FACTH*DSDD)/WN, then FDG*CG; vectorised element by element.
    for (int ik = 1; ik <= nk; ++ik) {
      w.frk[ik - 1] = (facth * w.dsdd[ik]) / wn[ik];
      w.frg[ik - 1] = fdg * cg[ik];
    }

    // 3.b, :1754-1767. Current refraction, scattered through MAPTH2.
    if (p.flcur != 0) {
      const Real dcyx = dcydx * facth;
      const Real dcxxyy = (dcxdx - dcydy) * facth;  // the parenthesis is kept, :1757
      const Real dcxy = dcxdy * facth;
      for (int isp = 1; isp <= nspec; ++isp) {
        // :1762. (ES2*DCYX + ESC*DCXXYY) - EC2*DCXY.
        w.vcflt[g.mapth2[isp - 1] - 1] =
            ((g.es2[isp - 1] * dcyx) + (g.esc[isp - 1] * dcxxyy)) - (g.ec2[isp - 1] * dcxy);
      }
    } else {
      // :1766. A second zero fill of the array zeroed at :1690 (quirk 8).
      for (int i = 0; i < nk2 * (nth + 1); ++i) w.vcflt[i] = r0;
    }

    // 3.c, :1785-1804. Total velocity in direction and the CTMAX limiter.
    for (int isp = 1; isp <= nspec; ++isp) {
      const int m2 = g.mapth2[isp - 1];
      const int mw = g.mapwn[isp - 1];
      // :1788. (VCFLT + FRG*ECOS) + FRK*((ESIN*DDDX) - (ECOS*DDDY)).
      const Real velnofilt =
          (w.vcflt[m2 - 1] + w.frg[mw - 1] * g.ecos[isp - 1]) +
          w.frk[mw - 1] * ((g.esin[isp - 1] * dddx) - (g.ecos[isp - 1] * dddy));
      const Real av = Kokkos::fabs(velnofilt);
      // :1797. maxss |v|,acc: (acc > |v|) ? acc : |v|.
      acc_th = max_r(acc_th, av);
      // :1803. minss CTMAX,|v|: (|v| < CTMAX) ? |v| : CTMAX, then copysign.
      // The k limiter at :1852 has the operands the other way round.
      w.vcflt[m2 - 1] = Kokkos::copysign(min_r(av, p.ctmax), velnofilt);
    }
  }

  // 4. Wavenumber shift velocities, :1811-1856, only when FLCK.
  if (p.flck != 0) {
    // 4.a, :1815-1823. Three sign flips (xorps), FKD = CX*DDDX + CY*DDDY, then
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

    // 4.b, :1827-1838. Band widths in column 1, copied to columns 2..NTH.
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

    // 4.c, :1843-1855. The velocity times DT/DK, capped at CTMAX, then divided
    // back by VELFAC (W3QCK2 multiplies by FACK/DB again, :1853); the division
    // is not folded.
    for (int ik = 0; ik <= nk + 1; ++ik) {
      const Real fkd0 = (fkd / cg[ik]) * w.dsdd[ik];  // :1844
      const Real velfac = fack / w.db[ik];            // :1845, DB(IK+1,1)
      // :1850. (WN*FKC + FKD0) * VELFAC, and its absolute value.
      auto velnofilt = [&](int ith) { return ((wn[ik] * w.fkc[ith - 1]) + fkd0) * velfac; };
      for (int ith = 1; ith <= nth; ++ith) {
        const Real v = velnofilt(ith);
        // :1852. minss/minps |v|,CTMAX: (CTMAX < |v|) ? CTMAX : |v|, reversed
        // against :1803; then copysign and the division by VELFAC.
        w.cflk[ik + (ith - 1) * nk2] =
            Kokkos::copysign(min_r(p.ctmax, Kokkos::fabs(v)), v) / velfac;  // CFLK(IK+1,ITH)
      }
      // :1851. -O3 vectorises this MAX over ITH (w3pro3md.o 0x1e56-0x1fea, read
      // in the W3KTP3 review): four lanes started from the running CFLKMAX over
      // the first 4*(NTH/4) directions when NTH >= 4, each `maxps |v|,lane`
      // (lane > |v| ? lane : |v|), folded as max(l2,l0), max(l3,l1), then
      // max(t1,t0); then, for the next pair, two lanes started from t0 =
      // max(l2,l0) and t1 = max(l3,l1) (from the running value when NTH < 4),
      // folded as max(m1,m0); then one scalar `maxss |v|,acc` for an odd last
      // direction (checked against 0x1d26-0x2003 by a second reviewer). The same
      // grouping here gives the Fortran's bits with a NaN
      // too, where a sequential max would not; the velocity is recomputed with
      // the same expression, so it has the same bits as the stored one.
      int ith = 1;
      Real m0 = acc_k, m1 = acc_k;  // two-lane start: acc for NTH < 4, else (t0, t1)
      if (nth >= 4) {
        Real l0 = acc_k, l1 = acc_k, l2 = acc_k, l3 = acc_k;
        const int n4 = (nth / 4) * 4;
        for (; ith <= n4; ith += 4) {
          l0 = max_r(l0, Kokkos::fabs(velnofilt(ith)));
          l1 = max_r(l1, Kokkos::fabs(velnofilt(ith + 1)));
          l2 = max_r(l2, Kokkos::fabs(velnofilt(ith + 2)));
          l3 = max_r(l3, Kokkos::fabs(velnofilt(ith + 3)));
        }
        m0 = max_r(l2, l0);  // t0
        m1 = max_r(l3, l1);  // t1
        acc_k = max_r(m1, m0);
      }
      if (nth - ith + 1 >= 2) {
        m0 = max_r(m0, Kokkos::fabs(velnofilt(ith)));
        m1 = max_r(m1, Kokkos::fabs(velnofilt(ith + 1)));
        ith += 2;
        acc_k = max_r(m1, m0);
      }
      if (ith <= nth) acc_k = max_r(acc_k, Kokkos::fabs(velnofilt(ith)));
    }
  }
  cflthmax = acc_th;
  cflkmax = acc_k;

  // 5. Propagate, :1861-1933. The k pass first on an even ITSTEP, the theta
  // pass first on an odd one. Before each k pass the high-k boundary row
  // (IK = NK+1, VQ row NK+2) is the last wavenumber times the tail factor
  // (:1864, :1915); row 1 (IK = 0) stays 0 from the memset.
  // W3QCK2 (:1871, :1922): the rotated grid with INC = 1 (wavenumber is the
  // fast index), CLOSE false, DT = FACK, NB0 = NSPEC-NTH, NB1 = NSPEC,
  // NB2 = NSPEC+NTH. W3QCK1 (:1887, :1903): INC = NK2, CLOSE true (periodic in
  // direction), NB0 = NB1 = NB2 = NSPEC. NDSE and NDST are not read by either.
  // The pointer bases match the dummies' lower bounds: VQ and DM start at
  // 1-NK2 as Q and DX2 do; CFLK, DB and VCFLT at 1 as VELO, DX1 and CFLL do.
  if (itstep % 2 == 0) {
    if (p.flck != 0) {
      for (int ith = 1; ith <= nth; ++ith) {  // :1863-1865
        w.vq[nk + 2 + (ith - 1) * nk2 + vqoff] = p.fachfa * w.vq[nk + 1 + (ith - 1) * nk2 + vqoff];
      }
      w3qck2(nth, nk2, nth, nk2, w.cflk, fack, w.db, w.dm, w.vq, false, 1, g.mapth2, nspec,
             g.mapwn2, nspec - nth, nspec, nspec + nth, w.fla);  // :1871
    }
    if (p.flcth != 0) {
      w3qck1(nth, nk2, nth, nk2, w.vcflt, w.vq, true, nk2, g.mapth2, nspec, g.mapth2, nspec,
             nspec, nspec, w.fla);  // :1887
    }
  } else {
    if (p.flcth != 0) {
      w3qck1(nth, nk2, nth, nk2, w.vcflt, w.vq, true, nk2, g.mapth2, nspec, g.mapth2, nspec,
             nspec, nspec, w.fla);  // :1903
    }
    if (p.flck != 0) {
      for (int ith = 1; ith <= nth; ++ith) {  // :1914-1916
        w.vq[nk + 2 + (ith - 1) * nk2 + vqoff] = p.fachfa * w.vq[nk + 1 + (ith - 1) * nk2 + vqoff];
      }
      w3qck2(nth, nk2, nth, nk2, w.cflk, fack, w.db, w.dm, w.vq, false, 1, g.mapth2, nspec,
             g.mapwn2, nspec - nth, nspec, nspec + nth, w.fla);  // :1922
    }
  }

  // 6. Store, :1937-1939. Gather back from the rotated space.
  for (int isp = 1; isp <= nspec; ++isp) va[isp - 1] = w.vq[g.mapth2[isp - 1] + vqoff];
}

}  // namespace ww::prop
