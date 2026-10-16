// kokkos/src/ww_kokkos/srce_ref1.hpp
// W3SREF, ported to Kokkos: shoreline, subgrid-island and iceberg reflection
// (switch REF1), one spectral point. Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3ref1md.F90:111-670 at NOAA-EMC/WW3 03adda91
// (identical in the WW3/ fork); author F. Ardhuin. Called from W3SRCE
// (w3srcemd.F90:2309-2313) when REFLEC(1) > 0, REFLEC(2) > 0 or REFLEC(4) > 0 with
// BERG > 0. The reference build is switches/switch_lab_shrd with REF0 replaced by
// REF1: W3_IG1 is not compiled (the ICALC = 2 pass, :250-257, :297-300,
// :318-438, :655-666, is not ported), W3_PDLIB is not compiled, W3_S output is
// not ported and a kernel never prints. Both GTYPE branches are ported, since
// GTYPE is a run-time grid setting.
//
// Dropped because the reference build never reads them: CX1, CY1, EMEAN, FMEAN,
// DEPTH and WN (read only under W3_IG1), JSEA (W3_PDLIB only) and FMEAN2 (:308,
// :312: -O3 removes it, it feeds only the IG1 branch). R1 at :497 is overwritten
// at :498 before any read and is not translated. RAMP2 at :475 is never read
// (:615 or :626 overwrite it before :631); kept as the source has it.
//
// LPDLIB: W3INIT sets it .FALSE. and sets it .TRUE. only under W3_PDLIB
// (w3initmd.F90:652-655), so in the model LPDLIB is "W3_PDLIB compiled". The
// caller passes `si` = SI(IX) and `iobpd` = IOBPD(:,IX) without PDLIB (the
// reference), PDLIB_SI(JSEA) and IOBPD_LOC(:,JSEA) with it (:287, :486); that arm
// is the same arithmetic but was not compiled or compared ⚠.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3ref1md.F90 (WW3's CMake Release flags, the switch above; the object is
// byte-identical to the CMake one):
//   :266        FACX = 1/(DERA*RADIUS) folded to 0x3716feb5;
//   :274-281    (SX*CLATS(ISEA))/FACX, SY/FACX, HPFAC/FACX, HQFAC/FACX: true divisions;
//   :291-292    REAL((SQRT(SI)*5.d0)*(DERA*RADIUS)), SI is REAL(8): sqrt in double,
//               DERA*RADIUS folded to the REAL 111111.109375 and widened;
//   :301-310    EB summed in ITH order, (DDEN*EB)/CG; FMEAN2 gone;
//   :311-313    maxss operand order MAX(1E-7,FMEANA) as b>a?b:a with a = 1E-7,
//               MAX(FMEANA,SIG(1)) with a = SIG(1) (from the object);
//   :440        NINT is lroundf; :465, :536, :591-593 R**NRS are __powisf2 calls;
//   :450-451    SLOPE**5 as s2 = s*s, s5 = s2*(s*s2); MAX(FMEANA,0.001)**4 as
//               (m*m)*(m*m); 0.0001*GRAV**2 folded to 0x3c1d8b6f; the NaN branch
//               folds the numerator to 0x2331612c, which these operations give;
//   :452, :622  log10f; (L+4.5)*0.07 + M*1.5 (0.007 at :622);
//   :473, :614  (FMEANA*(0.75*TPI))/SIG(IK), 0.75*TPI folded; powf(x, REFPARS(10));
//   :632-634    expf(-((BERG*DELX)*1E-4)); 1-TRN and 1-EXP as true subtractions.
// No FMA, no reciprocal multiply, no vectorised loop (-fopt-info-vec is empty);
// every libm call is scalar: log10f, powf, expf, lroundf, __powisf2.
// The C++ object (openmp-release flags, -ffp-contract=off) has the same
// operations, constants and MIN/MAX semantics, all scalar, no FMA. It calls powf
// three times where gfortran (-fno-math-errno) shares the :614 call with :623, and
// sqrt twice at :291-292: same arguments, same bits. NINT is roundf and a
// conversion: lroundf's bits for |x| < 2**63 (st4_params.hpp, nint).
// The kernel has no reduction across points and no atomic; within a point the
// loops run in the Fortran's order, so any execution space gives the same bits.
#pragma once

#include <cstdint>

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"  // nint: NINT of a REAL as lroundf

namespace ww::srce {

namespace ref1_detail {
// Fortran MIN/MAX of two REALs: fmin2(a,b) = b<a?b:a, fmax2(a,b) = b>a?b:a. A NaN
// or a +0/-0 tie gives `a`; each call passes the operands in the order of the
// build's minss/maxss (w3ref1md.o: `maxss src,dst` is fmax2(src,dst)), which is
// not always the Fortran order.
KOKKOS_INLINE_FUNCTION Real fmin2(Real a, Real b) { return (b < a) ? b : a; }
KOKKOS_INLINE_FUNCTION Real fmax2(Real a, Real b) { return (b > a) ? b : a; }

// x**n for a run-time INTEGER n: libgcc's __powisf2 (libgcc.a of gfortran 16.2,
// disassembled: y = odd ? x : 1; while (n >>= 1) { x = x*x; if odd y = y*x; };
// 1/y for n < 0). Never std::pow.
KOKKOS_INLINE_FUNCTION Real powi(Real x, int m) {
  unsigned int n = (m < 0) ? 0u - static_cast<unsigned int>(m) : static_cast<unsigned int>(m);
  Real y = (n % 2u) != 0u ? x : static_cast<Real>(1);
  while ((n >>= 1u) != 0u) {
    x = x * x;
    if ((n % 2u) != 0u) y = y * x;
  }
  return (m < 0) ? static_cast<Real>(1) / y : y;
}

KOKKOS_INLINE_FUNCTION int iabs(int i) { return (i < 0) ? -i : i; }

// Folded REAL constants, written as the bits of .rodata.cst4 of w3ref1md.o.
inline constexpr Real kFacxLL = 0x1.2dfd6ap-17f;      // 1./(DERA*RADIUS) :266, 0x3716feb5
inline constexpr double kDeraRadius = 0x1.b2071cp+16;  // DERA*RADIUS :291, REAL widened
inline constexpr Real kTpiInv = 0x1.45f306p-3f;       // TPIINV, 0x3e22f983
inline constexpr Real kMicheC = 0x1.3b16dep-7f;       // 0.0001*GRAV**2 :450, 0x3c1d8b6f
inline constexpr Real kC075Tpi = 0x1.2d97c8p+2f;      // 0.75*TPI :473, 0x4096cbe4
// REAL literals.
inline constexpr Real k1em7 = 0x1.ad7f2ap-24f;   // 1.E-7, 0x33d6bf95
inline constexpr Real k1em3 = 0x1.0624dep-10f;   // 0.001, 0x3a83126f
inline constexpr Real k1em4 = 0x1.a36e2ep-14f;   // 1E-4 and 0.0001, 0x38d1b717
inline constexpr Real k0p07 = 0x1.1eb852p-4f;    // 0.07, 0x3d8f5c29
inline constexpr Real k0p007 = 0x1.cac084p-8f;   // 0.007, 0x3be56042
inline constexpr Real k0p005 = 0x1.47ae14p-8f;   // 0.005, 0x3ba3d70a
inline constexpr Real k1p57 = 0x1.91eb86p+0f;    // 1.57, 0x3fc8f5c3
inline constexpr Real k4p5 = 4.5f;
inline constexpr Real k1p5 = 1.5f;
inline constexpr Real kHalf = 0.5f;
}  // namespace ref1_detail

/// GTYPE values, w3gdatmd.F90:625-628.
enum Ref1Gtype : int { kRlgType = 1, kClgType = 2, kUngType = 3, kSmcType = 4 };

/// RPAR indices (0-based) of the W3GDATMD scalars W3SREF reads.
enum Ref1Rpar : int {
  REF1_R_SX = 0,    // W3GDATMD SX
  REF1_R_SY,        // W3GDATMD SY
  REF1_R_REFPARS,   // W3GDATMD REFPARS(1:10): REF1_R_REFPARS + 0..9
  REF1_NRPAR = REF1_R_REFPARS + 10
};

/// IPAR indices (0-based).
enum Ref1Ipar : int {
  REF1_I_NK = 0,   // W3GDATMD NK
  REF1_I_NTH,      // W3GDATMD NTH
  REF1_I_NSPEC,    // W3GDATMD NSPEC
  REF1_I_GTYPE,    // W3GDATMD GTYPE
  REF1_I_FLAGLL,   // W3GDATMD FLAGLL, 0/1
  REF1_NIPAR
};

/// The grid scalars above, unpacked, under WW3's names in lower case.
struct Ref1Params {
  int nk, nth, nspec, gtype, flagll;
  Real sx, sy;
  Real refpars[10];  ///< REFPARS(1:10): refpars[i-1] (allocated with 10, w3gdatmd.F90:1824)
};

inline Ref1Params unpack_ref1(const float* r, const int* i) {
  Ref1Params p{};
  p.nk = i[REF1_I_NK];
  p.nth = i[REF1_I_NTH];
  p.nspec = i[REF1_I_NSPEC];
  p.gtype = i[REF1_I_GTYPE];
  p.flagll = i[REF1_I_FLAGLL];
  p.sx = r[REF1_R_SX];
  p.sy = r[REF1_R_SY];
  for (int j = 0; j < 10; ++j) p.refpars[j] = r[REF1_R_REFPARS + j];
  return p;
}

/// W3SREF(A, CG, WN, EMEAN, FMEAN, DEPTH, CX1, CY1, REFLC, REFLD, TRNX, TRNY,
/// BERG, DT, IX, IY, JSEA, S) for one point.
///
/// Grid arrays, 0-based pointers:
///   sig   = SIG(0:NK+1), sig[k] = SIG(k): :614 and :623 read SIG(IK) after the
///           IK loops, where IK = NK+1 (1 when NK < 1), as the -O3 build does;
///   dden  = DDEN(1:NK); ecos, esin = ECOS(1:NTH), ESIN(1:NTH);
///   iobpd = IOBPD(1:NTH,IX) (INTEGER*1), read only when GTYPE = UNGTYPE,
///           REFPARS(3) < 0.5 and REFLC(1) > 0; may be null otherwise.
/// Point values the caller looks up, each read only for its GTYPE (any value
/// otherwise): clats = CLATS(MAPFS(IY,IX)) (RLGTYPE, SMCTYPE), hpfac, hqfac =
/// HPFAC(IY,IX), HQFAC(IY,IX) (CLGTYPE), si = SI(IX), REAL(8) (UNGTYPE).
/// Point arrays: a = A(1:NSPEC), INOUT (the UNGTYPE branch zeroes the energy not
/// going to the coast, :493); cg = CG(1:NK); reflc = REFLC(1:4); refld =
/// REFLD(1:6); s = S(1:NSPEC), OUT.
KOKKOS_INLINE_FUNCTION void w3sref_point(const Ref1Params& p, const Real* sig, const Real* dden,
                                         const Real* ecos, const Real* esin,
                                         const std::int8_t* iobpd, Real clats, Real hpfac,
                                         Real hqfac, double si, Real* a, const Real* cg,
                                         const Real* reflc, const int* refld, Real trnx,
                                         Real trny, Real berg, Real dt, Real* s) {
  using namespace ref1_detail;
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  const Real one = static_cast<Real>(1.);
  const Real zero = static_cast<Real>(0.);

  // :258-269.
  Real emeana = zero;
  Real fmeana = zero;
  Real delx = one;
  Real dely = one;
  const Real facx = (p.flagll != 0) ? kFacxLL : one;

  // :271-294. ISEA = MAPFS(IY,IX) is resolved by the caller into clats.
  if (p.gtype == kRlgType || p.gtype == kSmcType) {
    delx = p.sx * clats / facx;
    dely = p.sy / facx;
  }
  if (p.gtype == kClgType) {
    delx = hpfac / facx;
    dely = hqfac / facx;
  }
  if (p.gtype == kUngType) {
    delx = static_cast<Real>(5. * Kokkos::sqrt(si) * kDeraRadius);
    dely = static_cast<Real>(5. * Kokkos::sqrt(si) * kDeraRadius);
  }

  // :296-313. IK1 = 1 without W3_IG1. ik keeps the Fortran DO variable's value
  // after the loop: NK+1, or 1 when NK < 1.
  int ik;
  for (ik = 1; ik <= nk; ++ik) {
    Real eb = zero;
    for (int ith = 1; ith <= nth; ++ith) {
      eb = eb + a[ith + (ik - 1) * nth - 1];
    }
    eb = eb * dden[ik - 1] / cg[ik - 1];
    emeana = emeana + eb;
    fmeana = fmeana + eb / sig[ik];
  }
  fmeana = kTpiInv * (emeana / fmax2(k1em7, fmeana));
  fmeana = fmax2(sig[1], fmeana);

  // :317, :335.
  for (int is = 0; is < nspec; ++is) s[is] = zero;

  // :440-456.
  const int nrs = ww::st4::nint(p.refpars[7]);
  Real ramp0;
  Real slope;
  Real michefac;
  if (p.refpars[5] > zero) {
    if (reflc[2] != reflc[2]) {  // isnan test, kept by -O3
      slope = k1em3;
    } else {
      slope = fmax2(k1em3, reflc[2]);
    }
    const Real s2 = slope * slope;
    const Real s5 = s2 * (slope * s2);  // SLOPE**5, GCC's power tree
    const Real m = fmax2(fmeana, k1em3);
    const Real m2 = m * m;
    michefac = kMicheC * s5 / (fmax2(k1em4, emeana) * (m2 * m2));
    ramp0 = fmax2(k0p005, k0p07 * (Kokkos::log10(michefac) + k4p5) + k1p5 * michefac);
  } else {
    ramp0 = one;
  }

  Real ramp = zero;
  Real ramp1 = zero;
  Real ramp2 = zero;
  Real ramp4 = zero;
  Real fac1 = zero;

  // :461-603. Shoreline reflection.
  if (reflc[0] > zero) {
    fac1 = one / (kHalf * static_cast<Real>(nth));
    const Real fac2 = k1p57 / (kHalf * static_cast<Real>(nth));
    Real sum = zero;  // SUM(ABS(ECOS(1:NTH))**NRS), in ITH order
    for (int ith = 1; ith <= nth; ++ith) sum = sum + powi(Kokkos::fabs(ecos[ith - 1]), nrs);
    const Real fac3 = static_cast<Real>(2.) / sum;

    for (ik = 1; ik <= nk; ++ik) {
      // :472-479.
      if (p.refpars[5] > zero) {
        ramp = Kokkos::pow(fmax2(one, kC075Tpi * fmeana / sig[ik]), p.refpars[9]) * ramp0;
        ramp1 = fmin2(p.refpars[8], reflc[0] * ramp);
        ramp2 = fmin2(p.refpars[8], reflc[1] * ramp);
      } else {
        ramp1 = ramp0 * reflc[0];
        ramp2 = ramp0 * reflc[1];
      }

      if (p.gtype == kUngType && p.refpars[2] < kHalf) {
        // :483-542, unstructured grids. IOBPDIP = IOBPD(:,IX) (:489).
        for (int ith = 1; ith <= nth; ++ith) {
          const int ispeci = ith + (ik - 1) * nth;
          a[ispeci - 1] = a[ispeci - 1] * static_cast<Real>(static_cast<int>(iobpd[ith - 1]));
        }
        for (int ith = 1; ith <= nth; ++ith) {
          const Real r1 = static_cast<Real>(static_cast<int>(iobpd[ith - 1]));  // :498
          const int ispeci = ith + (ik - 1) * nth;
          const Real r2 = ramp1 * a[ispeci - 1];
          if (r1 > zero && r2 > zero) {
            const int ith3 = 1 + (nth / 2 + nth + 2 * refld[0] - ith - 1) % nth;
            for (int ith2 = 1; ith2 <= nth; ++ith2) {
              const int ispec = ith2 + (ik - 1) * nth;
              const Real r3 = ecos[(1 + iabs(ith2 - refld[0]) % nth) - 1];
              if (r3 < zero) {
                const Real r4 = ecos[(1 + iabs(ith2 - ith3) % nth) - 1] *
                                static_cast<Real>(1 - static_cast<int>(iobpd[ith2 - 1]));
                if (r4 > zero) {
                  switch (refld[1]) {
                    case 0:
                      s[ispec - 1] = s[ispec - 1] + r2 * fac1 / dt;
                      break;
                    case 1:
                      s[ispec - 1] = s[ispec - 1] + r2 * Kokkos::fabs(r4) * fac2 / dt;
                      break;
                    case 2:
                      s[ispec - 1] = s[ispec - 1] + r2 * powi(r4, nrs) * fac3 / dt;
                      break;
                    default:
                      break;
                  }
                }
              }
            }
          }
        }
      } else {
        // :544-599, structured grids (and UNGTYPE with REFPARS(3) >= 0.5).
        for (int ith = 1; ith <= nth; ++ith) {
          const Real r1 = ecos[(1 + iabs(ith - refld[0]) % nth) - 1];
          const Real r2 = ramp1 * a[ith + (ik - 1) * nth - 1];
          if (r1 > zero && r2 > zero) {
            for (int ith2 = 1; ith2 <= nth; ++ith2) {
              const int ispec = ith2 + (ik - 1) * nth;
              const int ith2x = 1 + (nth + ith2 - refld[4] - 1) % nth;
              const int ith2y = 1 + (nth + ith2 - refld[5] - 1) % nth;
              const Real r3 = ecos[(1 + iabs(ith2 - refld[0]) % nth) - 1];
              if (r3 < zero) {
                const int ith3 = 1 + (nth / 2 + nth + 2 * refld[0] - ith - 1) % nth;
                const Real r4 = ecos[(1 + iabs(ith2 - ith3) % nth) - 1];
                if (r4 > zero) {
                  const Real cgk = cg[ik - 1];
                  const Real rx = static_cast<Real>(refld[2]);
                  const Real ry = static_cast<Real>(refld[3]);
                  switch (refld[1]) {
                    case 0:
                      s[ispec - 1] = s[ispec - 1] +
                                     rx * r2 * cgk * Kokkos::fabs(ecos[ith2x - 1] / delx) * fac1 +
                                     ry * r2 * cgk * Kokkos::fabs(esin[ith2y - 1] / dely) * fac1;
                      break;
                    case 1:
                      s[ispec - 1] = s[ispec - 1] +
                                     rx * r2 * cgk * Kokkos::fabs(ecos[ith2x - 1] / delx) *
                                         Kokkos::fabs(r4) * fac2 +
                                     ry * r2 * cgk * Kokkos::fabs(esin[ith2y - 1] / dely) *
                                         Kokkos::fabs(r4) * fac2;
                      break;
                    case 2: {
                      const Real r4n = powi(r4, nrs);  // one __powisf2, used twice
                      s[ispec - 1] = s[ispec - 1] +
                                     rx * r2 * cgk * Kokkos::fabs(ecos[ith2x - 1] / delx) * r4n *
                                         fac3 +
                                     ry * r2 * cgk * Kokkos::fabs(esin[ith2y - 1] / dely) * r4n *
                                         fac3;
                      break;
                    }
                    default:
                      break;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  // :608-653. Diffuse reflection by subgrid islands and icebergs. SIG(IK) at
  // :614 and :623 is read with IK left by the last DO loop (see above).
  if (((p.refpars[1] > zero) && ((trnx + trny) < static_cast<Real>(2.))) ||
      ((p.refpars[3] > zero) && (berg > zero))) {
    if (p.refpars[5] > zero) {
      ramp = Kokkos::pow(fmax2(one, kC075Tpi * fmeana / sig[ik]), p.refpars[9]) * ramp0;
      ramp2 = fmin2(p.refpars[8], reflc[1] * ramp);
      // :619-624: iceberg slope REFLC(4).
      slope = fmax2(k1em3, reflc[3]);
      const Real s2 = slope * slope;
      const Real s5 = s2 * (slope * s2);
      const Real m = fmax2(fmeana, k1em3);
      const Real m2 = m * m;
      michefac = kMicheC * s5 / (fmax2(k1em4, emeana) * (m2 * m2));
      ramp0 = fmax2(k0p005, k0p007 * (Kokkos::log10(michefac) + k4p5) + k1p5 * michefac);
      ramp = Kokkos::pow(fmax2(one, kC075Tpi * fmeana / sig[ik]), p.refpars[9]) * ramp0;
      ramp4 = fmin2(p.refpars[8], ramp);
    } else {
      ramp2 = ramp0 * reflc[1];
      ramp4 = ramp0 * reflc[3];
    }

    // :631-635.
    const Real r2x =
        ramp2 * p.refpars[1] * fmax2(zero, fmin2(one, (one - trnx))) +
        ramp4 * p.refpars[3] * fmax2(zero, fmin2(one, (one - Kokkos::exp(-(berg * delx * k1em4)))));
    const Real r2y =
        ramp2 * p.refpars[1] * fmax2(zero, fmin2(one, (one - trny))) +
        ramp4 * p.refpars[3] * fmax2(zero, fmin2(one, (one - Kokkos::exp(-(berg * dely * k1em4)))));
    fac1 = one / (kHalf * static_cast<Real>(nth));
    for (int k = 1; k <= nk; ++k) {
      for (int ith = 1; ith <= nth; ++ith) {
        const Real r2 = a[ith + (k - 1) * nth - 1];
        if (r2 > zero) {
          for (int ith2 = 1; ith2 <= nth; ++ith2) {
            const int ispec = ith2 + (k - 1) * nth;
            const Real r3 = ecos[(1 + (nth + ith2 - ith) % nth) - 1];
            if (r3 < zero) {
              s[ispec - 1] = s[ispec - 1] +
                             cg[k - 1] * r2x * r2 * Kokkos::fabs(ecos[ith2 - 1] / delx) * fac1 +
                             cg[k - 1] * r2y * r2 * Kokkos::fabs(esin[ith2 - 1] / dely) * fac1;
            }
          }
        }
      }
    }
  }
}

}  // namespace ww::srce
