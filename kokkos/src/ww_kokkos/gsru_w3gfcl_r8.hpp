// kokkos/src/ww_kokkos/gsru_w3gfcl_r8.hpp
// W3GFCL_R8: find the cell of a grid-search-utility object that encloses a target point (XT,YT),
// by the bucket that contains the point and a cross-product test of each cell of that bucket;
// with FNCL, when no cell encloses the point, the cell whose vertex centroid is nearest, searched
// over rings of neighbouring buckets. Host-side, no View is created and no kernel runs. Phase 1,
// translated, not regtested.
// WW3 heritage: FUNCTION W3GFCL_R8, WW3/model/src/w3gsrumd.F90:1369-1669 at h0ffmann/WW3
// 96892c95 (byte-identical to NOAA-EMC/WW3 03adda91: git -C WW3 diff 03adda91 HEAD --
// model/src/w3gsrumd.F90 is empty), and its callees W3CKCL_R8 (:3615-3840), W3DIST_R8
// (:5829-5877, with DIST_WITH_SINE, defined at :5827) and W3NNSC (:6819-6879), translated here;
// author T. J. Campbell (NRL), w3gsrumd.F90:21. Evidence: kokkos/port_report/W3GFCL_R8_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement switches/switch_lab_shrd compiles. Not compiled and not ported: W3_S
// (STRACE, :1400-1403, :3636-3639, :5838-5841, :6825-6828) and DIST_CHECK_NAN (:5870-5875;
// :5828 defines DIST_CHECK_NAN____disabled). The debug WRITEs under the DEBUG argument are
// ported as text appended to GfclResult::out (unit *); the error WRITEs to err (unit 0). EXTCDE
// is returned as the code it would exit with (W3SERVMD's EXTCDE, w3servmd.F90:1064-1211, ends
// in CALL EXIT). The GSU object is gsru_gsu_create.hpp's GsuClass.
//
// Arithmetic, as gfortran 16.2 -O3 compiles it (WW3's Release flags, -O3 -g, no -ffp-contract;
// the object's disassembly equals the CMake build's), read from the GIMPLE
// (-fdump-tree-optimized-lineno) and the assembly (v, 2026-11-12):
//  - MOD(X,REAL(D360,8)) is __builtin_fmod, an inline x87 fprem loop (:1464, :1527, :1607);
//    fmod is exact, so std::fmod gives the same value.
//  - The domain test XT.LT.XMIN-LEPS is emitted as XMIN-LEPS > XT (:1475-1476), the same
//    predicate for every input, NaN included (false: a NaN target is searched).
//  - INT((XT-XMIN)/DXB) is a 32-bit cvttsd2si (INT_MIN for a NaN or out-of-range quotient),
//    +1 in 32 bits, then MAX(.,1) and MIN(NBX,.) of integers (:1484-1485).
//  - SUM(XS)/FOUR is (((XS(1)+0.0)+XS(2))+XS(3))+XS(4) times 0.25 (:1616): the +0.0 turns a
//    -0.0 first vertex into +0.0, and the C++ keeps it.
//  - NINT(HALF*MAX(NBX,NBY)) is (double)MAX*0.5 then one lround call, low 32 bits (:1563).
//  - W3DIST_R8 (not inlined): SLAM = sin((DX*0.5)*D2R), SPHI = sin((DY*0.5)*D2R),
//    ARGD = sqrt(((cos(YT*D2R)*cos(YS*D2R))*SLAM)*SLAM + SPHI*SPHI), DIST = asin(ARGD)*(2*R2D)
//    (R2D*TWO folded, exact); sin, cos and asin are libm calls, SQRT is sqrtsd (:5844-5868).
//    SIGN(D360,DX) is copysign (:5850).
//  - W3CKCL_R8 (not inlined): MINVAL(XXS), MAXVAL(YS) are gfortran's NaN-aware inline
//    reductions (gsru_gsu_create.hpp's minval_nan/maxval_nan); SIGN(ONE,CROSS) is copysign;
//    V1X = XXS(J)-XXS(J) (:3743) is computed, not folded (NaN for a non-finite vertex);
//    S90-S90 (:3751) is folded to 0.0, the same value for S90 = +-90.
//  - No FMA (vfmadd: 0 in the object), no loop with floating-point arithmetic vectorised
//    (the vectorised loops of :1661, :3681 are COUNTs).
//
// The GSU argument is INTENT(IN), but NNB = W3NNSC(...) (:1563) is an intrinsic assignment
// through the pointer NNB => GSU%PTR%NNB: it overwrites the object's NNB with a freshly
// allocated one (and leaks the old arrays and the new object, ⚠ not measured). The C++
// overwrites gsu->nnb the same way, without the leak.
//
// Where the Fortran has undefined behaviour the C++ stops with a code that is not a WW3 code:
//  - kExitBucketBounds: B(JB,IB) read outside 1..NBX x 1..NBY. With LCLO, IB is not clamped
//    (:1484), so XT = 360 after XT+D360 rounds up (XT < 0, |XT| < 2.9e-14) gives IB = NBX+1; an
//    object built with BBOX_ONLY has no B.
//  - kExitNoClosestCell: with FNCL, no cell gives DD < HUGE (every bucket searched empty, or
//    every DD NaN or +Inf): IS1..YS1 and LVL1..K1 are read undefined (:1642-1649).
//  - kExitNoNnb: NNB is not associated (an object built with BBOX_ONLY).
//  - kExitNnscBounds: W3NNSC with NLVL < 0 writes N1(0) outside N1(0:NLVL), or (2*NLVL+1)**2
//    overflows.
// POLE is INTENT(OUT) and the Fortran leaves it undefined on the returns of :1479 and :1553;
// the C++ leaves *pole unchanged there.
#pragma once

#include <climits>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

#include "gsru_gsu_create.hpp"

namespace ww::gsru {

// Module parameters, w3gsrumd.F90:377-388, :402, :411. D2R and R2D are the folded double
// constants of the GIMPLE (D2R = 1.74532925199432954743716805978692718781530857086181640625e-2).
inline constexpr double kPi8 = 3.14159265358979323846;
inline constexpr double kD2R = kPi8 / 180.0;
inline constexpr double kR2D = 1.0 / kD2R;
inline constexpr double kD270 = 270.0;
inline constexpr double kOne = 1.0;
inline constexpr double kTwo = 2.0;
inline constexpr double kFour = 4.0;
inline constexpr double kEpsDefault = 1.0e-6;
inline constexpr int kMaxFnclLevel = 3;

inline constexpr int kExitNoClosestCell = -132;  // IS1..K1 read before any assignment
inline constexpr int kExitNoNnb = -133;          // GSU%PTR%NNB not associated
inline constexpr int kExitNnscBounds = -134;     // W3NNSC: NLVL < 0 or NNBR overflow

/// W3NNSC(NLVL) (:6819-6879): the index deltas of the nearest-neighbour rings 0..NLVL.
/// Returns 0 or kExitNnscBounds.
inline int w3nnsc(int nlvl, Nns& nns) {
  const long long side = 2LL * nlvl + 1;
  if (nlvl < 0 || side * side > INT_MAX) return kExitNnscBounds;
  nns.nlvl = nlvl;
  nns.nnbr = static_cast<int>(side * side);
  nns.n1.assign(static_cast<std::size_t>(nlvl) + 1, 0);
  nns.n2.assign(static_cast<std::size_t>(nlvl) + 1, 0);
  nns.di.assign(static_cast<std::size_t>(nns.nnbr), 0);
  nns.dj.assign(static_cast<std::size_t>(nns.nnbr), 0);
  auto at = [](std::vector<int>& v, int k) -> int& { return v[static_cast<std::size_t>(k)]; };
  int n = 0;
  int l = 0;
  at(nns.n1, l) = 0;
  at(nns.n2, l) = (2 * l + 1) * (2 * l + 1) - 1;
  at(nns.di, n) = 0;
  at(nns.dj, n) = 0;
  for (l = 1; l <= nns.nlvl; ++l) {
    at(nns.n1, l) = (2 * l - 1) * (2 * l - 1);
    at(nns.n2, l) = (2 * l + 1) * (2 * l + 1) - 1;
    int j = -l;
    for (int i = -l; i <= l - 1; ++i) {
      n = n + 1;
      at(nns.di, n) = i;
      at(nns.dj, n) = j;
    }
    int i = l;
    for (j = -l; j <= l - 1; ++j) {
      n = n + 1;
      at(nns.di, n) = i;
      at(nns.dj, n) = j;
    }
    j = l;
    for (i = l; i >= -l + 1; --i) {
      n = n + 1;
      at(nns.di, n) = i;
      at(nns.dj, n) = j;
    }
    i = -l;
    for (j = l; j >= -l + 1; --j) {
      n = n + 1;
      at(nns.di, n) = i;
      at(nns.dj, n) = j;
    }
  }
  return 0;
}

/// W3NNSC in the shape of GsuCreateHooks::w3nnsc, so gsu_create() can call the translation.
inline int w3nnsc_hook(void* /*ctx*/, int nlvl, Nns& out) { return w3nnsc(nlvl, out); }

/// W3DIST_R8 (:5829-5877) as switch_lab_shrd compiles it (DIST_WITH_SINE).
inline double w3dist_r8(bool llg, double xt, double yt, double xs, double ys) {
  double dx = xt - xs;
  const double dy = yt - ys;
  double dist = 0.0;
  if (llg) {
    if (std::fabs(dx) > kD270) dx = dx - std::copysign(kD360, dx);
    const double slam = std::sin(kHalf * dx * kD2R);
    const double sphi = std::sin(kHalf * dy * kD2R);
    const double argd = std::sqrt(std::cos(yt * kD2R) * std::cos(ys * kD2R) * slam * slam +
                                  sphi * sphi);
    dist = kR2D * kTwo * std::asin(argd);
  } else {
    dist = std::sqrt(dx * dx + dy * dy);
  }
  return dist;
}

/// exit_code is 0 or the EXTCDE code; out and err are the records written to units * and 0.
struct GfclResult {
  int exit_code = 0;
  bool ingrid = false;
  std::string out;
  std::string err;
};

namespace gfcl_detail {

using gsu_create_detail::fe;
using gsu_create_detail::fi;
using gsu_create_detail::fl;

inline int wrap_add(int a, int b) {
  return static_cast<int>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

inline std::string corners(const char* lbl, const int (&is)[4], const int (&js)[4],
                           const double (&xs)[4], const double (&ys)[4]) {
  std::string s;
  for (int l = 0; l < 4; ++l)
    s += std::string("\n") + lbl + fi(l + 1, 1) + "):" + fi(is[l], 6) + fi(js[l], 6) +
         fe(xs[l], 24, 16) + fe(ys[l], 24, 16);
  return s;
}

}  // namespace gfcl_detail

/// W3CKCL_R8 (:3615-3840) for NS = 4, the only NS W3GFCL_R8 passes (:1540). xt and xs are
/// INOUT: a cell with a branch cut returns them shifted (:3714-3716, :3836-3838). EPS is
/// present (LEPS of the caller, >= 0), so the EXTCDE of :3650-3654 is not reachable.
inline bool w3ckcl_r8(bool llg, double& xt, double& yt, double (&xs)[4], double (&ys)[4],
                      bool& pole, double leps, bool ldbg, std::string& out) {
  using namespace gfcl_detail;
  using gsu_create_detail::maxval_nan;
  using gsu_create_detail::minval_nan;
  constexpr int ns = 4;
  double xxt = xt;
  double yyt = yt;
  double xxs[ns], yys[ns];
  for (int k = 0; k < ns; ++k) {
    xxs[k] = xs[k];
    yys[k] = ys[k];
  }

  bool bcut = false;
  if (llg) {
    int n = 0;
    for (int i = 0; i < ns; ++i) {
      const int j = (i + 1) % ns;
      if (std::fabs(xxs[j] - xxs[i]) > kD180) n = n + 1;
    }
    bcut = n > 1;
    int cnt = 0;
    for (int k = 0; k < ns; ++k)
      if (std::fabs(kD90 - std::fabs(yys[k])) <= leps) ++cnt;
    pole = n == 1 || cnt == 1;
  } else {
    pole = false;
    bcut = false;
  }

  if (bcut) {
    if (minval_nan<double>(ns, [&](std::int64_t k) { return xxs[k]; }) >= kZero) {
      for (double& v : xxs)
        if (v > kD180) v = v - kD360;
      if (xxt > kD180) xxt = xxt - kD360;
    } else {
      for (double& v : xxs)
        if (v < kZero) v = v + kD360;
      if (xxt < kZero) xxt = xxt + kD360;
    }
    if (ldbg) {
      out += "W3CKCL_R8 - CELL INCLUDES A BRANCH CUT\n";
      out += "W3CKCL_R8 - SHIFT BRANCH CUT:" + fe(xxt, 24, 16) + fe(yyt, 24, 16);
      for (int k = 0; k < 4; ++k)
        out += "\n          CORNER(" + fi(k + 1, 1) + "):" + fe(xxs[k], 24, 16) +
               fe(yys[k], 24, 16);
      out += "\n";
    }
  }

  for (int i = 0; i < ns; ++i) {
    if (std::fabs(xxt - xxs[i]) <= leps && std::fabs(yyt - yys[i]) <= leps) {
      if (ldbg)
        out += "W3CKCL_R8 - COINCIDENT WITH CORNER(" + fi(i + 1, 1) + "): " +
               fe(std::fabs(xxt - xxs[i]), 24, 16) + fe(std::fabs(yyt - yys[i]), 24, 16) + "\n";
      if (bcut) {
        xt = xxt;
        for (int k = 0; k < ns; ++k) xs[k] = xxs[k];
      }
      return true;
    }
  }

  double sign1 = 0.0;
  if (pole) {
    if (ldbg) out += "W3CKCL_R8 - CELL INCLUDES A POLE\n";
    double s90 = kD90;
    if (maxval_nan<double>(ns, [&](std::int64_t k) { return ys[k]; }) < kZero) s90 = -kD90;
    for (int i = 0; i < ns; ++i) {
      bool lsbc = true;
      const int j = (i + 1) % ns;
      sign1 = 0.0;
      for (int k = 1; k <= 4 && lsbc; ++k) {
        double v1x = 0, v1y = 0, v2x = 0, v2y = 0;
        switch (k) {
          case 1:
            v1x = xxs[j] - xxs[i];
            v1y = yys[j] - yys[i];
            v2x = xxt - xxs[i];
            v2y = yyt - yys[i];
            break;
          case 2:
            v1x = xxs[j] - xxs[j];
            v1y = s90 - yys[j];
            v2x = xxt - xxs[j];
            v2y = yyt - yys[j];
            break;
          case 3:
            v1x = xxs[i] - xxs[j];
            v1y = s90 - s90;
            v2x = xxt - xxs[j];
            v2y = yyt - s90;
            break;
          default:
            v1x = xxs[i] - xxs[i];
            v1y = yys[i] - s90;
            v2x = xxt - xxs[i];
            v2y = yyt - s90;
            break;
        }
        if (std::fabs(v1x) > kD180) v1x = v1x - std::copysign(kD360, v1x);
        if (std::fabs(v2x) > kD180) v2x = v2x - std::copysign(kD360, v2x);
        double cross = v1x * v2y - v1y * v2x;
        if (std::fabs(cross) < leps) cross = kZero;
        if (ldbg)
          out += "W3CKCL_R8 - CROSS(" + fi(i + 1, 1) + "," + fi(j + 1, 1) + "," + fi(k, 1) +
                 "):" + fe(v1x, 24, 16) + fe(v1y, 24, 16) + fe(v2x, 24, 16) + fe(v2y, 24, 16) +
                 fe(cross, 24, 16) + "\n";
        if (std::fabs(sign1) <= leps) {
          if (std::fabs(cross) > leps) sign1 = std::copysign(kOne, cross);
        } else {
          if (std::fabs(cross) > leps) {
            if (std::copysign(kOne, cross) != sign1) lsbc = false;  // CYCLE SUBCELL_LOOP
          }
        }
      }
      if (lsbc) return true;
    }
    return false;
  }

  const double xct = xxt;
  const double yct = yyt;
  const double(&xcs)[ns] = xxs;
  const double(&ycs)[ns] = yys;
  sign1 = 0.0;
  for (int i = 0; i < ns; ++i) {
    const int j = (i + 1) % ns;
    const double v1x = xcs[j] - xcs[i];
    const double v1y = ycs[j] - ycs[i];
    const double v2x = xct - xcs[i];
    const double v2y = yct - ycs[i];
    double cross = v1x * v2y - v1y * v2x;
    if (std::fabs(cross) < leps) cross = kZero;
    if (ldbg)
      out += "W3CKCL_R8 - CROSS(" + fi(i + 1, 1) + "," + fi(j + 1, 1) + "):" + fe(v1x, 24, 16) +
             fe(v1y, 24, 16) + fe(v2x, 24, 16) + fe(v2y, 24, 16) + fe(cross, 24, 16) + "\n";
    if (std::fabs(sign1) <= leps) {
      if (std::fabs(cross) > leps) sign1 = std::copysign(kOne, cross);
    } else {
      if (std::fabs(cross) > leps) {
        if (std::copysign(kOne, cross) != sign1) return false;
      }
    }
  }
  if (bcut) {
    xt = xxt;
    for (int k = 0; k < ns; ++k) xs[k] = xxs[k];
  }
  return true;
}

/// W3GFCL_R8 (:1369-1669), statement for statement. gsu is GSU%PTR (null: not associated);
/// pole, eps, fncl, debug are the OPTIONAL arguments (null: absent).
inline GfclResult w3gfcl_r8(GsuClass* gsu, double& xt, double& yt, int (&is)[4], int (&js)[4],
                            double (&xs)[4], double (&ys)[4], bool* pole, const double* eps,
                            const bool* fncl, const bool* debug) {
  using namespace gfcl_detail;
  using gsu_create_detail::Shape;
  using gsu_create_detail::fortran_int;
  GfclResult r;
  const char* const kErr = "W3GFCL_R8 ERROR -- ";

  // 1. Test input (:1408-1422)
  if (gsu == nullptr) {
    r.err += std::string("\n") + kErr + "grid search utility object not created\n\n";
    r.exit_code = 1;
    return r;
  }
  double leps = kEpsDefault;
  if (eps != nullptr) {
    if (*eps < kZero) {
      r.err += std::string("\n") + kErr + "EPS parameter must be >= 0\n\n";
      r.exit_code = 1;
      return r;
    }
    leps = *eps;
  }

  // 2. Initialize search (:1427-1485)
  const bool lfncl = fncl != nullptr ? *fncl : false;
  const bool ldbg = debug != nullptr ? *debug : false;
  GsuClass& p = *gsu;
  const bool ijg = p.ijg, llg = p.llg, lclo = p.lclo, l360 = p.l360;
  const int iclo = p.iclo, gkind = p.gkind;
  const int lbx = p.lbx, lby = p.lby, ubx = p.ubx, uby = p.uby, nx = p.nx, ny = p.ny;
  const int nbx = p.nbx, nby = p.nby;
  const double dxb = p.dxb, dyb = p.dyb, xmin = p.xmin, ymin = p.ymin, xmax = p.xmax,
               ymax = p.ymax;
  const Shape sh{p.lb[0], p.ub[0], p.lb[1], p.ub[1]};

  r.ingrid = false;
  if (llg) {
    xt = std::fmod(xt, kD360);
    if (lclo || l360) {
      if (xt < kZero) xt = xt + kD360;
    } else {
      if (xt > kD180) xt = xt - kD360;
    }
  }
  if (ldbg) r.out += "\nW3GFCL_R8 - TARGET POINT:" + fe(xt, 24, 16) + fe(yt, 24, 16) + "\n";

  if (!lfncl) {
    if (xt < xmin - leps || xt > xmax + leps || yt < ymin - leps || yt > ymax + leps) {
      if (ldbg) r.out += "W3GFCL_R8 - TARGET POINT OUTSIDE SEARCH DOMAIN\n";
      return r;
    }
  }

  int ib = std::max(wrap_add(fortran_int((xt - xmin) / dxb), 1), 1);
  if (!lclo) ib = std::min(nbx, ib);
  int jb = std::max(wrap_add(fortran_int((yt - ymin) / dyb), 1), 1);
  jb = std::min(nby, jb);

  auto bucket = [&](int jbi, int ibi) -> Bkt* {
    if (ibi < 1 || ibi > nbx || jbi < 1 || jbi > nby || p.b.empty()) return nullptr;
    return &p.bkt(jbi, ibi);
  };

  // Cell corner indices and coordinates, :1496-1534 and :1576-1614 (identical).
  auto setup_cell = [&](const Bkt& bk, int k) {
    const int i0 = bk.i[static_cast<std::size_t>(k - 1)];
    const int j0 = bk.j[static_cast<std::size_t>(k - 1)];
    is[0] = i0;
    js[0] = j0;
    is[1] = i0 + 1;
    js[1] = j0;
    is[2] = i0 + 1;
    js[2] = j0 + 1;
    is[3] = i0;
    js[3] = j0 + 1;
    for (int l = 0; l < 4; ++l) {
      if (iclo % 2 == 0) is[l] = lbx + (nx - 1 + (is[l] - lbx + 1) % nx) % nx;
      if (iclo % 3 == 0) js[l] = lby + (ny - 1 + (js[l] - lby + 1) % ny) % ny;
      if (iclo == kIcloTrpl && js[l] > uby) {
        is[l] = ubx + lbx - is[l];
        js[l] = 2 * uby - js[l] + 1;
      }
      const std::int64_t o = ijg ? sh.at(is[l], js[l]) : sh.at(js[l], is[l]);
      if (gkind == 4) {
        xs[l] = static_cast<double>(p.xg4[o]);
        ys[l] = static_cast<double>(p.yg4[o]);
      } else {
        xs[l] = p.xg8[o];
        ys[l] = p.yg8[o];
      }
      if (llg) {
        xs[l] = std::fmod(xs[l], kD360);
        if (lclo || l360) {
          if (xs[l] < kZero) xs[l] = xs[l] + kD360;
        } else {
          if (xs[l] > kD180) xs[l] = xs[l] - kD360;
        }
      }
    }
  };

  // 3. Search for enclosing cell in bucket (:1490-1553)
  Bkt* b0 = bucket(jb, ib);
  if (b0 == nullptr) {
    r.exit_code = kExitBucketBounds;
    return r;
  }
  if (ldbg)
    r.out += "W3GFCL_R8 - BUCKET SEARCH:" + fi(ib, 6) + fi(jb, 6) + fi(b0->n, 6) +
             fe(xmin + static_cast<double>(ib - 1) * dxb, 24, 16) +
             fe(ymin + static_cast<double>(jb - 1) * dyb, 24, 16) +
             fe(xmin + static_cast<double>(ib) * dxb, 24, 16) +
             fe(ymin + static_cast<double>(jb) * dyb, 24, 16) + "\n";
  bool lplc = false;
  for (int k = 1; k <= b0->n; ++k) {
    setup_cell(*b0, k);
    if (ldbg)
      r.out += "W3GFCL_R8 - CHECK CELL:" + fi(ib, 6) + fi(jb, 6) + fi(k, 6) +
               corners("          CORNER(", is, js, xs, ys) + "\n";
    const bool incell = w3ckcl_r8(llg, xt, yt, xs, ys, lplc, leps, ldbg, r.out);
    if (ldbg) r.out += "W3GFCL_R8 - INCELL:" + fl(incell, 2) + "\n";
    if (incell) {
      if (ldbg) {
        r.out += "W3GFCL_R8 - ENCLOSING CELL:" + fi(ib, 6) + fi(jb, 6) + fi(k, 6);
        for (int l = 0; l < 4; ++l) r.out += fi(is[l], 6) + fi(js[l], 6);
        r.out += "\n";
      }
      if (pole != nullptr) *pole = lplc;
      r.ingrid = true;
      break;
    }
  }
  if (r.ingrid) return r;
  if (!lfncl) return r;

  // 4. If not in grid, then identify cell closest to target point (:1559-1667)
  int nlevel = 0;
  double dd1 = std::numeric_limits<double>::max();
  const int ib0 = ib, jb0 = jb;
  int ib1 = ib, jb1 = jb;
  int lvl1 = 0, n1 = 0, k1 = 0;
  bool have1 = false;
  int is1[4] = {0, 0, 0, 0}, js1[4] = {0, 0, 0, 0};
  double xs1[4] = {0, 0, 0, 0}, ys1[4] = {0, 0, 0, 0};
  if (p.nnb == nullptr) {
    r.exit_code = kExitNoNnb;
    return r;
  }
  {
    Nns fresh;
    const double half_max = static_cast<double>(std::max(nbx, nby)) * kHalf;
    const int rc = w3nnsc(static_cast<int>(std::lround(half_max)), fresh);
    if (rc != 0) {
      r.exit_code = rc;
      return r;
    }
    *p.nnb = std::move(fresh);
  }
  const Nns& nnb = *p.nnb;
  if (ldbg)
    r.out += "W3GFCL_R8 - CLOSEST CELL SEARCH:" + fi(ib0, 6) + fi(jb0, 6) + fi(nnb.nlvl, 6) +
             "\n";
  for (int lvl = 0; lvl <= nnb.nlvl; ++lvl) {
    bool found = false;
    const int na = nnb.n1[static_cast<std::size_t>(lvl)];
    const int nb = nnb.n2[static_cast<std::size_t>(lvl)];
    for (int n = na; n <= nb; ++n) {
      ib = ib0 + nnb.di[static_cast<std::size_t>(n)];
      jb = jb0 + nnb.dj[static_cast<std::size_t>(n)];
      if (ib < 1 || ib > nbx) continue;
      if (jb < 1 || jb > nby) continue;
      Bkt* bk = bucket(jb, ib);
      if (bk == nullptr) {
        r.exit_code = kExitBucketBounds;
        return r;
      }
      if (ldbg)
        r.out += "W3GFCL_R8 - CHECK BUCKET:" + fi(lvl, 6) + fi(n, 6) + fi(ib, 6) + fi(jb, 6) +
                 fi(bk->n, 6) + "\n";
      for (int k = 1; k <= bk->n; ++k) {
        setup_cell(*bk, k);
        const double xsm = (((xs[0] + 0.0) + xs[1]) + xs[2]) + xs[3];
        const double ysm = (((ys[0] + 0.0) + ys[1]) + ys[2]) + ys[3];
        const double xsm4 = xsm / kFour;
        const double ysm4 = ysm / kFour;
        const double dd = w3dist_r8(llg, xt, yt, xsm4, ysm4);
        if (ldbg)
          r.out += "W3GFCL_R8 - CHECK CELL:" + fi(lvl, 6) + fi(n, 6) + fi(ib, 6) + fi(jb, 6) +
                   fi(k, 6) + fe(xsm4, 24, 16) + fe(ysm4, 24, 16) + fe(dd, 24, 16) +
                   corners("          CORNER(", is, js, xs, ys) + "\n";
        if (dd < dd1) {
          lvl1 = lvl;
          n1 = n;
          ib1 = ib;
          jb1 = jb;
          k1 = k;
          dd1 = dd;
          for (int l = 0; l < 4; ++l) {
            is1[l] = is[l];
            js1[l] = js[l];
            xs1[l] = xs[l];
            ys1[l] = ys[l];
          }
          have1 = true;
        }
        found = true;
      }
    }
    if (found) nlevel = nlevel + 1;
    if (nlevel >= kMaxFnclLevel) break;
  }

  if (!have1) {
    r.exit_code = kExitNoClosestCell;
    return r;
  }
  for (int l = 0; l < 4; ++l) {
    is[l] = is1[l];
    js[l] = js1[l];
    xs[l] = xs1[l];
    ys[l] = ys1[l];
  }
  if (ldbg)
    r.out += "W3GFCL_R8 - CLOSEST CELL:" + fi(lvl1, 6) + fi(n1, 6) + fi(ib1, 6) + fi(jb1, 6) +
             fi(k1, 6) + fe(dd1, 24, 16) + corners("          CORNER(", is, js, xs, ys) + "\n";

  if (llg) {
    int n = 0;
    for (int i = 0; i < 4; ++i) {
      const int j = (i + 1) % 4;
      if (std::fabs(xs[j] - xs[i]) > kD180) n = n + 1;
    }
    int cnt = 0;
    for (int l = 0; l < 4; ++l)
      if (std::fabs(ys[l]) == kD90) ++cnt;
    lplc = n == 1 || cnt == 1;
    if (lplc && ldbg) r.out += "W3GFCL_R8 - CELL INCLUDES A POLE\n";
  } else {
    lplc = false;
  }
  if (pole != nullptr) *pole = lplc;
  return r;
}

}  // namespace ww::gsru
