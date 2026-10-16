// kokkos/src/ww_kokkos/pdlib_fsn2.hpp
// PDLIB_W3XYPFSN2: explicit N-scheme advection of one spectral component on the
// unstructured mesh of this MPI rank. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd_pdlib.F90:868-1200 at NOAA-EMC/WW3 03adda91;
// A. Roland, M. Dutour-Sikiric. Evidence: kokkos/port_report/PDLIB_W3XYPFSN2_KOKKOS.md.
// kRef1 is the W3_REF1 build (input_deep/switch_PDLIB, switch_NWS_rwps): it adds :1125-1127.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include <cmath>

#include "pdlib_params.hpp"
#include "real.hpp"

namespace ww::pdlib {

namespace fsn2_detail {
// SSE MIN/MAX as emitted (dest OP src ? dest : src): a NaN or a +0/-0 tie in
// `a` gives `b`. Each call passes the operands in the order read from the object.
KOKKOS_INLINE_FUNCTION Real min_ss(Real a, Real b) { return (a < b) ? a : b; }
KOKKOS_INLINE_FUNCTION Real max_ss(Real a, Real b) { return (a > b) ? a : b; }
KOKKOS_INLINE_FUNCTION double max_sd(double a, double b) { return (a > b) ? a : b; }
}  // namespace fsn2_detail

/// The Fortran's automatic arrays (w3profsmd_pdlib.F90:975-981), allocated once
/// by the caller and reused, plus two C++ work entries.
template <class MemSpace>
struct Fsn2Work {
  using memory_space = MemSpace;
  using W1 = Kokkos::View<Real*, Kokkos::LayoutLeft, MemSpace>;
  using W2 = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;
  W2 kelem;   ///< KELEM(3,NE)
  W2 flall;   ///< FLALL(3,NE)
  W1 nm;      ///< NM(NE)
  W1 kksum;   ///< KKSUM(npa)
  W1 st;      ///< ST(npa)
  W1 dtsi;    ///< DTSI(npa)
  W1 u;       ///< U(npa)
  W1 utilde;  ///< not in Fortran: UTILDE of :1111 per element, (NE)
  Kokkos::View<Real, MemSpace> dtmax;  ///< not in Fortran: DTMAX out of the LCALC loop

  static Fsn2Work allocate(int npa, int ne) {
    Fsn2Work w;
    w.kelem = W2("fsn2_kelem", 3, ne);
    w.flall = W2("fsn2_flall", 3, ne);
    w.nm = W1("fsn2_nm", ne);
    w.kksum = W1("fsn2_kksum", npa);
    w.st = W1("fsn2_st", npa);
    w.dtsi = W1("fsn2_dtsi", npa);
    w.u = W1("fsn2_u", npa);
    w.utilde = W1("fsn2_utilde", ne);
    w.dtmax = Kokkos::View<Real, MemSpace>("fsn2_dtmax");
    return w;
  }
};

/// C(npa,2) and AC(npa), the dummy arguments (:944-950).
template <class MemSpace>
using Fsn2CView = Kokkos::View<const Real**, Kokkos::LayoutLeft, MemSpace>;
template <class MemSpace>
using Fsn2AcView = Kokkos::View<Real*, Kokkos::LayoutLeft, MemSpace>;

/// :1015-1061. The element pass, and when lcalc the KKSUM gather and the serial
/// CFL loop that updates CFLXYMAX(1:np). Returns this rank's DTMAX, the FIN(1)
/// of :1062 (DBLE(10.E10) as REAL when not lcalc, never read then).
template <class ExecSpace>
Real fsn2_elements_cfl(const ExecSpace& exec,
                       const PdlibGeometry<typename ExecSpace::memory_space>& g,
                       const PdlibState<typename ExecSpace::memory_space>& s,
                       const Fsn2Work<typename ExecSpace::memory_space>& w,
                       const Fsn2CView<typename ExecSpace::memory_space>& c, bool lcalc,
                       Real dt) {
  using Policy = Kokkos::RangePolicy<ExecSpace>;
  using fsn2_detail::max_sd;
  using fsn2_detail::max_ss;
  using fsn2_detail::min_ss;
  const auto ine = g.ine;
  const auto ien = g.ien;
  const auto kelem = w.kelem;
  const auto flall = w.flall;
  const auto nm = w.nm;

  // 2.a (:1015-1042), one element per index.
  Kokkos::parallel_for(
      "pdlib_fsn2_elements", Policy(exec, 0, g.ne), KOKKOS_LAMBDA(const int e) {
        const int i1 = ine(0, e) - 1;  // :1016-1018
        const int i2 = ine(1, e) - 1;
        const int i3 = ine(2, e) - 1;
        // :1019-1020, ((C1+C2)+C3) then * ONESIXTH.
        const Real l1 = ((c(i1, 0) + c(i2, 0)) + c(i3, 0)) * kOneSixth;
        const Real l2 = ((c(i1, 1) + c(i2, 1)) + c(i3, 1)) * kOneSixth;
        // :1021-1023, two products then one add.
        const Real k1 = l1 * ien(0, e) + l2 * ien(1, e);
        const Real k2 = l1 * ien(2, e) + l2 * ien(3, e);
        const Real k3 = l1 * ien(4, e) + l2 * ien(5, e);
        // :1025. SUM(MIN(ZERO,KTMP)) is inlined as ((m1 + 0.) + m2) + m3; the
        // + 0. is kept (addss %0): it turns a -0 into +0, as the object does.
        Real sum = min_ss(k1, kZero) + kZero;
        sum = sum + min_ss(k2, kZero);
        sum = sum + min_ss(k3, kZero);
        const Real den = min_ss(sum, -kThr);
        nm(e) = -(static_cast<Real>(1.0f) / den);  // divss 1.0, then xorps sign
        // :1026.
        const Real kk1 = max_ss(k1, kZero);
        const Real kk2 = max_ss(k2, kZero);
        const Real kk3 = max_ss(k3, kZero);
        kelem(0, e) = kk1;
        kelem(1, e) = kk2;
        kelem(2, e) = kk3;
        // :1027-1032.
        const Real fl11 = c(i2, 0) * ien(0, e) + c(i2, 1) * ien(1, e);
        const Real fl12 = c(i3, 0) * ien(0, e) + c(i3, 1) * ien(1, e);
        const Real fl21 = c(i3, 0) * ien(2, e) + c(i3, 1) * ien(3, e);
        const Real fl22 = c(i1, 0) * ien(2, e) + c(i1, 1) * ien(3, e);
        const Real fl31 = c(i1, 0) * ien(4, e) + c(i1, 1) * ien(5, e);
        const Real fl32 = c(i2, 0) * ien(4, e) + c(i2, 1) * ien(5, e);
        // :1033-1038, in double: 2.d0*a is exact, then + b, then rounded to REAL.
        const Real fl111 = static_cast<Real>(2.0 * static_cast<double>(fl11) + static_cast<double>(fl12));
        const Real fl112 = static_cast<Real>(2.0 * static_cast<double>(fl12) + static_cast<double>(fl11));
        const Real fl211 = static_cast<Real>(2.0 * static_cast<double>(fl21) + static_cast<double>(fl22));
        const Real fl212 = static_cast<Real>(2.0 * static_cast<double>(fl22) + static_cast<double>(fl21));
        const Real fl311 = static_cast<Real>(2.0 * static_cast<double>(fl31) + static_cast<double>(fl32));
        const Real fl312 = static_cast<Real>(2.0 * static_cast<double>(fl32) + static_cast<double>(fl31));
        // :1039-1041, (a + b) * ONESIXTH, then + KELEM after the MAX of :1026.
        flall(0, e) = (fl311 + fl212) * kOneSixth + kk1;
        flall(1, e) = (fl111 + fl312) * kOneSixth + kk2;
        flall(2, e) = (fl211 + fl112) * kOneSixth + kk3;
      });
  exec.fence();

  if (!lcalc) return kDtmaxInit;  // :999, unused when not LCALC

  // :1048-1052, KKSUM = ZERO then the scatter-add, as a gather in IE order:
  // IE_CELL lists each node's elements in increasing IE, the Fortran's addition order.
  const auto kksum = w.kksum;
  const auto cell_off = g.cell_off;
  const auto ie_cell = g.ie_cell;
  const auto pos_cell = g.pos_cell;
  Kokkos::parallel_for(
      "pdlib_fsn2_kksum", Policy(exec, 0, g.npa), KOKKOS_LAMBDA(const int ip) {
        Real acc = kZero;
        for (int j = cell_off(ip); j < cell_off(ip + 1); ++j)
          acc = acc + kelem(pos_cell(j) - 1, ie_cell(j) - 1);
        kksum(ip) = acc;
      });
  exec.fence();

  // :1053-1061, one thread: DTMAXEXP carries across IP and the MIN chain depends on order.
  const int np = g.np;
  const int fsbccfl = g.fsbccfl;
  const auto iobp = g.iobp_loc;
  const auto iobdp = g.iobdp_loc;
  const auto si = g.si;
  const auto cflxymax = s.cflxymax;
  const auto dtmax_out = w.dtmax;
  Kokkos::parallel_for(
      "pdlib_fsn2_cfl", Policy(exec, 0, 1), KOKKOS_LAMBDA(const int) {
        const double dt_d = static_cast<double>(dt);
        Real dtmax = kDtmaxInit;        // :999
        Real dtmaxexp = kDtmaxExpInit;  // :1053
        for (int ip = 0; ip < np; ++ip) {
          if (iobp(ip) == 1 || fsbccfl != 0) {  // :1056
            // :1057, KKSUM*IOBDP in REAL (cvtsi2ss, mulss), then double.
            const Real p = static_cast<Real>(iobdp(ip)) * kksum(ip);
            const double d = max_sd(static_cast<double>(p), kSiFloor);
            dtmaxexp = static_cast<Real>(static_cast<double>(si(ip)) / d);
            dtmax = min_ss(dtmax, dtmaxexp);  // :1058
          }
          // :1060.
          const double q = dt_d / static_cast<double>(dtmaxexp);
          cflxymax(ip) = static_cast<Real>(max_sd(q, static_cast<double>(cflxymax(ip))));
        }
        dtmax_out() = dtmax;
      });
  exec.fence();
  Real dtmax = kDtmaxInit;
  Kokkos::View<Real, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>> h(&dtmax);
  Kokkos::deep_copy(exec, h, dtmax_out);
  exec.fence();
  return dtmax;
}

/// :1065-1073 on the host, after the MPI_ALLREDUCE: the new ITER(IK,ITH) from
/// DTMAX_GL. NINT is glibc's lroundf, the INTEGER(4) result is its low 32 bits,
/// and ABS is neg/cmovns (ABS(-2**31) stays -2**31), as the object does.
inline int fsn2_iter(Real dt, Real dtmax_gl) {
  const Real cflxy = dt / dtmax_gl;  // :1065, DBLE(DT)/DTMAX_GL narrowed to divss, as gfortran does
  // :1066, fprem of CFLXY by 1 (exact), to double, cvtsd2ss, andps.
  const Real rest = std::fabs(static_cast<Real>(std::fmod(static_cast<double>(cflxy), 1.0)));
  const int n = static_cast<int>(static_cast<unsigned int>(
      static_cast<unsigned long>(std::lroundf(cflxy))));
  const int an = (n < 0) ? static_cast<int>(0u - static_cast<unsigned int>(n)) : n;
  if (rest < kThr) return an;                                         // :1067-1068
  if (rest > kThr && rest < static_cast<Real>(0.5f))                  // :1069, 0.5d0 exact
    return static_cast<int>(static_cast<unsigned int>(an) + 1u);      // :1070
  return an;                                                          // :1072
}

/// :1080-1082: DTSI(IP) = DBLE(DT)/DBLE(ITER)/PDLIB_SI(IP) for IP = 1..npa.
/// The quotient DT/ITER is formed once, then divided by each SI, in double.
template <class ExecSpace>
void fsn2_dtsi(const ExecSpace& exec, const PdlibGeometry<typename ExecSpace::memory_space>& g,
               const Fsn2Work<typename ExecSpace::memory_space>& w, Real dt, int iter) {
  const double q = static_cast<double>(dt) / static_cast<double>(iter);
  const auto si = g.si;
  const auto dtsi = w.dtsi;
  Kokkos::parallel_for(
      "pdlib_fsn2_dtsi", Kokkos::RangePolicy<ExecSpace>(exec, 0, g.npa),
      KOKKOS_LAMBDA(const int ip) { dtsi(ip) = static_cast<Real>(q / static_cast<double>(si(ip))); });
  exec.fence();
}

/// One sub-step IT (1-based) of :1097-1176, everything before the exchange of
/// :1188. The caller performs PDLIB_exchange1DREAL(AC) after it returns.
/// kRef1 adds the W3_REF1 statement of :1126, which reads refpars3 = REFPARS(3).
template <bool kRef1 = false, class ExecSpace>
void fsn2_substep(const ExecSpace& exec, const PdlibGeometry<typename ExecSpace::memory_space>& g,
                  const PdlibBoundary<typename ExecSpace::memory_space>& b,
                  const Fsn2Work<typename ExecSpace::memory_space>& w, int isp, int ith, int it,
                  int iter, Real rd10, Real rd20, Real dt,
                  const Fsn2AcView<typename ExecSpace::memory_space>& ac,
                  [[maybe_unused]] Real refpars3 = kZero) {
  using Policy = Kokkos::RangePolicy<ExecSpace>;
  using fsn2_detail::max_ss;
  using fsn2_detail::min_ss;
  const auto ine = g.ine;
  const auto u = w.u;
  const auto st = w.st;
  const auto kelem = w.kelem;
  const auto flall = w.flall;
  const auto nm = w.nm;
  const auto utilde = w.utilde;
  const auto dtsi = w.dtsi;

  // :1107, U = DBLE(AC) into a REAL array: a copy (memcpy in the object).
  Kokkos::parallel_for(
      "pdlib_fsn2_u", Policy(exec, 0, g.npa), KOKKOS_LAMBDA(const int ip) { u(ip) = ac(ip); });
  exec.fence();

  // :1111, UTILDE = NM * DOT_PRODUCT(FLALL(:,IE), U(NI)); DOT_PRODUCT inlined
  // as ((f1*u1 + 0.) + f2*u2) + f3*u3, the + 0. kept.
  Kokkos::parallel_for(
      "pdlib_fsn2_utilde", Policy(exec, 0, g.ne), KOKKOS_LAMBDA(const int e) {
        Real d = flall(0, e) * u(ine(0, e) - 1) + kZero;
        d = d + flall(1, e) * u(ine(1, e) - 1);
        d = d + flall(2, e) * u(ine(2, e) - 1);
        utilde(e) = nm(e) * d;
      });
  exec.fence();

  // :1108, :1112, ST = ZERO then ST(NI) = ST(NI) + KELEM*(U(NI)-UTILDE), as a
  // gather in IE order, no atomics.
  const auto cell_off = g.cell_off;
  const auto ie_cell = g.ie_cell;
  const auto pos_cell = g.pos_cell;
  Kokkos::parallel_for(
      "pdlib_fsn2_st", Policy(exec, 0, g.npa), KOKKOS_LAMBDA(const int ip) {
        Real acc = kZero;
        const Real ui = u(ip);
        for (int j = cell_off(ip); j < cell_off(ip + 1); ++j) {
          const int e = ie_cell(j) - 1;
          acc = acc + kelem(pos_cell(j) - 1, e) * (ui - utilde(e));
        }
        st(ip) = acc;
      });
  exec.fence();

  // :1123-1128 and :1134 (AC = REAL(U), a copy). In REAL: DTSI*ST, times
  // REAL(1-IOBPA), U minus that, MAX with 0; then in double times IOBPD(ITH,IP)
  // and times IOBDP(IP), rounded to REAL.
  const auto iobpa = g.iobpa_loc;
  const auto iobpd = g.iobpd_loc;
  const auto iobdp = g.iobdp_loc;
  const int ith0 = ith - 1;
  if constexpr (!kRef1) {
    Kokkos::parallel_for(
        "pdlib_fsn2_update", Policy(exec, 0, g.npa), KOKKOS_LAMBDA(const int ip) {
          const Real a = dtsi(ip) * st(ip);
          const Real m = max_ss(u(ip) - a * static_cast<Real>(1 - static_cast<int>(iobpa(ip))), kZero);
          const double v = (static_cast<double>(m) * static_cast<double>(iobpd(ith0, ip))) *
                           static_cast<double>(iobdp(ip));
          const Real un = static_cast<Real>(v);
          u(ip) = un;
          ac(ip) = un;
        });
  } else {
    // :1126: REFPARS(3) < 0.5 keeps the old AC. A lambda of its own: a name in a discarded
    // if-constexpr branch is still captured, and the kRef1 = false closure must stay as it was.
    Kokkos::parallel_for(
        "pdlib_fsn2_update", Policy(exec, 0, g.npa), KOKKOS_LAMBDA(const int ip) {
          const Real a = dtsi(ip) * st(ip);
          const Real m = max_ss(u(ip) - a * static_cast<Real>(1 - static_cast<int>(iobpa(ip))), kZero);
          const double v = (static_cast<double>(m) * static_cast<double>(iobpd(ith0, ip))) *
                           static_cast<double>(iobdp(ip));
          Real un = static_cast<Real>(v);
          // REFPARS(3) .LT. 0.5 (REAL); AC(IP) still holds the value before :1134.
          if (refpars3 < static_cast<Real>(0.5f) && iobpd(ith0, ip) == 0 && iobpa(ip) == 0)
            un = ac(ip);
          u(ip) = un;
          ac(ip) = un;
        });
  }
  exec.fence();

  // :1145-1176, one thread in IBI order: two IBI on one node leave the last write.
  if (b.flbpi == 0) return;
  const int nbi = b.nbi;
  const int isp0 = isp - 1;
  const auto isbpi = b.isbpi;
  const auto bbpi0 = b.bbpi0;
  const auto bbpin = b.bbpin;
  const auto mapsf = g.mapsf;
  const auto ipgl_npa = g.ipgl_npa;
  const auto clats = g.clats;
  Kokkos::parallel_for(
      "pdlib_fsn2_boundary", Policy(exec, 0, 1), KOKKOS_LAMBDA(const int) {
        Real rd1;
        Real rd2 = rd20;  // :1147
        if (rd2 > kRd2Floor) {
          // :1146, DT*REAL(ITER-IT), / REAL(ITER), subtracted from RD10.
          rd1 = rd10 - (dt * static_cast<Real>(iter - it)) / static_cast<Real>(iter);
          rd2 = min_ss(max_ss(rd1 / rd2, kZero), static_cast<Real>(1.0f));  // :1149
          rd1 = static_cast<Real>(1.0f) - rd2;                               // :1150
        } else {
          rd1 = kZero;                   // :1152
          rd2 = static_cast<Real>(1.0f);  // :1153
        }
        for (int ibi = 1; ibi <= nbi; ++ibi) {
          const int isea = isbpi(ibi - 1);
          const int ip_glob = mapsf(isea - 1, 0);  // :1163
          const int jx = ipgl_npa(ip_glob - 1);    // :1164
          if (jx > 0) {
            // :1166, the paren wraps the sum, then * CLATS(ISBPI(IBI)).
            ac(jx - 1) = (rd1 * bbpi0(isp0, ibi) + rd2 * bbpin(isp0, ibi)) * clats(isea);
          }
        }
      });
  exec.fence();
}

/// PDLIB_W3XYPFSN2(ISP, C, LCALC, RD10, RD20, DT, AC). allreduce_min(Real) -> Real is the
/// shim's MPI_ALLREDUCE of :1062-1064 (LCALC only); exchange(ac) its PDLIB_exchange1DREAL(AC)
/// of :1188, after every sub-step. Host callables, called after a fence; a device-resident
/// AC must be brought to the host and back by exchange. refpars3 = REFPARS(3), kRef1 only.
template <bool kRef1 = false, class ExecSpace, class AllreduceMin, class Exchange>
void w3xypfsn2(const ExecSpace& exec, const PdlibGeometry<typename ExecSpace::memory_space>& g,
               const PdlibState<typename ExecSpace::memory_space>& s,
               const PdlibBoundary<typename ExecSpace::memory_space>& b,
               const Fsn2Work<typename ExecSpace::memory_space>& w, int isp,
               const Fsn2CView<typename ExecSpace::memory_space>& c, bool lcalc, Real rd10,
               Real rd20, Real dt, const Fsn2AcView<typename ExecSpace::memory_space>& ac,
               AllreduceMin&& allreduce_min, Exchange&& exchange, Real refpars3 = kZero) {
  const int ith = 1 + (isp - 1) % g.nth;  // :997
  const int ik = 1 + (isp - 1) / g.nth;   // :998
  using HostInt = Kokkos::View<int, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
  const auto iter_ik_ith = Kokkos::subview(s.iter, ik - 1, ith - 1);

  const Real dtmax = fsn2_elements_cfl(exec, g, s, w, c, lcalc, dt);
  int iter = 0;
  if (lcalc) {
    const Real dtmax_gl = allreduce_min(dtmax);  // :1062-1064, MPI in the shim
    iter = fsn2_iter(dt, dtmax_gl);              // :1065-1073
    HostInt h(&iter);
    Kokkos::deep_copy(exec, iter_ik_ith, h);  // ITER(IK,ITH) =
    exec.fence();
  } else {
    HostInt h(&iter);
    Kokkos::deep_copy(exec, h, iter_ik_ith);
    exec.fence();
  }

  fsn2_dtsi(exec, g, w, dt, iter);  // :1080-1082
  // :1097, the trip count is ITER(IK,ITH) read once.
  for (int it = 1; it <= iter; ++it) {
    fsn2_substep<kRef1>(exec, g, b, w, isp, ith, it, iter, rd10, rd20, dt, ac, refpars3);
    exchange(ac);  // :1188, PDLIB_exchange1DREAL(AC) in the shim
  }
}

}  // namespace ww::pdlib
