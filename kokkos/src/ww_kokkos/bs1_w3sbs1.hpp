// kokkos/src/ww_kokkos/bs1_w3sbs1.hpp
// W3SBS1: bottom scattering source term after Ardhuin and Herbers (2000) without a current
// and Magne and Ardhuin (2007) with one, one spectral point (S, D, TAUSCX, TAUSCY).
// Phase 1, translated, not wired, not regtested.
// WW3 heritage: WW3/model/src/w3sbs1md.F90:112-462 at h0ffmann/WW3 96892c95 (the same -O3
// object was compiled from it and checked against the CMake build's, see the port report);
// author F. Ardhuin. Evidence: kokkos/port_report/W3SBS1_KOKKOS.md.
// SPDX-FileCopyrightText: 2009 National Weather Service (NWS), National Oceanic and Atmospheric Administration
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Host side, not translated (module state of W3SBS1MD and the SAVE variables of W3SBS1):
//  - FIRST (:210, :247-250): CALL INSBS1(1) reads bottomspectrum.inp and allocates BOTSPEC,
//    sets NKBX, NKBY, DKBX, DKBY;
//  - the first call with no current (:251-257): w3sbs1_krange() below does the arithmetic of
//    KWMIN and KWMAX (:252-253), the caller prints the "k range" line (:254), runs INSBS1(2)
//    (it fills SCATMATA, SCATMATD, SCATMATV with DIAGONALIZE) and sets MATRICES = 1 (:256);
//  - CURTX = CX1, CURTY = CY1 (:229-230), module variables nothing in this file reads;
//  - the diagnostic WRITE of :298 ("Energy not conserved"): w3sbs1_point reports each IK that
//    would print through Sbs1Diag, and the caller prints.
// Not translated because the -O3 object does not compute them (dead stores): the SB, WNBOT,
// SMATRIX, SMATRIX2 locals, the accumulations INTEGRAL1, INTEGRAL1B, INTEGRAL2, INTEGRAL3, CGPK,
// the SUM of SMATRIX and everything under W3_T (:437-462).
//
// The no-current branch (:272-303) calls libgfortran's MATMUL for the product with the
// TRANSPOSE (the object has a call to _gfortran_matmul_r8, not inline code). The sum order of
// that routine is not the source's and depends on NTH, on the libgfortran build and on the CPU
// (it dispatches AVX2 and AVX-512 variants at run time). A hand model with two accumulators
// matched only even NTH from 4 and failed NTH = 2 and every odd NTH tried (v, 3000 random cases
// each, this build, AVX-512 CPU); it was removed. matmul_tn() now calls _gfortran_matmul_r8
// itself with the descriptors of the object, which is the Fortran's own call, so the bits are
// the executable's for every NTH (v: NTH 1..130, 5200 cases, 0 differences, r-u6 harness, one
// libgfortran 16.2.0 and one AVX-512 CPU; no repo command ⚠). This ties the header to
// libgfortran's gfc_array_r8 layout (GCC 8 or later) and to linking libgfortran, which any
// WW3 executable does; the symbol is weak and a missing one aborts. It is host only.
// ⚠ On a device sinhf, tanhf, sqrtf and fmodf are CUDA's, not glibc's.
#pragma once

#include <cmath>

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"

namespace ww::bs1 {

/// NKSCAT of w3sbs1md.F90:62.
inline constexpr int kNkscat = 30;

namespace w3sbs1_detail {
// minss/maxss of w3sbs1md.o, d the destination register: a NaN or a tie gives s.
KOKKOS_INLINE_FUNCTION Real minss(Real d, Real s) { return (d < s) ? d : s; }
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }
KOKKOS_INLINE_FUNCTION int imin(int a, int b) { return (a < b) ? a : b; }
KOKKOS_INLINE_FUNCTION int imax(int a, int b) { return (a > b) ? a : b; }

KOKKOS_INLINE_FUNCTION Real sinh_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::sinhf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::sinh(x);))
#else
  return Kokkos::sinh(x);
#endif
}
KOKKOS_INLINE_FUNCTION Real tanh_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::tanhf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::tanh(x);))
#else
  return Kokkos::tanh(x);
#endif
}
/// MOD(x, 1.0): the object calls fmodf.
KOKKOS_INLINE_FUNCTION Real fmod1_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::fmodf(x, 1.0f);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::fmod(x, 1.0f);))
#else
  return Kokkos::fmod(x, 1.0f);
#endif
}

inline constexpr Real kGrav = static_cast<Real>(9.806);                 // constants.F90:62
}  // namespace w3sbs1_detail

/// The variables of W3SBS1MD and the SAVE state of W3SBS1 that the arithmetic reads
/// (w3sbs1md.F90:60-71, :210-211). BOTSPEC(NKBX,NKBY), SCATMATV(0:NKSCAT-1,NTH,NTH) and
/// SCATMATD(0:NKSCAT-1,NTH) are column-major, indices as in the Fortran; the pointers address
/// the first element (BOTSPEC(1,1), SCATMATV(0,1,1), SCATMATD(0,1)).
struct Sbs1Module {
  int nkbx = 0, nkby = 0;
  Real dkbx = 0, dkby = 0, kwmin = 0, kwmax = 0;
  const Real* botspec = nullptr;
  const double* scatmatv = nullptr;
  const double* scatmatd = nullptr;
  int matrices = 0;  ///< MATRICES: 1 once INSBS1(2) ran
};

/// The grid arrays of W3GDATMD that W3SBS1 reads, 0-based (the Fortran index minus the lower
/// bound): sig = SIG(1:NK), sig2 = SIG2(1:NSPEC), ecos = ECOS(1:NTH), esin = ESIN(1:NTH),
/// ec2 = EC2(1:NTH), mapth = MAPTH(1:NSPEC), mapwn = MAPWN(1:NSPEC), values are Fortran 1-based.
struct Sbs1Grid {
  int nk = 0, nth = 0, nspec = 0;
  Real dth = 0;
  const Real* sig = nullptr;
  const Real* sig2 = nullptr;
  const Real* ecos = nullptr;
  const Real* esin = nullptr;
  const Real* ec2 = nullptr;
  const int* mapth = nullptr;
  const int* mapwn = nullptr;
};

/// Work memory: the Fortran's automatic and allocated locals.
///   dwork  4*NTH doubles (AVECT and the three temporaries of the matrix products)
///   wn2, kb, b  NSPEC*NTH reals each, (ISPEC,ITH2) column-major: [ispec-1 + nspec*(ith2-1)]
///   ka     NSPEC reals
struct Sbs1Work {
  double* dwork = nullptr;
  Real* wn2 = nullptr;
  Real* kb = nullptr;
  Real* b = nullptr;
  Real* ka = nullptr;
};

/// One IK where the Fortran would write "Energy not conserved:" (:298-299).
struct Sbs1Diag {
  int ik = 0;
  Real checksum = 0, etot = 0;
};

/// KWMIN and KWMAX, w3sbs1md.F90:252-253, from SIG(1) and the bottom spectrum grid.
KOKKOS_INLINE_FUNCTION void w3sbs1_krange(Real sig1, int nkbx, int nkby, Real dkbx, Real dkby,
                                          Real& kwmin, Real& kwmax) {
  using w3sbs1_detail::maxss;
  using w3sbs1_detail::minss;
  const Real dmax = maxss(dkby, dkbx);                                       // MAX(dkbx,dkby)
  kwmin = maxss(sig1 * sig1 / w3sbs1_detail::kGrav, dmax);                   // :252
  kwmax = minss(static_cast<Real>(nkbx) * dkbx, dkby * static_cast<Real>(nkby)) *
          static_cast<Real>(0.25);                                           // :253
}


/// y = MATMUL(TRANSPOSE(SCATMATV(IBK,:,:)), AVECT) (:289-291) by the library routine the object
/// calls: the same _gfortran_matmul_r8 with the descriptors of w3sbs1md.F90.273t.optimized
/// (a with the strides of the 3rd and 2nd dimension of SCATMATV swapped, try_blas 0, no gemm
/// pointer), so the bits are the executable's own whatever NTH, libgfortran build or CPU
/// dispatch. v is SCATMATV(0,1,1); the result goes to y (preset, the library allocates nothing).
/// Host only: a device run aborts here.
namespace gfc_detail {
struct Dim { long stride, lbound, ubound; };
struct Array {  // gfc_array_r8 of libgfortran.h, rank <= 2
  void* base_addr;
  long offset;
  unsigned long elem_len;
  int version;
  signed char rank, type;
  short attribute;
  long span;
  Dim dim[2];
};
}  // namespace gfc_detail

extern "C" void _gfortran_matmul_r8(gfc_detail::Array*, gfc_detail::Array*, gfc_detail::Array*, int,
                                    int, void*) __attribute__((weak));

KOKKOS_INLINE_FUNCTION void matmul_tn(const double* v, int ibk, int nth, const double* bvec,
                                      double* y) {
  KOKKOS_IF_ON_HOST((
    using gfc_detail::Array;
    if (&_gfortran_matmul_r8 == nullptr) Kokkos::abort("w3sbs1: libgfortran matmul_r8 not linked");
    const long n = nth;
    Array a{const_cast<double*>(v) + ibk, -(kNkscat * n) - kNkscat, 8, 0, 2, 3, 0, 8,
            {{kNkscat * n, 1, n}, {kNkscat, 1, n}}};
    Array b{const_cast<double*>(bvec), -1, 8, 0, 1, 3, 0, 8, {{1, 1, n}, {0, 0, 0}}};
    Array r{y, -1, 8, 0, 1, 3, 0, 8, {{1, 1, n}, {0, 0, 0}}};
    _gfortran_matmul_r8(&r, &a, &b, 0, 0, nullptr);))
  KOKKOS_IF_ON_DEVICE((
    (void)v; (void)ibk; (void)nth; (void)bvec; (void)y;
    Kokkos::abort("w3sbs1: no-current branch is host only");))
}

/// W3SBS1(A, CG, WN, DEPTH, CX1, CY1, TAUSCX, TAUSCY, S, D) for one point, arrays 0-based:
/// a = A(NTH,NK) column-major, cg = CG(1:NK), wn = WN(1:NK), s, d = S(NSPEC), D(NSPEC).
/// `mod` holds the state after the host steps listed at the top. `diag` has room for NK
/// entries; *ndiag receives the count.
KOKKOS_INLINE_FUNCTION void w3sbs1_point(const Sbs1Grid& g, const Sbs1Module& mod, const Real* a,
                                         const Real* cg, const Real* wn, Real depth, Real cx1,
                                         Real cy1, Real& tauscx, Real& tauscy, Real* s, Real* d,
                                         const Sbs1Work& w, Sbs1Diag* diag, int* ndiag) {
  using namespace w3sbs1_detail;
  const int nk = g.nk;
  const int nth = g.nth;
  const int nspec = g.nspec;
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.);
  const Real two = static_cast<Real>(2.);
  *ndiag = 0;

  for (int is = 0; is < nspec; ++is) {  // :261-262
    d[is] = zero;
    s[is] = zero;
  }
  tauscx = zero;  // :263
  tauscy = zero;  // :264

  const Real cur = ::fabsf(cx1) + ::fabsf(cy1);  // :251, :272

  if (depth * wn[0] <= static_cast<Real>(6)) {  // :268
    if (cur == zero && mod.matrices == 1) {     // :272
      // 3.a, no current
      double* avect = w.dwork;
      double* y1 = w.dwork + nth;
      double* tt = w.dwork + 2 * nth;
      double* res = w.dwork + 3 * nth;
      for (int ik = 1; ik <= nk; ++ik) {  // :273
        const Real kd = wn[ik - 1] * depth;  // :274
        if (kd <= static_cast<Real>(6) && wn[ik - 1] < mod.kwmax) {  // :275
          const Real wk = wn[ik - 1];
          const Real wk2 = wk * wk;
          const Real wk4 = wk2 * wk2;
          const Real kd2 = kd * two;
          const Real sh = sinh_(kd2);
          // :278-279, (WN**4)*SIG*PI*4./(SINH(2KD)*(2KD+SINH(2KD)))
          const Real kfactor = wk4 * g.sig[ik - 1] * ww::kPi * static_cast<Real>(4.) / ((kd2 + sh) * sh);
          const Real kscaled =
              static_cast<Real>(kNkscat - 2) * (wk - mod.kwmin) / (mod.kwmax - mod.kwmin);  // :280
          for (int ith = 0; ith < nth; ++ith) avect[ith] = static_cast<double>(a[ith + nth * (ik - 1)]);  // :281
          int ibk;
          Real kmod;
          if (kscaled < zero) {  // :282
            ibk = 0;
            kmod = zero;
          } else {
            ibk = ww::st4::fint(kscaled);  // :286
            kmod = fmod1_(kscaled);       // :287
          }
          const double kf = static_cast<double>(kfactor);
          Real* sseg = s + (ik - 1) * nth;
          // :289-291 then :292-295: both terms are the same product for IBK and IBK+1
          for (int pass = 0; pass < 2; ++pass) {
            const int ib = ibk + pass;
            matmul_tn(mod.scatmatv, ib, nth, avect, y1);
            for (int j = 0; j < nth; ++j) {
              tt[j] = y1[j] * (kf * mod.scatmatd[ib + kNkscat * j]);  // Kfactor*SCATMATD(IB,:)*y1
            }
            // MATMUL(SCATMATV(IB,:,:), tt): inline in the object, j outer, i inner, no FMA
            for (int i = 0; i < nth; ++i) res[i] = 0.0;
            for (int j = 0; j < nth; ++j) {
              for (int i = 0; i < nth; ++i) {
                res[i] = res[i] + mod.scatmatv[ib + kNkscat * (i + nth * j)] * tt[j];
              }
            }
            if (pass == 0) {
              const double f = static_cast<double>(one - kmod);  // (1.-kmod), PAREN_EXPR
              for (int i = 0; i < nth; ++i) sseg[i] = static_cast<Real>(f * res[i]);
            } else {
              const double f = static_cast<double>(kmod);
              for (int i = 0; i < nth; ++i) sseg[i] = sseg[i] + static_cast<Real>(res[i] * f);
            }
          }
          // :296-298
          Real sum = zero;
          for (int i = 0; i < nth; ++i) sum = sum + sseg[i];
          const Real checksum = ::fabsf(sum);
          Real etot = zero;
          for (int ith = 0; ith < nth; ++ith) etot = etot + a[ith + nth * (ik - 1)];
          if (checksum > static_cast<Real>(0.01) * etot) {
            diag[*ndiag].ik = ik;
            diag[*ndiag].checksum = checksum;
            diag[*ndiag].etot = etot;
            *ndiag = *ndiag + 1;
          }
        } else {
          Real* sseg = s + (ik - 1) * nth;  // :301
          for (int i = 0; i < nth; ++i) sseg[i] = zero;
        }
      }
    } else {
      // 3.b, with a current: k' (WN2) from k and U
      Real* wn2 = w.wn2;
      for (int ith2 = 1; ith2 <= nth; ++ith2) {  // :310
        for (int ispec = 1; ispec <= nspec; ++ispec) {  // :312
          const int mt = g.mapth[ispec - 1];
          const int mw = g.mapwn[ispec - 1];
          Real ku = cx1 * g.ecos[mt - 1] + cy1 * g.esin[mt - 1];     // :314
          Real kpu = cx1 * g.ecos[ith2 - 1] + cy1 * g.esin[ith2 - 1];  // :315
          const Real cgk = cg[mw - 1];                                // :316
          if ((cgk + kpu) < static_cast<Real>(0.1) * cgk) kpu = -(static_cast<Real>(0.9) * cgk);  // :317
          if ((cgk + ku) < static_cast<Real>(0.1) * cgk) ku = -(static_cast<Real>(0.9) * cg[mw - 1]);  // :318
          wn2[(ispec - 1) + nspec * (ith2 - 1)] = wn[mw - 1] * (cgk + ku) / (cgk + kpu);  // :319
        }
      }
      // 3.c coupling coefficient Ka(k) * Kb(k,k',theta')
      for (int ispec = 1; ispec <= nspec; ++ispec) {  // :333
        const int mt = g.mapth[ispec - 1];
        const int mw = g.mapwn[ispec - 1];
        w.ka[ispec - 1] = static_cast<Real>(4.) * ww::kPi * g.sig2[ispec - 1] * wn[mw - 1] /
                          sinh_(minss(two * wn[mw - 1] * depth, static_cast<Real>(20.)));  // :335-336
        for (int ith2 = 1; ith2 <= nth; ++ith2) {  // :338
          const Real kpu = cx1 * g.ecos[ith2 - 1] + cy1 * g.esin[ith2 - 1];  // :340
          const Real w2 = wn2[(ispec - 1) + nspec * (ith2 - 1)];
          const Real sigp = Kokkos::sqrt(kGrav * w2 * tanh_(w2 * depth));  // :341
          const int ie = 1 + (mt > ith2 ? mt - ith2 : ith2 - mt);          // EC2(1+ABS(MAPTH-ITH2))
          // :345-351. The second MIN has the constant first in the object (minss(20, x)).
          w.kb[(ispec - 1) + nspec * (ith2 - 1)] =
              w2 * w2 * w2 * g.ec2[ie - 1] /
              (two * w2 * depth +
               sinh_(minss(static_cast<Real>(20.), two * w2 * depth)) * (one + w2 * kpu * two / sigp));
        }
      }
      // 3.a bilinear interpolation of the bottom spectrum along the locus
      for (int i = 0; i < nspec * nth; ++i) w.b[i] = zero;  // :374
      for (int ispec = 1; ispec <= nspec; ++ispec) {        // :375
        const int mt = g.mapth[ispec - 1];
        const int mw = g.mapwn[ispec - 1];
        const Real kcutoff = zero * wn[mw - 1];  // :376, SCATTCUTOFF = 0.
        for (int ith2 = 1; ith2 <= nth; ++ith2) {  // :377
          const Real w2 = wn2[(ispec - 1) + nspec * (ith2 - 1)];
          const Real kbotx = wn[mw - 1] * g.ecos[mt - 1] - w2 * g.ecos[ith2 - 1];  // :378
          const Real kboty = wn[mw - 1] * g.esin[mt - 1] - w2 * g.esin[ith2 - 1];  // :380
          if ((kbotx * kbotx + kboty * kboty) > (kcutoff * kcutoff)) {            // :386
            const Real kbotxi = static_cast<Real>(mod.nkbx - (mod.nkbx % 2)) / static_cast<Real>(2.) +
                                static_cast<Real>(1.) + kbotx / mod.dkbx;  // :388
            const Real kbotyi = static_cast<Real>(mod.nkby - (mod.nkby % 2)) / static_cast<Real>(2.) +
                                static_cast<Real>(1.) + kboty / mod.dkby;  // :389
            const int ibk = imax(imin(ww::st4::fint(kbotxi), mod.nkbx - 1), 1);  // :391
            const Real xbk = fmod1_(kbotxi);                                     // :392
            const int jbk = imax(imin(ww::st4::fint(kbotyi), mod.nkby - 1), 1);  // :393
            const Real ybk = fmod1_(kbotyi);                                     // :394
            const Real* bs = mod.botspec;
            const int nx = mod.nkbx;
            // :396-401, BOTSPEC(i,j) = bs[(i-1) + nx*(j-1)]
            w.b[(ispec - 1) + nspec * (ith2 - 1)] =
                ((bs[(ibk - 1) + nx * (jbk - 1)] * (one - ybk) + bs[(ibk - 1) + nx * jbk] * ybk) * (one - xbk) +
                 (bs[ibk + nx * (jbk - 1)] * (one - ybk) + bs[ibk + nx * jbk] * ybk) * xbk);
          }
        }
      }
      // 4. Sbscat: linear interpolation of A(k', theta'), then the integral over theta'
      for (int ispec = 1; ispec <= nspec; ++ispec) {  // :415
        const int mt = g.mapth[ispec - 1];
        const int mw = g.mapwn[ispec - 1];
        Real integral = zero;  // :416
        for (int ith2 = 1; ith2 <= nth; ++ith2) {  // :417
          const Real w2 = wn2[(ispec - 1) + nspec * (ith2 - 1)];
          int iajust = 1;  // :418
          for (int i = 2; i <= nk; ++i) {
            if (w2 >= wn[i - 1]) iajust = i;  // :420
          }
          iajust = imax(iajust, 1);                // :422
          const int iajust2 = imin(iajust + 1, nk);  // :423
          Real ap;
          if (iajust == iajust2) {
            ap = a[(ith2 - 1) + nth * (iajust - 1)];  // :425
          } else {
            const Real bb = (w2 - wn[iajust - 1]) / (wn[iajust2 - 1] - wn[iajust - 1]);   // :427
            const Real aa = (wn[iajust2 - 1] - w2) / (wn[iajust2 - 1] - wn[iajust - 1]);  // :428
            ap = (a[(ith2 - 1) + nth * (iajust - 1)] * aa + a[(ith2 - 1) + nth * (iajust2 - 1)] * bb);  // :429
          }
          // :432-434, the factor WN/WN2 accounts for the Jacobians of N(K) and N(K')
          integral = integral + w.ka[ispec - 1] * w.kb[(ispec - 1) + nspec * (ith2 - 1)] *
                                    w.b[(ispec - 1) + nspec * (ith2 - 1)] *
                                    ((ap * wn[mw - 1] / w2) - a[(mt - 1) + nth * (mw - 1)]) * g.dth;
        }
        s[ispec - 1] = s[ispec - 1] + integral;  // :442
      }
    }
  }
}

}  // namespace ww::bs1
