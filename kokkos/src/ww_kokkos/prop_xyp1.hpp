// kokkos/src/ww_kokkos/prop_xyp1.hpp
// W3XYP1, ported to Kokkos: first-order upwind flux propagation in physical
// space of one spectral bin on a regular (or curvilinear) grid. Phase 1,
// translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3pro1md.F90:301-873 at NOAA-EMC/WW3
// 03adda91; author H. L. Tolman, curvilinear version W. E. Rogers and T. J.
// Campbell. W3_S, W3_T* output, W3_OMPH directives, the W3_MGP moving grid
// terms and the W3_PDLIB boundary branch (:820-822) are not in switch_lab_shrd
// with PR1 and are not ported. The tripole closure (ICLOSE_TRPL) is: W3XYP1,
// unlike W3XYP3, handles it (:642-648, :709-727).
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and assembly of
// w3pro1md.F90 (WW3's own CMake Release flags, switch_lab_shrd with "PR3 UQ"
// replaced by "PR1"): no libm call, sqrtss inline for the two SQRTs, no FMA,
// the MAXVALs of :500-502 inlined as in-order scans, 0.575*GRAV and DERA*RADIUS
// folded, every MIN/MAX in the operand order of the emitted maxss/minss. The
// only vectorised loops are the zero-fills, the ghost copies and the GSQRT
// multiply and divide, elementwise and so bit-identical to a scalar loop. The
// whole routine runs serially for one ISP, in source order, as W3WAVE calls it
// (w3wavemd.F90:1985, no OpenMP there under this switch).
#pragma once

#include <Kokkos_Core.hpp>

#include "prop_params.hpp"
#include "prop_params1.hpp"
#include "real.hpp"
#include "st4_params.hpp"  // fint, max_r, min_r

namespace ww::prop {

/// Reals of work storage w3xyp1 needs, for the Fortran's automatic arrays
/// (w3pro1md.F90:474-480, malloc'ed per call), in this order:
///   cxtot2d, cytot2d                NY*NX each           CXTOT2D, CYTOT2D(NY,NX)
///   fld2d                           (NY+1)*(NX+1)        FLD2D(NY+1,NX+1)
///   vcx2d                           NY*(NX+1)            VCX2D(NY,NX+1)
///   vcy2d                           (NY+1)*NX            VCY2D(NY+1,NX)
///   vflx2d                          NY*(NX+1)            VFLX2D(1:NY,0:NX)
///   pad                             1                    the word before VFLY2D, see below
///   vfly2d                          NY*NX                VFLY2D(NY,NX)
KOKKOS_INLINE_FUNCTION constexpr int xyp1_work_reals(int nx, int ny) {
  return 2 * nx * ny + (ny + 1) * (nx + 1) + ny * (nx + 1) + (ny + 1) * nx + ny * (nx + 1) + 1 +
         nx * ny;
}

namespace xyp1_detail {

/// X(IY,IX) of a column-major work array with leading dimension ld and lower
/// bounds (1,1).
KOKKOS_INLINE_FUNCTION int at(int iy, int ix, int ld) { return (ix - 1) * ld + iy - 1; }

/// gfortran's inline MAXVAL((CGL*ECOS(ITH)+CX(1:NSEA))**2+(CGL*ESIN(ITH)+CY(1:NSEA))**2)
/// (:500-501; GIMPLE bb 3-12, object 0x2872-0x2a07): -HUGE for an empty section;
/// the scan for the first element that is not NaN (x >= -Inf) starts the
/// reduction there, NaN when there is none; then in index order, maxss %limit,%x:
/// x > limit ? x : limit, so a NaN x keeps the limit. The element is
/// (CX+ECOS*CGL)*(CX+ECOS*CGL) + (CY+ESIN*CGL)*(CY+ESIN*CGL), **2 a multiply,
/// ECOS*CGL and ESIN*CGL hoisted (a, b). Not vectorised, not reassociated.
KOKKOS_INLINE_FUNCTION Real maxval_cga(const Real* cx, const Real* cy, int n, Real a, Real b) {
  if (n <= 0) return -Kokkos::finite_max_v<Real>;
  int i = 0;
  Real x = Kokkos::quiet_NaN_v<Real>;
  for (; i < n; ++i) {
    const Real u = cx[i] + a;
    const Real v = cy[i] + b;
    x = u * u + v * v;
    if (x >= -Kokkos::infinity_v<Real>) break;
  }
  if (i == n) return Kokkos::quiet_NaN_v<Real>;
  Real limit = x;
  for (++i; i < n; ++i) {
    const Real u = cx[i] + a;
    const Real v = cy[i] + b;
    const Real y = u * u + v * v;
    limit = (y > limit) ? y : limit;
  }
  return limit;
}

/// MAXVAL(CX(1:NSEA)**2+CY(1:NSEA)**2) likewise (:502; bb 14-19).
KOKKOS_INLINE_FUNCTION Real maxval_cc(const Real* cx, const Real* cy, int n) {
  if (n <= 0) return -Kokkos::finite_max_v<Real>;
  int i = 0;
  Real x = Kokkos::quiet_NaN_v<Real>;
  for (; i < n; ++i) {
    x = cx[i] * cx[i] + cy[i] * cy[i];
    if (x >= -Kokkos::infinity_v<Real>) break;
  }
  if (i == n) return Kokkos::quiet_NaN_v<Real>;
  Real limit = x;
  for (++i; i < n; ++i) {
    const Real y = cx[i] * cx[i] + cy[i] * cy[i];
    limit = (y > limit) ? y : limit;
  }
  return limit;
}

}  // namespace xyp1_detail

/// W3XYP1(ISP, DTG, MAPSTA, FIELD, VGX, VGY) for one spectral bin. VGX, VGY
/// are read only under W3_MGP (:503-512, :570-573) and are left out.
///
/// Fortran arrays arrive 0-based; the explicit lower bounds become offsets:
///   mapsta -> MAPSTA(NY*NX):           MAPSTA(i) = mapsta[i - 1]
///   field  -> FIELD(1-NY : NY*(NX+2)): FIELD(i)  = field[i + ny - 1], the caller's
///             FIELD (w3wavemd.F90:763); only FIELD(IXY) of sea points is read
///             and written
///   wr     -> xyp1_work_reals entries, caller-owned; the routine zeroes every
///             work array where the Fortran does (:539-545) and the pad once.
/// p, g, s: prop_params.hpp; f: prop_params1.hpp. s.dsec_day is not read. The
/// caller checks s.nbi > 0 only when s.flbpi, and computes the two DSEC21
/// values of Step.
///
/// The flux loops are gathers and the propagation reads only its own cell, so
/// a parallel version over IX, IY or ISEA would keep the bits; the boundary
/// loop (:817-826) stores in IBI order, a point listed twice taking the last
/// write. Phase 1 keeps the serial body in source order.
KOKKOS_INLINE_FUNCTION void w3xyp1(const Params& p, const Grid& g, const Facv& f, const Step& s,
                                   int isp, Real dtg, const int* mapsta, Real* field, Real* wr) {
  using ww::st4::fint;
  using ww::st4::max_r;
  using ww::st4::min_r;
  using xyp1_detail::at;
  constexpr Real r0 = static_cast<Real>(0.);
  constexpr Real r1 = static_cast<Real>(1.);
  constexpr Real r1em3 = static_cast<Real>(0.001);
  constexpr Real r09999 = static_cast<Real>(0.9999);
  static_assert(r1em3 == 1.000000047497451305389404296875e-3f, "0.001 as REAL, 0x3a83126f");
  static_assert(r09999 == 9.9989998340606689453125e-1f, "0.9999 as REAL, 0x3f7ff972");

  const int nx = p.nx;
  const int ny = p.ny;
  const int nsea = p.nsea;
  const int nxy = nx * ny;
  const int ld1 = ny + 1;          // leading dimension of FLD2D and VCY2D
  const int foff = ny - 1;         // FIELD(i) -> [i + foff]
  const int cgld = p.nk + 2;       // CG(IK,ISEA) -> cg[ISEA*cgld + IK]
  const bool trpl = p.iclose == kIcloseTrpl;

  // Work arrays, in the order xyp1_work_reals documents.
  Real* const cxtot2d = wr;
  Real* const cytot2d = cxtot2d + nxy;
  Real* const fld2d = cytot2d + nxy;
  Real* const vcx2d = fld2d + (ny + 1) * (nx + 1);
  Real* const vcy2d = vcx2d + ny * (nx + 1);
  Real* const vflx2d = vcy2d + (ny + 1) * nx;  // VFLX2D(IY,IX) = vflx2d[IX*ny + IY-1], IX from 0
  Real* const pad = vflx2d + ny * (nx + 1);
  Real* const vfly2d = pad + 1;
  // ⚠ :761 reads VFLY2D(IY-1,IX), which for IY = 1 is VFLY2D(NY,IX-1) when
  // IX > 1 (in bounds, column-major; 0 from :545 unless the tripole loop
  // :709-727 wrote it) and the 4 bytes before the array when IX = 1 (out of
  // bounds; the object indexes it, GIMPLE bb 152, offset -1). The Fortran's
  // automatic array is a malloc block (GIMPLE bb 2). On glibc x86-64 those
  // bytes should be the upper half of the chunk size field, 0 below 4 GiB, and
  // the pad models them as +0.0: an assumption about the allocator, not a
  // reproduction. The read is inside the MAPSTA == 1 branch; ww3_grid, when it
  // reads the map in parts (FROM = 'PART', w3gridmd.F90:4565), makes active
  // points on rows 1 and NY excluded (:4773-4776). A map read from file
  // (:4793 on, the only way for a tripole grid, :4570-4575) is not checked.
  *pad = r0;

  // 1.a Set constants (:493-521).
  const int ith = 1 + (isp - 1) % p.nth;  // :493
  const int ik = 1 + (isp - 1) / p.nth;   // :494
  const Real cg0 = kCg0Fac / g.sig[1];    // :496, 0.575*GRAV folded
  const Real cgl = kCg0Fac / g.sig[ik];   // :497
  const Real ecosi = g.ecos[ith - 1];
  const Real esini = g.esin[ith - 1];

  Real cga;
  Real cc;
  if (p.flcur != 0) {
    // :500-502. SQRT is sqrtss; the empty section gives SQRT(-HUGE), NaN.
    cga = Kokkos::sqrt(xyp1_detail::maxval_cga(s.cx + 1, s.cy + 1, nsea, ecosi * cgl, esini * cgl));
    cc = Kokkos::sqrt(xyp1_detail::maxval_cc(s.cx + 1, s.cy + 1, nsea));
  } else {
    cga = cgl;  // :509
    cc = r0;    // :513
  }
  // :516, MAX(CGA, CC, 0.001*CG0) emitted as maxss %cga,%cc then
  // maxss %e,%t (object 0x2ab7, 0x2add): each is B > A ? B : A with B the
  // destination, so CC first, then the 0.001*CG0 term last.
  const Real t = max_r(cc, cga);
  const Real e = cg0 * r1em3;
  const Real cgn = max_r(t, e) * r09999;
  // :518, DTCFL*CG0 first, then / CGN, then DTG / that; INT is cvttss2si. CG0,
  // the group speed at the first frequency, is the CFL reference for every IK.
  const int ntloc = 1 + fint(dtg / ((p.dtcfl * cg0) / cgn));
  const Real rntloc = static_cast<Real>(ntloc);  // one REAL(NTLOC), shared by :519 and :808
  const Real dtloc = dtg / rntloc;                // :519
  Real dtrad = dtloc;                             // :520
  if (p.flagll != 0) dtrad = dtrad / kDeraRadius;  // :521, DERA*RADIUS folded

  // Loop over sub-steps (:531-840). Each starts again from FIELD.
  for (int itloc = 1; itloc <= ntloc; ++itloc) {
    // 1.b Initialize arrays (:539-545), memsets in the object.
    for (int i = 0; i < ny * (nx + 1); ++i) vcx2d[i] = r0;
    for (int i = 0; i < (ny + 1) * nx; ++i) vcy2d[i] = r0;
    for (int i = 0; i < nxy; ++i) cxtot2d[i] = r0;
    for (int i = 0; i < nxy; ++i) cytot2d[i] = r0;
    for (int i = 0; i < (ny + 1) * (nx + 1); ++i) fld2d[i] = r0;
    for (int i = 0; i < ny * (nx + 1); ++i) vflx2d[i] = r0;
    for (int i = 0; i < nxy; ++i) vfly2d[i] = r0;

    // 2. Field and velocities (:561-579).
    for (int isea = 1; isea <= nsea; ++isea) {
      const int ix = g.mapsf[isea - 1];
      const int iy = g.mapsf[nsea + isea - 1];
      const int ixy = g.mapsf[2 * nsea + isea - 1];
      const Real cgi = s.cg[isea * cgld + ik];
      const Real clat = g.clats[isea];
      fld2d[at(iy, ix, ld1)] = (field[ixy + foff] / cgi) * clat;  // :566, divide then multiply
      cxtot2d[at(iy, ix, ny)] = (ecosi * cgi) / clat;             // :568
      cytot2d[at(iy, ix, ny)] = esini * cgi;                      // :569
    }
    if (p.flcur != 0) {
      for (int isea = 1; isea <= nsea; ++isea) {
        const int ix = g.mapsf[isea - 1];
        const int iy = g.mapsf[nsea + isea - 1];
        cxtot2d[at(iy, ix, ny)] = cxtot2d[at(iy, ix, ny)] + (s.cx[isea] / g.clats[isea]);  // :590
        cytot2d[at(iy, ix, ny)] = cytot2d[at(iy, ix, ny)] + s.cy[isea];                    // :591
      }
    }
    if (p.flcx != 0) {
      for (int isea = 1; isea <= nsea; ++isea) {
        const int ix = g.mapsf[isea - 1];
        const int iy = g.mapsf[nsea + isea - 1];
        const Real cp = cxtot2d[at(iy, ix, ny)] * g.dpdx[at(iy, ix, ny)] +
                        cytot2d[at(iy, ix, ny)] * g.dpdy[at(iy, ix, ny)];  // :600
        vcx2d[at(iy, ix, ny)] = cp * dtrad;                                // :601
      }
    } else {
      for (int i = 0; i < ny * (nx + 1); ++i) vcx2d[i] = r0;  // :604
    }
    if (p.flcy != 0) {
      for (int isea = 1; isea <= nsea; ++isea) {
        const int ix = g.mapsf[isea - 1];
        const int iy = g.mapsf[nsea + isea - 1];
        const Real cq = cxtot2d[at(iy, ix, ny)] * g.dqdx[at(iy, ix, ny)] +
                        cytot2d[at(iy, ix, ny)] * g.dqdy[at(iy, ix, ny)];  // :611
        vcy2d[at(iy, ix, ld1)] = cq * dtrad;                               // :612
      }
    } else {
      for (int i = 0; i < (ny + 1) * nx; ++i) vcy2d[i] = r0;  // :615
    }

    // :622, to index space, every cell of 1:NY,1:NX, land included.
    for (int ix = 1; ix <= nx; ++ix)
      for (int iy = 1; iy <= ny; ++iy)
        fld2d[at(iy, ix, ld1)] = fld2d[at(iy, ix, ld1)] * g.gsqrt[at(iy, ix, ny)];

    // :627-638, longitude closure: column 1 copied into column NX+1.
    if (p.iclose != kIcloseNone) {
      for (int iy = 1; iy <= ny; ++iy) {
        fld2d[at(iy, nx + 1, ld1)] = fld2d[at(iy, 1, ld1)];
        vcx2d[at(iy, nx + 1, ny)] = vcx2d[at(iy, 1, ny)];
      }
    }
    // :642-648, tripole closure: row NY, mirrored in IX, copied into row NY+1.
    if (trpl) {
      for (int ix = 1; ix <= nx; ++ix) {
        fld2d[at(ny + 1, ix, ld1)] = fld2d[at(ny, nx - ix + 1, ld1)];
        vcy2d[at(ny + 1, ix, ld1)] = vcy2d[at(ny, nx - ix + 1, ld1)];
      }
    }

    // 3. Propagation fluxes (:653-727).
    const int nymax = trpl ? ny : ny - 1;  // :653-654
    for (int ix = 1; ix <= nx; ++ix) {
      for (int iy = 1; iy <= nymax; ++iy) {
        const int ixy = iy + (ix - 1) * ny;  // :662
        const Real vcb =
            f.facvx[ixy - 1] * (vcx2d[at(iy, ix, ny)] + vcx2d[at(iy, ix + 1, ny)]);  // :663
        // :664-665, maxss %0,%vcb: vcb > 0 ? vcb : 0; minss %0,%vcb: vcb < 0 ?
        // vcb : 0. So -0.0 and NaN give +0.0 in both.
        vflx2d[ix * ny + iy - 1] = max_r(vcb, r0) * fld2d[at(iy, ix, ld1)] +
                                   min_r(vcb, r0) * fld2d[at(iy, ix + 1, ld1)];
      }
    }
    // :677-687, longitude closure: column NX copied into column 0.
    if (p.iclose != kIcloseNone) {
      for (int iy = 1; iy <= ny; ++iy) vflx2d[0 * ny + iy - 1] = vflx2d[nx * ny + iy - 1];
    }
    for (int ix = 1; ix <= nx; ++ix) {
      for (int iy = 1; iy <= ny - 1; ++iy) {
        const int ixy = iy + (ix - 1) * ny;  // :695
        const Real vcb =
            f.facvy[ixy - 1] * (vcy2d[at(iy, ix, ld1)] + vcy2d[at(iy + 1, ix, ld1)]);  // :696
        vfly2d[at(iy, ix, ny)] = max_r(vcb, r0) * fld2d[at(iy, ix, ld1)] +
                                 min_r(vcb, r0) * fld2d[at(iy + 1, ix, ld1)];  // :697-698
      }
    }
    // :709-727, tripole: row NY with the difference across the seam.
    if (trpl) {
      const int iy = ny;
      for (int ix = 1; ix <= nx; ++ix) {
        const int ixy = iy + (ix - 1) * ny;
        const Real vcb =
            f.facvy[ixy - 1] * (vcy2d[at(iy, ix, ld1)] - vcy2d[at(iy + 1, ix, ld1)]);  // :718
        vfly2d[at(iy, ix, ny)] = max_r(vcb, r0) * fld2d[at(iy, ix, ld1)] +
                                 min_r(vcb, r0) * fld2d[at(iy + 1, ix, ld1)];  // :719-720
      }
    }

    // 4. Propagate (:739-794).
    for (int isea = 1; isea <= nsea; ++isea) {
      const int ix = g.mapsf[isea - 1];
      const int iy = g.mapsf[nsea + isea - 1];
      const int ixy = g.mapsf[2 * nsea + isea - 1];
      if (mapsta[ixy - 1] == 1) {  // :749
        // :751-770, the upwind side of each face; a NaN flux compares false.
        const Real fxn = vflx2d[(ix - 1) * ny + iy - 1];  // VFLX2D(IY,IX-1)
        const Real fxp = vflx2d[ix * ny + iy - 1];        // VFLX2D(IY,IX)
        const Real fyn = vfly2d[at(iy - 1, ix, ny)];      // VFLY2D(IY-1,IX), see the pad
        const Real fyp = vfly2d[at(iy, ix, ny)];          // VFLY2D(IY,IX)
        const int jxn = (fxn > r0) ? -1 : 0;
        const int jxp = (fxp < r0) ? 1 : 0;
        const int jyn = (fyn > r0) ? -1 : 0;
        const int jyp = (fyp < r0) ? 1 : 0;
        // :772-776, left to right: each product, then the running sum.
        // ATRNX(IXY,J) = atrnx[(J+1)*nxy + IXY-1].
        fld2d[at(iy, ix, ld1)] = fld2d[at(iy, ix, ld1)] + g.atrnx[(jxn + 1) * nxy + ixy - 1] * fxn -
                                 g.atrnx[(jxp + 1) * nxy + ixy - 1] * fxp +
                                 g.atrny[(jyn + 1) * nxy + ixy - 1] * fyn -
                                 g.atrny[(jyp + 1) * nxy + ixy - 1] * fyp;
      }
      // :792, CG/CLATS first, then times the field, for every sea point.
      fld2d[at(iy, ix, ld1)] = (s.cg[isea * cgld + ik] / g.clats[isea]) * fld2d[at(iy, ix, ld1)];
    }

    // :802, back to physical space, every cell of 1:NY,1:NX.
    for (int ix = 1; ix <= nx; ++ix)
      for (int iy = 1; iy <= ny; ++iy)
        fld2d[at(iy, ix, ld1)] = fld2d[at(iy, ix, ld1)] / g.gsqrt[at(iy, ix, ny)];

    // 5. Update boundary conditions (:806-827), the live #else branch of :820-825.
    if (s.flbpi != 0) {
      // :807-808, DTG*REAL(NTLOC-ITLOC) then /REAL(NTLOC) then subtracted.
      Real rd1 = s.dsec_bpi0 - ((dtg * static_cast<Real>(ntloc - itloc)) / rntloc);
      Real rd2 = s.dsec_bpin;  // :809
      if (rd2 > r1em3) {       // :810
        // :811, maxss %0,%q: q > 0 ? q : 0 (NaN -> 0); minss %1,%t: t < 1 ? t : 1.
        const Real q = rd1 / rd2;
        rd2 = min_r(max_r(q, r0), r1);
        rd1 = r1 - rd2;  // :812
      } else {
        rd1 = r0;  // :814
        rd2 = r1;  // :815
      }
      // :817-826. The value is stored as it is, in physical space: unlike
      // W3XYP3 there is no /CG*CLATS, since FLD2D is already back from it.
      for (int ibi = 1; ibi <= s.nbi; ++ibi) {
        const int ib = s.isbpi[ibi - 1];
        const int ix = g.mapsf[ib - 1];
        const int iy = g.mapsf[nsea + ib - 1];
        const Real b0 = s.bbpi0[ibi * p.nspec + isp - 1];
        const Real bn = s.bbpin[ibi * p.nspec + isp - 1];
        fld2d[at(iy, ix, ld1)] = (rd1 * b0) + (rd2 * bn);
      }
    }

    // 6. Put back in 1d shape (:831-836), every sea point.
    for (int isea = 1; isea <= nsea; ++isea) {
      const int ix = g.mapsf[isea - 1];
      const int iy = g.mapsf[nsea + isea - 1];
      const int ixy = g.mapsf[2 * nsea + isea - 1];
      field[ixy + foff] = fld2d[at(iy, ix, ld1)];
    }
  }
}

}  // namespace ww::prop
