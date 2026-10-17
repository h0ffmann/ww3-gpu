// kokkos/src/ww_kokkos/gsru_gsu_create.hpp
// GSU_CREATE: build the grid-search-utility object of W3GSRUMD over a logically rectangular grid
// (rectilinear or curvilinear, Cartesian or spherical): the grid's index closure, its
// longitudinal periodicity (LCLO) and range (L360), the bounding box of the search domain with
// the poles it contains, and the bucket-search structure, NBY x NBX buckets of extent DXB x DYB,
// each listing the cells (by their lower-left corner) whose vertex box overlaps it. Host-side, no
// View is created and no kernel runs. Phase 1, translated, not regtested.
// WW3 heritage: FUNCTION GSU_CREATE, WW3/model/src/w3gsrumd.F90:7354-7849 at h0ffmann/WW3 8e433f99
// (byte-identical to NOAA-EMC/WW3 03adda91: git -C WW3 diff 03adda91 HEAD -- model/src/w3gsrumd.F90
// is empty); author T. J. Campbell (NRL), w3gsrumd.F90:21. Evidence:
// kokkos/port_report/GSU_CREATE_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement switches/switch_lab_shrd compiles. The routine has one #ifdef, W3_S
// (STRACE, :7382-7385), not compiled and not ported; the debug WRITEs under the DEBUG argument
// are compiled and are ported, as text appended to GsuCreateResult::out (unit *, stdout); the
// error WRITEs go to GsuCreateResult::err (unit 0). EXTCDE is returned as the code it would exit
// with (EXTCDE ends in CALL EXIT, w3servmd.F90:1207). W3NNSC (:6819-6879) is not translated:
// it is the hook GsuCreateHooks::w3nnsc, called where the Fortran calls it (:7482, :7484, :7829);
// an empty hook ends with kExitMissingHook.
//
// Arithmetic, as gfortran 16.2 -O3 compiles it (WW3's Release flags, -O3 -g, no
// -ffp-contract; the object's disassembly equals the CMake build's), read from the GIMPLE
// (-fdump-tree-optimized-lineno) and the assembly:
//  - XC/YC are REAL(8); an R4 grid is widened by cvtss2sd on each load (:7537-7552, :7700-7715).
//  - ABS(XC(K)-XC(L)) .GT. D180 with K = MOD(L,4)+1: XC(2)-XC(1), XC(3)-XC(2), XC(4)-XC(3),
//    XC(1)-XC(4) (:7564, :7734).
//  - MINVAL and MAXVAL of a REAL array are gfortran's NaN-aware inline reductions: the first
//    element <= +Inf (>= -Inf) starts the reduction, later ones replace it when strictly less
//    (greater); all NaN gives NaN; an empty array gives HUGE (-HUGE) of the array's kind
//    (:7574, :7580, :7597-7601). minval_nan() and maxval_nan() follow that.
//  - MOD(XC,D360) is __builtin_fmod, emitted as an inline x87 fprem loop; fmod is exact, so
//    std::fmod returns the same value (:7724).
//  - DXB = (XMAX-XMIN)/REAL(NBX): REAL(NBX) is a default REAL, cvtsi2ss then cvtss2sd, so the
//    divisor is (double)(float)NBX (:7624-7625).
//  - INT((XC(L)-XMIN)/DXB) is a 32-bit cvttsd2si: a NaN or out-of-range quotient gives
//    INT_MIN (:7744, :7746); fortran_int() reproduces it without C++'s undefined conversion.
//  - NINT(HALF*MAX(NBX,NBY)) calls lround on the double and keeps the low 32 bits (:7829).
//
// Where the Fortran has undefined behaviour the C++ stops with a code that is not a WW3 code:
//  - JB1(1), JB2(1) are not set in the pole branch when the cell has neither MINVAL(YC) > 0 nor
//    MAXVAL(YC) < 0 (:7754-7761); the Fortran then uses the previous cell's values (kept here,
//    as the locals persist across cells and across ISTEP) or, for the first such cell, an
//    undefined value: kExitUndefinedBounds.
//  - A bucket index outside 1..NBX x 1..NBY, or a second-pass count above the first pass's (from
//    the stale bounds above), writes outside B or B%I/B%J: kExitBucketBounds.
#pragma once

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace ww::gsru {

// Index-closure parameters, w3gsrumd.F90:314-319.
inline constexpr int kIcloNone = -1;
inline constexpr int kIcloSmpl = 2;
inline constexpr int kIcloGrdi = kIcloSmpl;
inline constexpr int kIcloGrdj = 3;
inline constexpr int kIcloTrdl = 6;
inline constexpr int kIcloTrpl = 8;

// Module parameters, w3gsrumd.F90:384-389, :406, :408.
inline constexpr double kD360 = 360.0;
inline constexpr double kD180 = 180.0;
inline constexpr double kD90 = 90.0;
inline constexpr double kZero = 0.0;
inline constexpr double kHalf = 0.5;
inline constexpr int kNcbDefault = 10;
inline constexpr int kNnpDefault = 2;

/// ISTAT of a failed ALLOCATE: LIBERROR_ALLOCATION, the constant the GIMPLE stores (:7449).
inline constexpr int kStatAllocation = 5020;
/// Codes outside the Fortran's EXTCDE codes. -101 is ww::iogr::kExitMissingHook's value.
inline constexpr int kExitMissingHook = -101;
inline constexpr int kExitUndefinedBounds = -130;  // JB1(1)/JB2(1) read before any assignment
inline constexpr int kExitBucketBounds = -131;     // a write outside B or B%I/B%J

/// T_NNS (w3gsrumd.F90:366-373): nearest-neighbor search index deltas. N1, N2 run 0:NLVL and DI,
/// DJ run 0:NNBR-1; element k of a vector is index k.
struct Nns {
  int nlvl = 0;
  int nnbr = 0;
  std::vector<int> n1, n2, di, dj;
};

/// T_BKT (:358-362). An empty i/j is a nullified pointer: ISTEP 2 allocates only when N > 0
/// (:7653-7676). The lists are sized by the first pass; n is the second pass's count.
struct Bkt {
  int n = 0;
  std::vector<int> i, j;
};

/// CLASS_GSU (:332-354), what GSU%PTR points at. The grid coordinates are not copied: XG4 => XG4
/// (:7465-7470) keeps the caller's array, Fortran XG(LB(1):UB(1),LB(2):UB(2)), column-major.
/// nbx .. nnb are left undefined by the Fortran when BBOX_ONLY returns at :7611-7614; here they
/// stay 0, empty and null.
struct GsuClass {
  bool ijg = false;
  bool llg = false;
  int iclo = 0;
  bool lclo = false;
  bool l360 = false;
  int gkind = 0;
  int lbx = 0, lby = 0, ubx = 0, uby = 0, nx = 0, ny = 0;
  int lb[2] = {0, 0};
  int ub[2] = {0, 0};
  const float* xg4 = nullptr;
  const float* yg4 = nullptr;
  const double* xg8 = nullptr;
  const double* yg8 = nullptr;
  std::unique_ptr<Nns> nnp;
  int nbx = 0, nby = 0;
  double dxb = 0.0, dyb = 0.0;
  double xmin = 0.0, ymin = 0.0, xmax = 0.0, ymax = 0.0;
  std::vector<Bkt> b;  ///< B(NBY,NBX): B(jb,ib) is b[(jb-1) + (ib-1)*nby]
  std::unique_ptr<Nns> nnb;

  Bkt& bkt(int jb, int ib) {
    return b[static_cast<std::size_t>(jb - 1) +
             static_cast<std::size_t>(ib - 1) * static_cast<std::size_t>(nby)];
  }
};

/// The arguments of GSU_CREATE (:7354-7370). A null pointer is an absent OPTIONAL. TYPE_R4 needs
/// both xg4 and yg4, TYPE_R8 both xg8 and yg8; with both, the R4 pair is used (:7464).
struct GsuCreateArgs {
  bool ijg = true;
  bool llg = false;
  int iclo = kIcloNone;
  int lb[2] = {1, 1};
  int ub[2] = {0, 0};
  const float* xg4 = nullptr;
  const float* yg4 = nullptr;
  const double* xg8 = nullptr;
  const double* yg8 = nullptr;
  const bool* bbox_only = nullptr;
  const int* ncb = nullptr;
  const int* nnp = nullptr;
  const bool* debug = nullptr;
};

/// W3NNSC(NLVL) (:6819-6879), not translated: the caller fills `out` and returns 0, or the code
/// the Fortran would exit with.
struct GsuCreateHooks {
  void* ctx = nullptr;
  int (*w3nnsc)(void* ctx, int nlvl, Nns& out) = nullptr;
};

/// exit_code is 0 or the EXTCDE code; gsu is GSU%PTR when exit_code is 0. out and err are the
/// records the WRITEs to units * and 0 print, each ended by its newline.
struct GsuCreateResult {
  int exit_code = 0;
  std::unique_ptr<GsuClass> gsu;
  std::string out;
  std::string err;
};

namespace gsu_create_detail {

/// Iw: right-justified, all '*' when it does not fit.
inline std::string fi(int v, int w) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%d", v);
  const std::string s(buf);
  if (static_cast<int>(s.size()) > w) return std::string(static_cast<std::size_t>(w), '*');
  return std::string(static_cast<std::size_t>(w) - s.size(), ' ') + s;
}

/// Lw: w-1 blanks, then T or F.
inline std::string fl(bool v, int w) {
  return std::string(static_cast<std::size_t>(w - 1), ' ') + (v ? "T" : "F");
}

/// Aw for a string no longer than w: right-justified.
inline std::string fa(const std::string& s, int w) {
  return std::string(static_cast<std::size_t>(w) - s.size(), ' ') + s;
}

/// Ew.d of a REAL(8), scale factor 0, default rounding, w >= 9: 0.d1..dd then E+nn, or +nnn
/// without the E when |exponent| > 99; zero has exponent +00; -0.0 keeps its sign. The digits
/// are snprintf "%+-#.*e" with precision d-1 (libgfortran's DTOA). Inf and NaN are
/// right-justified "Infinity" ("-Infinity") and "NaN".
inline std::string fe(double x, int w, int d) {
  if (!std::isfinite(x)) {
    std::string p(static_cast<std::size_t>(w), ' ');
    if (std::isnan(x)) {
      p.replace(static_cast<std::size_t>(w - 3), 3, "NaN");
    } else {
      p.replace(static_cast<std::size_t>(w - 8), 8, "Infinity");
      if (std::signbit(x)) p[static_cast<std::size_t>(w - 9)] = '-';
    }
    return p;
  }
  char buf[512];
  std::snprintf(buf, sizeof buf, "%+-#.*e", d - 1, x);
  const bool neg = buf[0] == '-';
  const std::string bs(buf);
  const std::size_t epos = bs.find('e');
  std::string mant;
  for (std::size_t k = 1; k < epos; ++k)
    if (bs[k] != '.') mant += bs[k];
  int e = std::atoi(bs.c_str() + epos + 1);
  bool zero = true;
  for (const char c : mant) zero = zero && c == '0';
  if (!zero) ++e;
  char ex[16];
  const int ae = e < 0 ? -e : e;
  if (ae <= 99)
    std::snprintf(ex, sizeof ex, "E%c%02d", e < 0 ? '-' : '+', ae);
  else
    std::snprintf(ex, sizeof ex, "%c%03d", e < 0 ? '-' : '+', ae);
  std::string s = std::string(neg ? "-" : "") + "0." + mant + ex;
  if (static_cast<int>(s.size()) == w + 1) s = std::string(neg ? "-" : "") + "." + mant + ex;
  if (static_cast<int>(s.size()) > w) return std::string(static_cast<std::size_t>(w), '*');
  return std::string(static_cast<std::size_t>(w) - s.size(), ' ') + s;
}

/// INT() of a REAL(8) as the 32-bit cvttsd2si computes it: truncation, INT_MIN for a NaN or a
/// value outside the int range.
inline int fortran_int(double q) {
  if (q > -2147483649.0 && q < 2147483648.0) return static_cast<int>(q);
  return INT_MIN;
}

/// gfortran's inline MINVAL of a REAL array of n elements read by get(k), k = 0..n-1 in array
/// element order (GIMPLE :7574, :7597): +Inf start, the first element <= limit starts the
/// reduction, then strict <; all NaN gives NaN, no element gives HUGE of kind T.
template <class T, class Get>
double minval_nan(std::int64_t n, Get get) {
  T limit = std::numeric_limits<T>::infinity();
  bool fast = false;
  bool nonempty = false;
  for (std::int64_t k = 0; k < n; ++k) {
    const T v = get(k);
    nonempty = true;
    if (fast) {
      if (v < limit) limit = v;
    } else if (v <= limit) {
      limit = v;
      fast = true;
    }
  }
  if (fast) return static_cast<double>(limit);
  return nonempty ? std::numeric_limits<double>::quiet_NaN()
                  : static_cast<double>(std::numeric_limits<T>::max());
}

/// MAXVAL, the mirror image: -Inf start, >= then strict >, -HUGE when empty (:7580, :7598).
template <class T, class Get>
double maxval_nan(std::int64_t n, Get get) {
  T limit = -std::numeric_limits<T>::infinity();
  bool fast = false;
  bool nonempty = false;
  for (std::int64_t k = 0; k < n; ++k) {
    const T v = get(k);
    nonempty = true;
    if (fast) {
      if (v > limit) limit = v;
    } else if (v >= limit) {
      limit = v;
      fast = true;
    }
  }
  if (fast) return static_cast<double>(limit);
  return nonempty ? std::numeric_limits<double>::quiet_NaN()
                  : -static_cast<double>(std::numeric_limits<T>::max());
}

inline double minval4(const double (&a)[4]) {
  return minval_nan<double>(4, [&](std::int64_t k) { return a[k]; });
}
inline double maxval4(const double (&a)[4]) {
  return maxval_nan<double>(4, [&](std::int64_t k) { return a[k]; });
}

/// The grid array XG(LB(1):UB(1),LB(2):UB(2)), column-major: number of elements, and the
/// offset of element (i1,i2).
struct Shape {
  int lb0, ub0, lb1, ub1;
  std::int64_t ext0() const { return ub0 >= lb0 ? std::int64_t{ub0} - lb0 + 1 : 0; }
  std::int64_t ext1() const { return ub1 >= lb1 ? std::int64_t{ub1} - lb1 + 1 : 0; }
  std::int64_t size() const { return ext0() * ext1(); }
  std::int64_t at(int i1, int i2) const {
    return (std::int64_t{i1} - lb0) + (std::int64_t{i2} - lb1) * (std::int64_t{ub0} - lb0 + 1);
  }
};

/// "Create list of cell vertices", "apply index closure" and "copy cell vertex coordinates" of
/// :7523-7555 and :7686-7718, identical in both loops.
inline void cell_vertices(const GsuCreateArgs& a, bool type_r4, const Shape& sh, int i, int j,
                          int lbx, int lby, int ubx, int uby, int nx, int ny, int (&ic)[4],
                          int (&jc)[4], double (&xc)[4], double (&yc)[4]) {
  const int iclo = a.iclo;
  ic[0] = i;
  jc[0] = j;
  ic[1] = i + 1;
  jc[1] = j;
  ic[2] = i + 1;
  jc[2] = j + 1;
  ic[3] = i;
  jc[3] = j + 1;
  for (int l = 0; l < 4; ++l) {
    if (iclo % 2 == 0) ic[l] = lbx + (nx - 1 + (ic[l] - lbx + 1) % nx) % nx;
    if (iclo % 3 == 0) jc[l] = lby + (ny - 1 + (jc[l] - lby + 1) % ny) % ny;
    if (iclo == kIcloTrpl && jc[l] > uby) {
      ic[l] = ubx + lbx - ic[l];
      jc[l] = 2 * uby - jc[l] + 1;
    }
    const std::int64_t o = a.ijg ? sh.at(ic[l], jc[l]) : sh.at(jc[l], ic[l]);
    if (type_r4) {
      xc[l] = static_cast<double>(a.xg4[o]);
      yc[l] = static_cast<double>(a.yg4[o]);
    } else {
      xc[l] = a.xg8[o];
      yc[l] = a.yg8[o];
    }
  }
}

inline std::string rec_a_ints(const char* msg, const int* v, int n) {
  std::string s(msg);
  for (int k = 0; k < n; ++k) s += fi(v[k], 6);
  return s + "\n";
}

inline std::string rec_a_e4(const char* msg, const double (&v)[4]) {
  std::string s(msg);
  for (int k = 0; k < 4; ++k) s += fe(v[k], 24, 16);
  return s + "\n";
}

}  // namespace gsu_create_detail

/// GSU_CREATE (:7354-7849), statement for statement.
inline GsuCreateResult gsu_create(const GsuCreateArgs& a, const GsuCreateHooks& h) {
  using namespace gsu_create_detail;
  GsuCreateResult r;
  const char* const kErr = "W3GSUC ERROR -- ";

  // 1. Test input (:7386-7444)
  const bool type_r4 = a.xg4 != nullptr && a.yg4 != nullptr;
  const bool type_r8 = a.xg8 != nullptr && a.yg8 != nullptr;
  if (!type_r4 && !type_r8) {
    r.err += std::string("\n") + kErr + "no input grid coordinates specified\n";
    r.exit_code = 1;
    return r;
  }
  int lbx, lby, ubx, uby;
  if (a.ijg) {
    lbx = a.lb[0];
    lby = a.lb[1];
    ubx = a.ub[0];
    uby = a.ub[1];
  } else {
    lbx = a.lb[1];
    lby = a.lb[0];
    ubx = a.ub[1];
    uby = a.ub[0];
  }
  const int nx = ubx - lbx + 1;
  const int ny = uby - lby + 1;
  const int iclo = a.iclo;
  switch (iclo) {
    case kIcloNone:
    case kIcloGrdi:
    case kIcloGrdj:
    case kIcloTrdl:
    case kIcloTrpl:
      break;
    default:
      r.err += std::string("\n") + kErr + "unsupported ICLO: " + fi(iclo, 2) + "\n\n";
      r.exit_code = 1;
      return r;
  }
  if (iclo == kIcloTrpl && nx % 2 != 0) {
    r.err += std::string("\n") + kErr + "tripole grid closure requires NX=UBX-LBX+1 be even\n\n";
    r.exit_code = 1;
    return r;
  }
  const bool lbbox = a.bbox_only != nullptr ? *a.bbox_only : false;
  if (a.ncb != nullptr && *a.ncb <= 0) {
    r.err += std::string("\n") + kErr + "NCB must be greater than zero\n\n";
    r.exit_code = 1;
    return r;
  }
  const bool ldbg = a.debug != nullptr ? *a.debug : false;

  // 2. Allocate object and set grid related data and pointers (:7449-7475)
  std::unique_ptr<GsuClass> pp;
  try {
    pp = std::make_unique<GsuClass>();
  } catch (const std::bad_alloc&) {
    r.err += std::string("\n") + kErr + "gsu object allocation failed\n\n";
    r.exit_code = kStatAllocation;
    return r;
  }
  GsuClass& p = *pp;
  p.ijg = a.ijg;
  p.llg = a.llg;
  p.iclo = iclo;
  p.lbx = lbx;
  p.lby = lby;
  p.ubx = ubx;
  p.uby = uby;
  p.nx = nx;
  p.ny = ny;
  p.lb[0] = a.lb[0];
  p.lb[1] = a.lb[1];
  p.ub[0] = a.ub[0];
  p.ub[1] = a.ub[1];
  if (type_r4) {
    p.xg4 = a.xg4;
    p.yg4 = a.yg4;
    p.gkind = 4;
  } else {
    p.xg8 = a.xg8;
    p.yg8 = a.yg8;
    p.gkind = 8;
  }

  // 3. Create nearest-neighbor point search object (:7480-7486)
  if (!lbbox) {
    if (h.w3nnsc == nullptr) {
      r.exit_code = kExitMissingHook;
      return r;
    }
    p.nnp = std::make_unique<Nns>();
    const int rc = h.w3nnsc(h.ctx, a.nnp != nullptr ? *a.nnp : kNnpDefault, *p.nnp);
    if (rc != 0) {
      r.exit_code = rc;
      return r;
    }
  }

  // 4. Construct bucket search "object" (:7492-7842)
  const int lxc = lbx;
  const int lyc = lby;
  int uxc = 0, uyc = 0;
  switch (iclo) {
    case kIcloNone:
      uxc = ubx - 1;
      uyc = uby - 1;
      break;
    case kIcloGrdi:
      uxc = ubx;
      uyc = uby - 1;
      break;
    case kIcloGrdj:
      uxc = ubx - 1;
      uyc = uby;
      break;
    default:  // ICLO_TRDL, ICLO_TRPL
      uxc = ubx;
      uyc = uby;
      break;
  }
  p.lclo = a.llg && iclo != kIcloNone;

  const Shape sh{a.lb[0], a.ub[0], a.lb[1], a.ub[1]};
  int ic[4], jc[4];
  double xc[4], yc[4];
  int n = 0;  // N; the Fortran leaves it undefined until the first LLG cell (:7561)

  if (ldbg) r.out += "\nW3GSUC - check source grid\n";
  bool lnpl = false;
  bool lspl = false;
  for (int i = lxc; i <= uxc; ++i) {
    for (int j = lyc; j <= uyc; ++j) {
      cell_vertices(a, type_r4, sh, i, j, lbx, lby, ubx, uby, nx, ny, ic, jc, xc, yc);
      if (a.llg) {
        n = 0;
        for (int l = 0; l < 4; ++l) {
          const int k = (l + 1) % 4;
          if (std::fabs(xc[k] - xc[l]) > kD180) n = n + 1;
        }
        const bool lbc = n > 1;
        int icjc[8] = {ic[0], ic[1], ic[2], ic[3], jc[0], jc[1], jc[2], jc[3]};
        if (lbc && ldbg) r.out += rec_a_ints("W3GSUC -- cell includes branch cut:", icjc, 8);
        int cnt = 0;
        for (int l = 0; l < 4; ++l)
          if (std::fabs(yc[l]) == kD90) ++cnt;
        const bool lpl = n == 1 || cnt == 1;
        if (lpl && minval4(yc) > kZero) {
          if (ldbg) r.out += rec_a_ints("W3GSUC -- cell includes N-pole:", icjc, 8);
          lnpl = true;
        }
        if (lpl && maxval4(yc) < kZero) {
          if (ldbg) r.out += rec_a_ints("W3GSUC -- cell includes S-pole:", icjc, 8);
          lspl = true;
        }
        if (n > 0) p.lclo = true;
      }
    }
  }

  if (type_r4) {
    const float* x = a.xg4;
    const float* y = a.yg4;
    p.xmin = minval_nan<float>(sh.size(), [&](std::int64_t k) { return x[k]; });
    p.xmax = maxval_nan<float>(sh.size(), [&](std::int64_t k) { return x[k]; });
    p.ymin = minval_nan<float>(sh.size(), [&](std::int64_t k) { return y[k]; });
    p.ymax = maxval_nan<float>(sh.size(), [&](std::int64_t k) { return y[k]; });
  } else {
    const double* x = a.xg8;
    const double* y = a.yg8;
    p.xmin = minval_nan<double>(sh.size(), [&](std::int64_t k) { return x[k]; });
    p.xmax = maxval_nan<double>(sh.size(), [&](std::int64_t k) { return x[k]; });
    p.ymin = minval_nan<double>(sh.size(), [&](std::int64_t k) { return y[k]; });
    p.ymax = maxval_nan<double>(sh.size(), [&](std::int64_t k) { return y[k]; });
  }
  if (p.lclo) {
    p.xmin = kZero;
    p.xmax = kD360;
  }
  if (lspl) p.ymin = -kD90;
  if (lnpl) p.ymax = kD90;
  p.l360 = p.xmin >= kZero;

  if (lbbox) {
    r.gsu = std::move(pp);
    return r;
  }

  if (a.ncb != nullptr) {
    p.nbx = std::max(1, nx / *a.ncb);
    p.nby = std::max(1, ny / *a.ncb);
  } else {
    p.nbx = std::max(1, nx / kNcbDefault);
    p.nby = std::max(1, ny / kNcbDefault);
  }
  p.dxb = (p.xmax - p.xmin) / static_cast<double>(static_cast<float>(p.nbx));
  p.dyb = (p.ymax - p.ymin) / static_cast<double>(static_cast<float>(p.nby));

  if (ldbg) {
    r.out += "\nW3GSUC - ICLO,LCLO,GKIND: " + fi(p.iclo, 2) + fl(p.lclo, 2) + fi(p.gkind, 2) + "\n";
    const double dom[4] = {p.xmin, p.ymin, p.xmax, p.ymax};
    r.out += rec_a_e4("W3GSUC - grid search domain:", dom);
    const int nb[2] = {p.nbx, p.nby};
    r.out += rec_a_ints("W3GSUC - number of search buckets:", nb, 2);
    r.out += "W3GSUC - search bucket size:" + fe(p.dxb, 24, 16) + fe(p.dyb, 24, 16) + "\n";
  }

  try {
    p.b.resize(static_cast<std::size_t>(p.nby) * static_cast<std::size_t>(p.nbx));
  } catch (const std::bad_alloc&) {
    r.err += std::string("\n") + kErr + "search bucket array allocation failed\n\n";
    r.exit_code = kStatAllocation;
    return r;
  }

  // The locals NS, IB1, IB2, JB1, JB2 keep their values from cell to cell and across ISTEP.
  int ns = 0;
  int ib1[2] = {0, 0}, ib2[2] = {0, 0}, jb1[2] = {0, 0}, jb2[2] = {0, 0};
  bool jb_defined = false;
  int ibc[4], jbc[4];

  for (int istep = 1; istep <= 2; ++istep) {
    if (istep == 2) {
      for (int ib = 1; ib <= p.nbx; ++ib) {
        for (int jb = 1; jb <= p.nby; ++jb) {
          Bkt& bk = p.bkt(jb, ib);
          bk.i.clear();
          bk.j.clear();
          if (bk.n > 0) {
            const char* which = "i";
            try {
              bk.i.assign(static_cast<std::size_t>(bk.n), 0);
              which = "j";
              bk.j.assign(static_cast<std::size_t>(bk.n), 0);
            } catch (const std::bad_alloc&) {
              r.err += std::string("\n") + kErr + "search bucket cell-" + which +
                       " list allocation failed -- bucket: " + fi(ib, 6) + fi(jb, 6) + fi(n, 6) +
                       "\n\n";
              r.exit_code = kStatAllocation;
              return r;
            }
          }
        }
      }
    }

    for (Bkt& bk : p.b) bk.n = 0;
    for (int i = lxc; i <= uxc; ++i) {
      for (int j = lyc; j <= uyc; ++j) {
        if (iclo == kIcloTrpl) {
          if (j == uyc && i > lbx + nx / 2) continue;
        }
        cell_vertices(a, type_r4, sh, i, j, lbx, lby, ubx, uby, nx, ny, ic, jc, xc, yc);
        bool lpl = false;
        bool lbc = false;
        if (a.llg) {
          for (int l = 0; l < 4; ++l) xc[l] = std::fmod(xc[l], kD360);
          if (p.lclo || p.l360) {
            for (int l = 0; l < 4; ++l)
              if (xc[l] < kZero) xc[l] = xc[l] + kD360;
          } else {
            for (int l = 0; l < 4; ++l)
              if (xc[l] > kD180) xc[l] = xc[l] - kD360;
          }
          n = 0;
          for (int l = 0; l < 4; ++l) {
            const int k = (l + 1) % 4;
            if (std::fabs(xc[k] - xc[l]) > kD180) n = n + 1;
          }
          lbc = n > 1;
          int cnt = 0;
          for (int l = 0; l < 4; ++l)
            if (std::fabs(yc[l]) == kD90) ++cnt;
          lpl = n == 1 || cnt == 1;
        }
        for (int l = 0; l < 4; ++l) {
          ibc[l] = fortran_int((xc[l] - p.xmin) / p.dxb) + 1;
          if (!p.lclo) ibc[l] = std::min(ibc[l], p.nbx);
          jbc[l] = std::min(fortran_int((yc[l] - p.ymin) / p.dyb) + 1, p.nby);
        }
        const int jbmin = std::min(std::min(jbc[0], jbc[1]), std::min(jbc[2], jbc[3]));
        const int jbmax = std::max(std::max(jbc[0], jbc[1]), std::max(jbc[2], jbc[3]));
        if (lpl) {
          ns = 1;
          ib1[0] = 1;
          ib2[0] = p.nbx;
          if (minval4(yc) > kZero) {
            jb1[0] = std::max(1, jbmin);
            jb2[0] = p.nby;
            jb_defined = true;
          }
          if (maxval4(yc) < kZero) {
            jb1[0] = 1;
            jb2[0] = std::min(p.nby, jbmax);
            jb_defined = true;
          }
          if (!jb_defined) {
            r.exit_code = kExitUndefinedBounds;
            return r;
          }
          ib1[1] = 0;
          ib2[1] = 0;
          jb1[1] = 0;
          jb2[1] = 0;
        } else if (lbc) {
          ns = 2;
          ib1[0] = p.nbx;
          ib2[0] = p.nbx;
          ib1[1] = 1;
          ib2[1] = 1;
          for (int l = 0; l < 4; ++l) {
            if (ibc[l] > p.nbx / 2) {
              ib1[0] = std::min(ib1[0], ibc[l]);
            } else {
              ib2[1] = std::max(ib2[1], ibc[l]);
            }
          }
          jb1[0] = jb1[1] = std::max(1, jbmin);
          jb2[0] = jb2[1] = std::min(p.nby, jbmax);
          jb_defined = true;
        } else {
          ns = 1;
          const int ibmin = std::min(std::min(ibc[0], ibc[1]), std::min(ibc[2], ibc[3]));
          const int ibmax = std::max(std::max(ibc[0], ibc[1]), std::max(ibc[2], ibc[3]));
          ib1[0] = std::max(1, ibmin);
          ib2[0] = std::min(p.nbx, ibmax);
          jb1[0] = std::max(1, jbmin);
          jb2[0] = std::min(p.nby, jbmax);
          jb_defined = true;
          ib1[1] = 0;
          ib2[1] = 0;
          jb1[1] = 0;
          jb2[1] = 0;
        }
        if (ldbg && istep == 1) {
          const int ij[2] = {i, j};
          r.out += "\n" + rec_a_ints("W3GSUC -- BUCKET SORT:", ij, 2);
          r.out += "W3GSUC -- LBC,LPL:" + fl(lbc, 6) + fl(lpl, 6) + "\n";
          r.out += rec_a_ints("W3GSUC -- IC:", ic, 4);
          r.out += rec_a_ints("W3GSUC -- JC:", jc, 4);
          r.out += rec_a_e4("W3GSUC -- XC:", xc);
          r.out += rec_a_e4("W3GSUC -- YC:", yc);
          r.out += rec_a_ints("W3GSUC -- IBC:", ibc, 4);
          r.out += rec_a_ints("W3GSUC -- JBC:", jbc, 4);
          r.out += rec_a_ints("W3GSUC -- NS:", &ns, 1);
          r.out += rec_a_ints("W3GSUC -- IB1:", ib1, 2);
          r.out += rec_a_ints("W3GSUC -- JB1:", jb1, 2);
          r.out += rec_a_ints("W3GSUC -- IB2:", ib2, 2);
          r.out += rec_a_ints("W3GSUC -- JB2:", jb2, 2);
        }
        for (int k = 0; k < ns; ++k) {
          for (int ib = ib1[k]; ib <= ib2[k]; ++ib) {
            for (int jb = jb1[k]; jb <= jb2[k]; ++jb) {
              if (ib < 1 || ib > p.nbx || jb < 1 || jb > p.nby) {
                r.exit_code = kExitBucketBounds;
                return r;
              }
              Bkt& bk = p.bkt(jb, ib);
              bk.n = bk.n + 1;
              if (istep == 2) {
                if (bk.n > static_cast<int>(bk.i.size())) {
                  r.exit_code = kExitBucketBounds;
                  return r;
                }
                bk.i[static_cast<std::size_t>(bk.n - 1)] = ic[0];
                bk.j[static_cast<std::size_t>(bk.n - 1)] = jc[0];
              }
            }
          }
        }
      }
    }
  }

  if (h.w3nnsc == nullptr) {
    r.exit_code = kExitMissingHook;
    return r;
  }
  p.nnb = std::make_unique<Nns>();
  {
    const double half_max = static_cast<double>(std::max(p.nby, p.nbx)) * kHalf;
    const int rc = h.w3nnsc(h.ctx, static_cast<int>(std::lround(half_max)), *p.nnb);
    if (rc != 0) {
      r.exit_code = rc;
      return r;
    }
  }

  if (ldbg) {
    r.out += "\nW3GSUC - search bucket list:\n";
    r.out += fa("I", 6) + fa("J", 6) + fa("N", 6) + fa("X1", 14) + fa("Y1", 14) + fa("X2", 14) +
             fa("Y2", 14) + "\n";
    for (int ib = 1; ib <= p.nbx; ++ib) {
      for (int jb = 1; jb <= p.nby; ++jb) {
        r.out += fi(ib, 6) + fi(jb, 6) + fi(p.bkt(jb, ib).n, 6) +
                 fe(p.xmin + static_cast<double>(ib - 1) * p.dxb, 24, 16) +
                 fe(p.ymin + static_cast<double>(jb - 1) * p.dyb, 24, 16) +
                 fe(p.xmin + static_cast<double>(ib - 0) * p.dxb, 24, 16) +
                 fe(p.ymin + static_cast<double>(jb - 0) * p.dyb, 24, 16) + "\n";
      }
    }
  }

  // 5. Set return parameter (:7847)
  r.gsu = std::move(pp);
  return r;
}

}  // namespace ww::gsru
