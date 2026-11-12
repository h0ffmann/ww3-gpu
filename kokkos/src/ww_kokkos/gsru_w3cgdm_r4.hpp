// kokkos/src/ww_kokkos/gsru_w3cgdm_r4.hpp
// W3CGDM_R4: the metric of a curvilinear grid at every output point. From the derivatives of
// the physical coordinates (x,y) with respect to the index coordinates (p,q), computed by finite
// differences of order NFD (falling back to NFD = 2 where NFD > 2 gives a left-handed cell), it
// returns the covariant metric gpp, gqq, gpq, the Jacobian sqrt(g), the scale factors hp, hq,
// the contravariant metric, the inverse derivatives dp/dx .. dq/dy, cos(alpha), and the local
// grid rotation (cos, sin and angle). Host-side, no View is created and no kernel runs. Phase 1,
// translated, not regtested.
// WW3 heritage: SUBROUTINE W3CGDM_R4, WW3/model/src/w3gsrumd.F90:4014-4363 at h0ffmann/WW3
// 96892c95 (byte-identical to NOAA-EMC/WW3 03adda91: git -C WW3 diff 03adda91 HEAD --
// model/src/w3gsrumd.F90 is empty), with GET_FDW3 (:9582-9604) and W3FDWT_R8 (:6706-6747),
// translated here; author T. J. Campbell (NRL), w3gsrumd.F90:21. Called by W3GNTX through
// W3CGDM (w3gdatmd.F90:3050, :3065, the curvilinear metric of the model grid). Evidence:
// kokkos/port_report/W3CGDM_R4_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement switches/switch_lab_shrd compiles. Not compiled and not ported: W3_S
// (STRACE, :4074-4077). DXYDP (:8410-8687) and DXYDQ (:8693-8982), the finite-difference
// stencils with their masks, index closure and the polar stereographic projection (W3SPLX,
// SPDDP, SPDDQ), are not translated: they are the hooks CgdmHooks::dxydp and ::dxydq, called
// where the Fortran calls them (:4214, :4230, :4259, :4275) with RC present; an empty hook ends
// with kExitMissingHook. The error WRITEs to unit 0 are text appended to CgdmResult::err; EXTCDE
// (W3SERVMD's, w3servmd.F90:1064-1211, ends in CALL EXIT) is returned as exit_code when RC is
// absent.
//
// Arithmetic, as gfortran 16.2 -O3 compiles it (WW3's Release flags, -O3 -g, no -ffp-contract;
// the object's disassembly equals the CMake build's), read from the GIMPLE
// (-fdump-tree-optimized-lineno) and the assembly (v, 2026-11-12):
//  - FACY = R*D2R (:4141); FACX = FACY*cos(REAL(Y(I1,I2),8)*D2R), a libm cos call (:4243,
//    :4288); D2R and R2D are the folded doubles of PI/180D0 and 1D0/D2R, REARTH = 4.D7/PI2
//    (:377-398, COAMPS not defined).
//  - GSQRL = DXDPL*DYDQL - DXDQL*DYDPL (:4249, :4294); GPPCL = DXDPL*DXDPL + DYDPL*DYDPL and
//    the other sums of products as written (:4312-4314, :4322-4324).
//  - MAX(x,SMALL) is maxsd with x first (:4315-4317): a NaN or a tie gives SMALL = 1D-15
//    (the literal 1.00000000000000007770539987666107923830718560119501514549e-15). fmax_d().
//  - DPDYL = -(DXDQL/GSQRL), DQDXL = -(DYDPL/GSQRL), the negation after the division
//    (:4319-4320).
//  - SQRT is sqrtsd (:4325-4326); GSQRL**2 is GSQRL*GSQRL (:4328); ATAN2 is a libm atan2 call,
//    times R2D (:4333); each output is a cvtsd2ss of its REAL(8) local (:4334-4354).
//  - W3FDWT_R8: C(I,K) = (C1*(K*C(I-1,K-1) - C5*C(I-1,K)))/C2 (:6735, vectorised over K,
//    elementwise, same bits), C(I,0) = -(((C1*C5)*C(I-1,0))/C2) (:6737),
//    C(J,K) = (C4*C(J,K) - K*C(J,K-1))/C3 (:6740), C(J,0) = (C4*C(J,0))/C3 (:6742).
//  - No FMA (vfmadd: 0 in the object).
//  - ⚠ Which NaN comes out when both operands of a commutative add or multiply are NaN is the
//    operand order GCC picks from its SSA numbering, which the Fortran and C++ front ends number
//    differently. Not reproduced in general: on a differential run against the Fortran object
//    with NaN latitudes, 41 of 1 056 510 output values (all APQC) differ only in the sign of a
//    NaN at -O3, and 234 (APPC, AQQC, APQC) at -O0; reordering the C++ terms made it worse, so
//    the source order is kept. Every other value matched bit for bit (v, 2026-11-12). The count
//    moves with register allocation: after the review's change to the allocation of K and C
//    (cgdm_detail::alloc_fd), which touches no arithmetic, it is 43 at -O3 (v, 2026-11-12); the
//    Fortran's own GIMPLE has two copies of :4312-4333 that order the same products differently
//    (HQFCL*HPFCL in one, HPFCL*HQFCL in the other, :4327): the order is GCC's, not the source's.
//
// Undefined in the Fortran and kept as zero here: the entries of K(0:N,0:N,1:N) and
// C(0:N,0:N,1:N) with J > L or I > L, which GET_FDW3 never sets (:9592-9601) ⚠.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <new>
#include <string>
#include <vector>

#include "gsru_gsu_create.hpp"
#include "gsru_w3gfcl_r8.hpp"

namespace ww::gsru {

inline constexpr int kNfdDefault = 4;               // :413
inline constexpr double kSmall = 1.0e-15;           // :4062
/// ALLOCATE's STAT= when the size computation overflows (the object's constant, :4172).
inline constexpr int kStatAllocationSize = 5014;
inline constexpr double kPi2 = 2.0 * kPi8;          // :378
inline constexpr double kRearth = 4.0e7 / kPi2;     // :396

/// K(0:N,0:N,1:N) and C(0:N,0:N,1:N) of GET_FDW3, column-major: element (j,i,l) is
/// [j + (n+1)*i + (n+1)^2*(l-1)].
struct FdTables {
  int n = 0;
  std::vector<int> k;
  std::vector<double> c;
  std::size_t at(int j, int i, int l) const {
    const auto s = static_cast<std::size_t>(n) + 1;
    return static_cast<std::size_t>(j) + s * static_cast<std::size_t>(i) +
           s * s * static_cast<std::size_t>(l - 1);
  }
};

/// W3FDWT_R8(N, ND, M, Z, X, C) (:6706-6747): Fornberg's finite-difference weights for the
/// nodes X(0:N) at Z, derivatives 0..M. x has ND+1 elements, c is C(0:ND,0:M), column-major.
inline void w3fdwt_r8(int n, int nd, int m, double z, const double* x, double* c) {
  const auto ld = static_cast<std::size_t>(nd) + 1;
  auto cc = [&](int j, int k) -> double& {
    return c[static_cast<std::size_t>(j) + ld * static_cast<std::size_t>(k)];
  };
  double c1 = kOne;
  double c4 = x[0] - z;
  for (std::size_t q = 0; q < ld * (static_cast<std::size_t>(m) + 1); ++q) c[q] = kZero;
  cc(0, 0) = kOne;
  for (int i = 1; i <= n; ++i) {
    const int mn = std::min(i, m);
    double c2 = kOne;
    const double c5 = c4;
    c4 = x[i] - z;
    for (int j = 0; j <= i - 1; ++j) {
      const double c3 = x[i] - x[j];
      c2 = c2 * c3;
      if (j == i - 1) {
        for (int k = mn; k >= 1; --k)
          cc(i, k) = c1 * (static_cast<double>(k) * cc(i - 1, k - 1) - c5 * cc(i - 1, k)) / c2;
        cc(i, 0) = -(c1 * c5 * cc(i - 1, 0) / c2);
      }
      for (int k = mn; k >= 1; --k)
        cc(j, k) = (c4 * cc(j, k) - static_cast<double>(k) * cc(j, k - 1)) / c3;
      cc(j, 0) = c4 * cc(j, 0) / c3;
    }
    c1 = c2;
  }
}

/// GET_FDW3(N, M, K, C) (:9582-9604) into t, whose vectors the caller has sized.
inline void get_fdw3(int n, int m, FdTables& t) {
  std::vector<double> a(static_cast<std::size_t>(n) + 1, 0.0);
  std::vector<double> b((static_cast<std::size_t>(n) + 1) * (static_cast<std::size_t>(m) + 1),
                        0.0);
  const auto ld = static_cast<std::size_t>(n) + 1;
  for (int l = 1; l <= n; ++l) {
    for (int i = 0; i <= l; ++i) {
      for (int j = 0; j <= l; ++j) {
        t.k[t.at(j, i, l)] = j - i;
        a[static_cast<std::size_t>(j)] = static_cast<double>(t.k[t.at(j, i, l)]);
      }
      w3fdwt_r8(l, n, m, kZero, a.data(), b.data());
      for (int j = 0; j <= l; ++j)
        t.c[t.at(j, i, l)] = b[static_cast<std::size_t>(j) + ld * static_cast<std::size_t>(m)];
    }
  }
}

/// The arguments of W3CGDM_R4 (:4014-4058). A null pointer is an absent OPTIONAL. x, y and mask
/// are X(LBI(1):UBI(1),LBI(2):UBI(2)), the outputs (LBO(1):UBO(1),LBO(2):UBO(2)), column-major.
/// mask is LOGICAL(4), passed on to the hooks only.
struct CgdmArgs {
  bool ijg = true, llg = false;
  int iclo = kIcloNone;
  bool ptiled = false, qtiled = false;
  int prange[2] = {1, 0}, qrange[2] = {1, 0};
  int lbi[2] = {1, 1}, ubi[2] = {0, 0}, lbo[2] = {1, 1}, ubo[2] = {0, 0};
  const float* x = nullptr;
  const float* y = nullptr;
  const std::int32_t* mask = nullptr;
  const int* nfd = nullptr;
  const bool* sphere = nullptr;
  const float* radius = nullptr;
  const float* dx = nullptr;
  const float* dy = nullptr;
  float *gppc = nullptr, *gqqc = nullptr, *gpqc = nullptr, *gsqr = nullptr;
  float *hpfc = nullptr, *hqfc = nullptr, *appc = nullptr, *aqqc = nullptr, *apqc = nullptr;
  float *dxdp = nullptr, *dydp = nullptr, *dxdq = nullptr, *dydq = nullptr;
  float *dpdx = nullptr, *dpdy = nullptr, *dqdx = nullptr, *dqdy = nullptr;
  float *cosa = nullptr, *cosc = nullptr, *sinc = nullptr, *angl = nullptr;
  int* rc = nullptr;
};

/// DXYDP / DXYDQ(N, K, C, IJG, LLG, ICLO, PTILED, QTILED, PRANGE, QRANGE, LBI, UBI, P, Q,
/// D1, D2, MASK=MASK, X4=X, Y4=Y, RC=ISTAT), not translated: the caller sets d1, d2, appends
/// its unit-0 records to err and returns RC (0, or 1 on its errors).
using CgdmDxyd = int (*)(void* ctx, int n, const FdTables& t, const CgdmArgs& a, int p, int q,
                         double& d1, double& d2, std::string& err);
struct CgdmHooks {
  void* ctx = nullptr;
  CgdmDxyd dxydp = nullptr;
  CgdmDxyd dxydq = nullptr;
};

struct CgdmResult {
  int exit_code = 0;  ///< EXTCDE's code when RC is absent
  std::string err;    ///< records written to unit 0
};

namespace cgdm_detail {
/// MAX(x,SMALL) as the object's maxsd computes it: a NaN or a tie gives SMALL.
inline double fmax_d(double x, double s) { return (x > s) ? x : s; }
/// ALLOCATE(K(0:N,0:N,1:N), C(0:N,0:N,1:N), STAT=ISTAT) (:4172) as the object does it: an
/// element count N*(N+1)**2 that overflows INTEGER(8), or whose byte size does, gives 5014;
/// a failed malloc gives 5020 (kStatAllocation). Returns 0 or that STAT. N > 0.
inline int alloc_fd(int n, FdTables& t) {
  constexpr long long kMax64 = 9223372036854775807LL;
  const long long s = static_cast<long long>(n) + 1;
  if (kMax64 / n < s * s || static_cast<long long>(n) * s * s > kMax64 / 2)
    return kStatAllocationSize;
  const auto ne = static_cast<std::size_t>(n) * static_cast<std::size_t>(s * s);
  t.n = n;
  try {
    t.k.assign(ne, 0);
  } catch (const std::exception&) {  // bad_alloc, or length_error above max_size()
    return kStatAllocation;
  }
  if (static_cast<long long>(n) * s * s > kMax64 / 4) return kStatAllocationSize;
  try {
    t.c.assign(ne, 0.0);
  } catch (const std::exception&) {
    return kStatAllocation;
  }
  return 0;
}
}  // namespace cgdm_detail

/// W3CGDM_R4 (:4014-4363), statement for statement.
inline CgdmResult w3cgdm_r4(const CgdmArgs& a, const CgdmHooks& h) {
  using cgdm_detail::fmax_d;
  using gsu_create_detail::fa;
  using gsu_create_detail::fe;
  using gsu_create_detail::fi;
  CgdmResult r;
  const char* const kErr = "W3CGDM ERROR -- ";
  int istat = 0;
  // RC = ISTAT; RETURN when RC is present, else CALL EXTCDE(ISTAT) (:4092-4097 and others).
  auto fail = [&](int code) {
    if (a.rc != nullptr) {
      *a.rc = code;
    } else {
      r.exit_code = code;
    }
    return r;
  };

  // 1. Check and setup inputs (:4081-4167)
  if (a.rc != nullptr) *a.rc = 0;
  const int n = a.nfd != nullptr ? *a.nfd : kNfdDefault;
  if (n <= 0 || n % 2 != 0) {
    r.err += std::string("\n") + kErr + "NFD must be even and greater than zero\n\n";
    return fail(1);
  }
  const int np = a.prange[1] - a.prange[0] + 1;
  switch (a.iclo) {
    case kIcloNone:
    case kIcloGrdi:
    case kIcloGrdj:
    case kIcloTrdl:
    case kIcloTrpl:
      break;
    default:
      r.err += std::string("\n") + kErr + "unsupported ICLO: " + fi(a.iclo, 2) + "\n\n";
      return fail(1);
  }
  if (a.iclo == kIcloTrpl && np % 2 != 0) {
    r.err += std::string("\n") + kErr + "tripole grid closure requires NP even\n\n";
    return fail(1);
  }
  const bool sphr = a.sphere != nullptr ? *a.sphere : true;
  const double rad = a.radius != nullptr ? static_cast<double>(*a.radius) : kRearth;
  const double facy = rad * kD2R;
  if (a.dx != nullptr && static_cast<double>(*a.dx) <= kZero) {
    r.err += std::string("\n") + kErr + "DX must be > 0\n\n";
    return fail(1);
  }
  if (a.dy != nullptr && static_cast<double>(*a.dy) <= kZero) {
    r.err += std::string("\n") + kErr + "DY must be > 0\n\n";
    return fail(1);
  }

  // 2. Setup finite difference coefficients (:4172-4196)
  FdTables t, t2;
  if (const int stat = cgdm_detail::alloc_fd(n, t); stat != 0) {
    r.err += std::string("\n") + kErr + "finite difference coeff allocation failed\n\n";
    return fail(stat);
  }
  get_fdw3(n, 1, t);
  t2.n = 2;
  t2.k.assign(18, 0);
  t2.c.assign(18, 0.0);
  get_fdw3(2, 1, t2);

  // A hook the loop would call and that is empty stops before the first point.
  const bool any_point = a.lbo[0] <= a.ubo[0] && a.lbo[1] <= a.ubo[1];
  if (any_point && ((a.dx == nullptr && h.dxydp == nullptr) ||
                    (a.dy == nullptr && h.dxydq == nullptr))) {
    r.exit_code = kExitMissingHook;
    return r;
  }

  // 3. Compute optional return quantities (:4201-4356)
  const gsu_create_detail::Shape shi{a.lbi[0], a.ubi[0], a.lbi[1], a.ubi[1]};
  const gsu_create_detail::Shape sho{a.lbo[0], a.ubo[0], a.lbo[1], a.ubo[1]};
  for (int i2 = a.lbo[1]; i2 <= a.ubo[1]; ++i2) {
    for (int i1 = a.lbo[0]; i1 <= a.ubo[0]; ++i1) {
      const int p = a.ijg ? i1 : i2;
      const int q = a.ijg ? i2 : i1;
      double dxdpl = 0, dydpl = 0, dxdql = 0, dydql = 0;
      auto derivs = [&](int nn, const FdTables& tt) -> bool {
        if (a.dx != nullptr) {
          dxdpl = static_cast<double>(*a.dx);
          dydpl = kZero;
        } else {
          istat = h.dxydp(h.ctx, nn, tt, a, p, q, dxdpl, dydpl, r.err);
          if (istat != 0) return false;
        }
        if (a.dy != nullptr) {
          dxdql = kZero;
          dydql = static_cast<double>(*a.dy);
        } else {
          istat = h.dxydq(h.ctx, nn, tt, a, p, q, dxdql, dydql, r.err);
          if (istat != 0) return false;
        }
        if (a.llg && sphr) {
          const double facx = facy * std::cos(static_cast<double>(a.y[shi.at(i1, i2)]) * kD2R);
          dxdpl = dxdpl * facx;
          dydpl = dydpl * facy;
          dxdql = dxdql * facx;
          dydql = dydql * facy;
        }
        return true;
      };
      if (!derivs(n, t)) return fail(istat);
      double gsqrl = dxdpl * dydql - dxdql * dydpl;
      if (gsqrl < kZero && n > 2) {
        if (!derivs(2, t2)) return fail(istat);
        gsqrl = dxdpl * dydql - dxdql * dydpl;
      }
      if (gsqrl < kZero) {
        istat = 1;
        r.err += std::string("\n") + kErr +
                 "input coordinates do not define a right-handed coordinate system\n";
        r.err += "W3CGDM ERROR --" + fa("P", 6) + fa("Q", 6) + fa("GSQRL", 16) +
                 fa("DXDPL", 16) + fa("DYDQL", 16) + fa("DXDQL", 16) + fa("DYDPL", 16) + "\n";
        r.err += "W3CGDM ERROR --" + fi(p, 6) + fi(q, 6) + fe(gsqrl, 16, 8) + fe(dxdpl, 16, 8) +
                 fe(dydql, 16, 8) + fe(dxdql, 16, 8) + fe(dydpl, 16, 8) + "\n\n";
        return fail(istat);
      }
      double gppcl = dxdpl * dxdpl + dydpl * dydpl;
      double gqqcl = dxdql * dxdql + dydql * dydql;
      const double gpqcl = dxdpl * dxdql + dydpl * dydql;
      gsqrl = fmax_d(gsqrl, kSmall);
      gppcl = fmax_d(gppcl, kSmall);
      gqqcl = fmax_d(gqqcl, kSmall);
      const double dpdxl = dydql / gsqrl;
      const double dpdyl = -(dxdql / gsqrl);
      const double dqdxl = -(dydpl / gsqrl);
      const double dqdyl = dxdpl / gsqrl;
      const double appcl = dpdxl * dpdxl + dpdyl * dpdyl;
      const double aqqcl = dqdxl * dqdxl + dqdyl * dqdyl;
      const double apqcl = dpdxl * dqdxl + dpdyl * dqdyl;
      const double hpfcl = std::sqrt(gppcl);
      const double hqfcl = std::sqrt(gqqcl);
      const double cosal = gpqcl / (hpfcl * hqfcl);
      const double sinal = gsqrl * gsqrl / (gppcl * gqqcl);
      const double costp = dxdpl / hpfcl;
      const double sintp = dydpl / hqfcl;
      const double coscl = sinal * costp + cosal * sintp;
      const double sincl = sinal * sintp - cosal * costp;
      const double angll = std::atan2(sincl, coscl) * kR2D;
      const std::int64_t o = sho.at(i1, i2);
      auto put = [o](float* f, double v) {
        if (f != nullptr) f[o] = static_cast<float>(v);
      };
      put(a.gppc, gppcl);
      put(a.gqqc, gqqcl);
      put(a.gpqc, gpqcl);
      put(a.appc, appcl);
      put(a.aqqc, aqqcl);
      put(a.apqc, apqcl);
      put(a.gsqr, gsqrl);
      put(a.hpfc, hpfcl);
      put(a.hqfc, hqfcl);
      put(a.dxdp, dxdpl);
      put(a.dydp, dydpl);
      put(a.dxdq, dxdql);
      put(a.dydq, dydql);
      put(a.dpdx, dpdxl);
      put(a.dpdy, dpdyl);
      put(a.dqdx, dqdxl);
      put(a.dqdy, dqdyl);
      put(a.cosa, cosal);
      put(a.cosc, coscl);
      put(a.sinc, sincl);
      put(a.angl, angll);
    }
  }
  return r;
}

}  // namespace ww::gsru
