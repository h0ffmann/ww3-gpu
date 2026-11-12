// kokkos/src/ww_kokkos/cspc_w3cspc.hpp
// W3CSPC: convert a set of NSP spectra from one discrete (frequency, direction) grid to another
// by conservative redistribution of the discrete energies, then back to densities, with a tail
// FTL*SP2(:,J-1,:) above the last input frequency. The redistribution tables of each pair of
// grids are kept in a chain and reused (module variables NCASES, IDATA). Host-side, no View is
// created and no kernel runs. Phase 1, translated, not regtested.
// WW3 heritage: SUBROUTINE W3CSPC, WW3/model/src/w3cspcmd.F90:143-603 at h0ffmann/WW3 96892c95
// (byte-identical to NOAA-EMC/WW3 03adda91: git -C WW3 diff 03adda91 HEAD --
// model/src/w3cspcmd.F90 is empty), with TYPE CASE and the chain NCASES/IDATA (:104-114);
// author H. L. Tolman (:4). Called by W3IOBC (w3iobcmd.F90:738), WMINIO, WMIOPO, ww3_bounc.
// Evidence: kokkos/port_report/W3CSPC_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement switches/switch_lab_shrd compiles. Not compiled and not ported: W3_S
// (STRACE, :228-230, :246-248, :261-263) and the test output W3_T, W3_T1, W3_T2 (:287-290,
// :305-310, :327-329, :349-351, :500-511, :517-519, :540-542, :553-555). The WRITEs to NDSE
// (:271, :276, :281) are text appended to CspcResult::ndse; EXTCDE (:272, :277) is returned as
// the code it would exit with (EXTCDE ends in CALL EXIT, w3servmd.F90:1207).
//
// Arithmetic, as gfortran 16.2 -O3 compiles it (WW3's Release flags, -O3 -g, no -ffp-contract;
// the object's disassembly equals the CMake build's), read from the GIMPLE
// (-fdump-tree-optimized-lineno) and the assembly (v, 2026-11-12):
//  - TPI is the REAL literal 6.283185482025146484375 (constants.F90:73); DTH = TPI/REAL(NTH)
//    in REAL (:380, :382); INT(DTH1/DTH2) is a 32-bit cvttss2si (:387).
//  - LOW = (TH1 + REAL(I-1)*DTH1) - 0.5*DTH1, RLOW = (LOW-TH2)/DTH2 + 1 (:397-400); RHGH is
//    dead and not computed. NINT(RLOW) is a call of lroundf, of which the low 32 bits are kept
//    (:401): a NaN gives 0, not INT_MIN.
//  - MIN(BHGH,HGH) is minss with BHGH first, MAX(BLOW,LOW) is maxss with BLOW first (:404,
//    :472): a NaN or a tie gives HGH and LOW. fmin_f() and fmax_f() compute that.
//  - The frequency edges are 0.5*(F(I)+F(I+1)) (:456, :464, :484), 0.5*(F(I)+F(I-1)) (:459,
//    :467, :486), LOW+XDF*F(I) (:460, :468, :487), HGH-XDF*F(I) (:457, :465).
//  - XDF = 0.5*(XF - 1./XF) (:430, :432, vectorised as one divps, same bits).
//  - FRAC = (((R2*FRQ1(I2))*XDF1)*R1)*DTH1 (:531), SP2 + FRAC*SP1 (:532), FACT =
//    1./((FRQ2(J2)*XDF2)*DTH2) (:546), FACT*SP2 (:547), FTL*SP2(:,J2-1,:) (:558, vectorised
//    elementwise, same bits). No FMA (vfmadd: 0 in the object).
//  - The comparisons are the source's: a NaN FRAC fails .GT.1.E-5 (:405, not stored) and fails
//    .LT.1.E-5 (:473, stored); BHGH.LE.LOW and BLOW.GE.HGH are false for a NaN (:470-471).
//
// Where the Fortran has undefined behaviour the C++ stops with a code that is not a WW3 code:
//  - kExitNrmax: the loop :440-445 never ends when AUX1 cannot become negative (XF1 or XF2 NaN
//    or +Inf, or XDF1 above XDF2*XF2/(XF2-1)); NRMAX overflows. The C++ stops when NRMAX would
//    pass INT_MAX.
//  - kExitTableBounds: IDFR(0,I) above NRMAX (:474-476) writes outside IDFR/RDFR, and an index
//    1+MOD(J-1+NTH2,NTH2) outside 1..NTH2 (J-1+NTH2 negative, :407) or an ALLOCATE of a
//    negative extent would read or write outside SP2. The index of :407 is stored as the
//    Fortran stores it and checked only where 3.a reads it (:532): a table whose IDFR is empty
//    never indexes SP2 with it, and the Fortran then returns zero spectra.
#pragma once

#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "real.hpp"

namespace ww::cspc {

/// TPI = 2.0*PI in REAL, constants.F90:72-73 (GIMPLE literal 6.283185482025146484375).
inline constexpr float kCspcTpi = 2.0F * ww::kPi;

/// ISTAT of a failed ALLOCATE without STAT=: the Fortran stops in the runtime; not a WW3 code.
inline constexpr int kExitAllocation = -140;
inline constexpr int kExitNrmax = -141;        // :440-445 cannot end (see the header)
inline constexpr int kExitTableBounds = -142;  // a write or read outside IDFR/RDFR or SP2

/// TYPE CASE (:104-111). IDTH(0:NRTH,NTH1), RDTH(NRTH,NTH1), IDFR(0:NRFR,NFR1),
/// RDFR(NRFR,NFR1), column-major: idth[l + (i-1)*(nrth+1)], rdth[(l-1) + (i-1)*nrth].
struct CspcCase {
  int icase = 0, nfr1 = 0, nth1 = 0, nfr2 = 0, nth2 = 0, nfr2t = 0;
  float xf1 = 0, fr1 = 0, th1 = 0, xf2 = 0, fr2 = 0, th2 = 0;
  float dth1 = 0, dth2 = 0, xdf1 = 0, xdf2 = 0;
  int nrth = 0, nrfr = 0;  ///< the two NRMAX of :384-388 and :434-446
  std::vector<int> idth, idfr;
  std::vector<float> rdth, frq1, frq2, rdfr;

  int& id_th(int l, int i) { return idth[ix0(l, i, nrth)]; }
  float& rd_th(int l, int i) { return rdth[ix1(l, i, nrth)]; }
  int& id_fr(int l, int i) { return idfr[ix0(l, i, nrfr)]; }
  float& rd_fr(int l, int i) { return rdfr[ix1(l, i, nrfr)]; }

  static std::size_t ix0(int l, int i, int nr) {
    return static_cast<std::size_t>(l) +
           static_cast<std::size_t>(i - 1) * (static_cast<std::size_t>(nr) + 1);
  }
  static std::size_t ix1(int l, int i, int nr) {
    return static_cast<std::size_t>(l - 1) +
           static_cast<std::size_t>(i - 1) * static_cast<std::size_t>(nr);
  }
};

/// The module variables NCASES and IDATA (:113-114): the chain in the order it was built. The
/// Fortran keeps one chain per process; a caller mirrors that with one CspcState.
struct CspcState {
  std::vector<std::unique_ptr<CspcCase>> cases;  ///< NCASES = cases.size()
};

/// exit_code is 0 or the EXTCDE code; ndse holds the records written to unit NDSE.
struct CspcResult {
  int exit_code = 0;
  std::string ndse;
};

namespace cspc_detail {

/// MIN(a,b) of two REALs as the object's minss computes it: a NaN or a tie gives b.
inline float fmin_f(float a, float b) { return (a < b) ? a : b; }
/// MAX(a,b) as maxss: a NaN or a tie gives b.
inline float fmax_f(float a, float b) { return (a > b) ? a : b; }

/// NINT of a REAL: lroundf, then the low 32 bits (C++20 conversion is modular).
inline int nint_f(float x) { return static_cast<int>(std::lroundf(x)); }

/// INT of a REAL as a 32-bit cvttss2si: INT_MIN for a NaN or out-of-range value.
inline int int_f(float x) {
  if (x > -2147483904.0F && x < 2147483648.0F) return static_cast<int>(x);
  return INT_MIN;
}

/// 32-bit wrap-around addition, as the object's lea/add compute it.
inline int wrap_add(int a, int b) {
  return static_cast<int>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

/// Iw: right-justified, all '*' when it does not fit.
inline std::string fi(int v, int w) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%d", v);
  const std::string s(buf);
  if (static_cast<int>(s.size()) > w) return std::string(static_cast<std::size_t>(w), '*');
  return std::string(static_cast<std::size_t>(w) - s.size(), ' ') + s;
}

/// Fw.d of a REAL, w > d+1 >= 2: the correctly rounded decimal of the binary value, the leading
/// zero dropped when only it does not fit; "Infinity", "-Infinity", "NaN" right-justified.
inline std::string ff(float x, int w, int d) {
  std::string s;
  if (std::isnan(x)) {
    s = "NaN";
  } else if (std::isinf(x)) {
    s = std::signbit(x) ? "-Infinity" : "Infinity";
  } else {
    char buf[96];
    std::snprintf(buf, sizeof buf, "%.*f", d, static_cast<double>(x));
    s = buf;
    if (static_cast<int>(s.size()) == w + 1) {
      if (s.rfind("0.", 0) == 0) s.erase(0, 1);
      else if (s.rfind("-0.", 0) == 0) s.erase(1, 1);
    }
  }
  if (static_cast<int>(s.size()) > w) return std::string(static_cast<std::size_t>(w), '*');
  return std::string(static_cast<std::size_t>(w) - s.size(), ' ') + s;
}

inline bool try_assign_i(std::vector<int>& v, long long n) {
  if (n < 0) return false;
  try {
    v.assign(static_cast<std::size_t>(n), 0);
  } catch (const std::bad_alloc&) {
    return false;
  }
  return true;
}
inline bool try_assign_f(std::vector<float>& v, long long n) {
  if (n < 0) return false;
  try {
    v.assign(static_cast<std::size_t>(n), 0.0F);
  } catch (const std::bad_alloc&) {
    return false;
  }
  return true;
}

/// 2.b (:348-494): a new CASE appended to the chain, tables computed. Returns 0 or a code.
inline int compute_case(CspcState& st, int nfr1, int nth1, float xf1, float fr1, float th1,
                        int nfr2, int nth2, float xf2, float fr2, float th2) {
  std::unique_ptr<CspcCase> cp;
  try {
    cp = std::make_unique<CspcCase>();
  } catch (const std::bad_alloc&) {
    return kExitAllocation;
  }
  CspcCase& c = *cp;
  c.icase = static_cast<int>(st.cases.size()) + 1;  // :348, :365
  c.nfr1 = nfr1;
  c.nth1 = nth1;
  c.xf1 = xf1;
  c.fr1 = fr1;
  c.th1 = th1;
  c.nfr2 = nfr2;
  c.nth2 = nth2;
  c.xf2 = xf2;
  c.fr2 = fr2;
  c.th2 = th2;

  // 2.b.3 Directional redistribution data (:379-411)
  c.dth1 = kCspcTpi / static_cast<float>(nth1);
  c.dth2 = kCspcTpi / static_cast<float>(nth2);
  const float dth1 = c.dth1;
  const float dth2 = c.dth2;
  int nrmax = 0;
  if (dth1 <= dth2) {
    nrmax = 2;
  } else {
    nrmax = wrap_add(2, int_f(dth1 / dth2));
  }
  c.nrth = nrmax;
  if (nrmax < 0) return kExitTableBounds;  // ALLOCATE(IDTH(0:NRMAX,..)) of a negative extent
  if (!try_assign_i(c.idth, (static_cast<long long>(nrmax) + 1) * nth1) ||
      !try_assign_f(c.rdth, static_cast<long long>(nrmax) * nth1))
    return kExitAllocation;

  for (int i = 1; i <= nth1; ++i) {
    const float low = (th1 + static_cast<float>(i - 1) * dth1) - 0.5F * dth1;
    const float hgh = low + dth1;
    const float rlow = 1.0F + (low - th2) / dth2;
    const int j0 = nint_f(rlow);
    // DO J=NINT(RLOW), NINT(RLOW)+NRMAX-1: the object skips the loop when the 32-bit
    // NINT(RLOW)+NRMAX is <= NINT(RLOW), then counts J-1 up NRMAX times.
    const int ntrip = wrap_add(j0, nrmax) <= j0 ? 0 : nrmax;
    for (int k = 0; k < ntrip; ++k) {
      const int jm1 = wrap_add(wrap_add(j0, -1), k);
      const float blow = (th2 + static_cast<float>(jm1) * dth2) - 0.5F * dth2;
      const float bhgh = blow + dth2;
      const float frac = (fmin_f(bhgh, hgh) - fmax_f(blow, low)) / (hgh - low);
      if (frac > 1.E-5F) {
        int& n0 = c.id_th(0, i);
        n0 = n0 + 1;
        c.id_th(n0, i) = 1 + wrap_add(jm1, nth2) % nth2;  // may be < 1: checked where read
        c.rd_th(n0, i) = frac;
      }
    }
  }

  // 2.b.4 Frequency redistribution data (:415-494)
  if (!try_assign_f(c.frq1, nfr1) || !try_assign_f(c.frq2, nfr2)) return kExitAllocation;
  c.frq1[0] = fr1;
  for (int i = 2; i <= nfr1; ++i) c.frq1[static_cast<std::size_t>(i - 1)] = xf1 * c.frq1[static_cast<std::size_t>(i - 2)];
  c.frq2[0] = fr2;
  for (int i = 2; i <= nfr2; ++i) c.frq2[static_cast<std::size_t>(i - 1)] = xf2 * c.frq2[static_cast<std::size_t>(i - 2)];
  auto f1 = [&](int i) { return c.frq1[static_cast<std::size_t>(i - 1)]; };
  auto f2 = [&](int j) { return c.frq2[static_cast<std::size_t>(j - 1)]; };

  c.xdf1 = 0.5F * (xf1 - 1.0F / xf1);
  c.xdf2 = 0.5F * (xf2 - 1.0F / xf2);
  const float xdf1 = c.xdf1;
  const float xdf2 = c.xdf2;
  if (xdf1 <= xdf2) {
    nrmax = 2;
  } else {
    nrmax = 1;
    float aux1 = xdf1;
    float aux2 = xdf2;
    for (;;) {
      if (nrmax == INT_MAX) return kExitNrmax;
      nrmax = nrmax + 1;
      aux1 = aux1 - aux2;
      aux2 = aux2 / xf2;
      if (aux1 < 0.0F) break;
    }
  }
  c.nrfr = nrmax;
  if (!try_assign_i(c.idfr, (static_cast<long long>(nrmax) + 1) * nfr1) ||
      !try_assign_f(c.rdfr, static_cast<long long>(nrmax) * nfr1))
    return kExitAllocation;

  float low = 0.0F;
  float hgh = 0.0F;
  float blow = 0.0F;
  float bhgh = 0.0F;
  for (int i = 1; i <= nfr1; ++i) {
    if (i == 1) {
      hgh = 0.5F * (f1(i) + f1(i + 1));
      low = hgh - xdf1 * f1(i);
    } else {
      low = 0.5F * (f1(i) + f1(i - 1));
      hgh = low + xdf1 * f1(i);
    }
    for (int j = 1; j <= nfr2; ++j) {
      if (j == 1) {
        bhgh = 0.5F * (f2(j) + f2(j + 1));
        blow = bhgh - xdf2 * f2(j);
      } else {
        blow = 0.5F * (f2(j) + f2(j - 1));
        bhgh = blow + xdf2 * f2(j);
      }
      if (bhgh <= low) continue;
      if (blow >= hgh) break;
      const float frac = (fmin_f(bhgh, hgh) - fmax_f(blow, low)) / (hgh - low);
      if (frac < 1.E-5F) continue;
      int& n0 = c.id_fr(0, i);
      n0 = n0 + 1;
      if (n0 > nrmax) return kExitTableBounds;
      c.id_fr(n0, i) = j;
      c.rd_fr(n0, i) = frac;
    }
  }

  // HGH is the last input bin's upper edge (:454-478, I = NFR1)
  c.nfr2t = nfr2 + 1;
  for (int j = nfr2; j >= 1; --j) {
    if (j == 1) {
      bhgh = 0.5F * (f2(j) + f2(j + 1));
    } else {
      blow = 0.5F * (f2(j) + f2(j - 1));
      bhgh = blow + xdf2 * f2(j);
    }
    if (bhgh > hgh) {
      c.nfr2t = j;
    } else {
      break;
    }
  }

  try {
    st.cases.push_back(std::move(cp));
  } catch (const std::bad_alloc&) {
    return kExitAllocation;
  }
  return 0;
}

}  // namespace cspc_detail

/// W3CSPC (:143-603). sp1 is SP1(NTH1,NFR1,NSP), sp2 is SP2(NTH2,NFR2,NSP), column-major. ndst
/// is only used by the test output, which the lab switch does not compile.
inline CspcResult cspc_w3cspc(CspcState& st, const float* sp1, int nfr1, int nth1, float xf1,
                              float fr1, float th1, float* sp2, int nfr2, int nth2, float xf2,
                              float fr2, float th2, int nsp, int ndst, int ndse, float ftl) {
  using namespace cspc_detail;
  (void)ndst;
  (void)ndse;
  CspcResult r;

  // 0.a Check input (:269-283)
  if (nfr1 < 3 || nth1 < 4 || xf1 <= 1.0F || fr1 <= 0.0F || nfr2 < 3 || nth2 < 4 ||
      xf2 <= 1.0F || fr2 <= 0.0F) {
    r.ndse += "\n *** ERROR W3CSPC: ILLEGAL INPUT PARAMETERS ***\n";
    r.ndse += "                   INPUT  : " + fi(nfr1, 8) + fi(nth1, 8) + ff(xf1, 10, 4) +
              ff(fr1, 10, 4) + "\n";
    r.ndse += "                   OUTPUT : " + fi(nfr2, 8) + fi(nth2, 8) + ff(xf2, 10, 4) +
              ff(fr2, 10, 4) + "\n";
    r.exit_code = 1;
    return r;
  }
  if (nsp < 0) {
    r.ndse += "\n *** ERROR W3CSPC: NEGATIVE NUMBER OF SPECTRA ***\n\n";
    r.exit_code = 2;
    return r;
  }
  if (nsp == 0) {
    r.ndse += "\n *** WARNING W3CSPC: NO SPECTRA ***\n\n";
    return r;
  }

  // 1. Search stored interpolation data for match (:295-319); .EQ. of REALs, so a NaN never
  // matches and +0. matches -0.
  CspcCase* cur = nullptr;
  for (const auto& cp : st.cases) {
    const CspcCase& c = *cp;
    if (c.nfr1 == nfr1 && c.nfr2 == nfr2 && c.nth1 == nth1 && c.nth2 == nth2 && c.xf1 == xf1 &&
        c.xf2 == xf2 && c.fr1 == fr1 && c.fr2 == fr2 && c.th1 == th1 && c.th2 == th2) {
      cur = cp.get();
      break;
    }
  }

  // 2. Link or compute interpolation data (:325-496)
  if (cur == nullptr) {
    const int rc = compute_case(st, nfr1, nth1, xf1, fr1, th1, nfr2, nth2, xf2, fr2, th2);
    if (rc != 0) {
      r.exit_code = rc;
      return r;
    }
    cur = st.cases.back().get();
  }
  CspcCase& c = *cur;

  // 3. Convert (:521-559)
  const std::size_t n1 = static_cast<std::size_t>(nth1) * static_cast<std::size_t>(nfr1);
  const std::size_t n2 = static_cast<std::size_t>(nth2) * static_cast<std::size_t>(nfr2);
  const std::size_t ns = static_cast<std::size_t>(nsp);
  auto s1 = [&](int i1, int i2, std::size_t is) -> const float& {
    return sp1[static_cast<std::size_t>(i1 - 1) +
               static_cast<std::size_t>(i2 - 1) * static_cast<std::size_t>(nth1) + is * n1];
  };
  auto s2 = [&](int j1, int j2, std::size_t is) -> float& {
    return sp2[static_cast<std::size_t>(j1 - 1) +
               static_cast<std::size_t>(j2 - 1) * static_cast<std::size_t>(nth2) + is * n2];
  };

  for (std::size_t k = 0; k < n2 * ns; ++k) sp2[k] = 0.0F;  // :521

  // 3.a Discrete energies (:523-536)
  for (int i2 = 1; i2 <= nfr1; ++i2) {
    for (int l2 = 1; l2 <= c.id_fr(0, i2); ++l2) {
      const int j2 = c.id_fr(l2, i2);
      const float r2 = c.rd_fr(l2, i2);
      for (int i1 = 1; i1 <= nth1; ++i1) {
        for (int l1 = 1; l1 <= c.id_th(0, i1); ++l1) {
          const int j1 = c.id_th(l1, i1);
          if (j1 < 1) {  // SP2(J1,J2,:) outside its first dimension (:407, :532)
            r.exit_code = kExitTableBounds;
            return r;
          }
          const float r1 = c.rd_th(l1, i1);
          const float frac = r2 * c.frq1[static_cast<std::size_t>(i2 - 1)] * c.xdf1 * r1 * c.dth1;
          for (std::size_t is = 0; is < ns; ++is) s2(j1, j2, is) = s2(j1, j2, is) + frac * s1(i1, i2, is);
        }
      }
    }
  }

  // 3.b Energy densities (:544-549)
  for (int j2 = 1; j2 <= nfr2; ++j2) {
    for (int j1 = 1; j1 <= nth2; ++j1) {
      const float fact = 1.0F / (c.frq2[static_cast<std::size_t>(j2 - 1)] * c.xdf2 * c.dth2);
      for (std::size_t is = 0; is < ns; ++is) s2(j1, j2, is) = fact * s2(j1, j2, is);
    }
  }

  // 3.c Add the tail (:557-559); NFR2T >= 2 because BHGH(1) > HGH would need FRQ2(1) above
  // the last input frequency, and then the Fortran reads SP2(:,0,:) ⚠
  for (int j2 = c.nfr2t; j2 <= nfr2; ++j2) {
    if (j2 < 2) {
      r.exit_code = kExitTableBounds;
      return r;
    }
    for (std::size_t is = 0; is < ns; ++is)
      for (int j1 = 1; j1 <= nth2; ++j1) s2(j1, j2, is) = ftl * s2(j1, j2 - 1, is);
  }
  return r;
}

}  // namespace ww::cspc
