// kokkos/src/ww_kokkos/pdlib_fsfct2.hpp
// PDLIB_W3XYPFSFCT2: explicit flux-corrected transport (FCT) advection of one spectral component
// on the unstructured mesh of this MPI rank. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd_pdlib.F90:1504-1877 at NOAA-EMC/WW3 03adda91;
// A. Roland, M. Dutour-Sikiric. Evidence: kokkos/port_report/PDLIB_W3XYPFSFCT2_KOKKOS.md.
// No regtest namelist sets EXPFSFCT = T, so no WW3 regtest reaches this routine ⚠.
// kRef1 is W3_REF1: it adds :1805 and changes the MIN/MAX order of :1762-1763.
// Hook positions: allreduce_min at :1699-1701 (LCALC only), exchange(ac) at :1863 after each
// sub-step's boundary update.
// Each value is rounded in its declared width (:1603-1626), as the GIMPLE does; the + 0.0 of the
// inlined SUM/DOT_PRODUCT is kept (:1665, :1725-1726); BET1 is (-THETA_L)/q6 (:1729).
// MIN/MAX operand order, read from the object (min_ab(a,b) = a<b?a:b, a the destination):
//   :1697 max_ab(q,CFL) (span-1 loop copy). :1762-1763 by IP, nv = npa>3 ? npa&~3 : 0:
//   REF0 USTARI(1) = max_ab(UL,U) at IP-1 == nv, else max_ab(U,UL); USTARI(2) = min_ab(U,UL).
//   REF1 USTARI(1) = max_ab(U,UL); USTARI(2) = min_ab(UL,U) at IP-1 in {nv,nv+1} (npa>3),
//   0 (npa == 1), 1 (npa in {2,3}), else min_ab(U,UL).
//   :1769-1770 corner 3 takes max_ab(lim,UIP)/min_ab(lim,UIM), corners 1-2 the other order.
//   :1773-1774 max_ab(THR,PP), min_ab(-THR,PM) at S-1 == nve+2 (nve = ne>3 ? ne&~3 : 0), else
//   the other order.
// Extents departure ⚠: UIP/UIM are (NE) but indexed by node, and :1773-1774 run S = 1..NE over
// WII(2,NPA); the Fortran is defined only when NE == npa. The C++ sizes them npa and runs
// S = 1..min(NE,npa), a departure for the owner to confirm.
// Node sums (:1688, :1740, :1744-1745, :1769-1770, :1797-1799) are gathered per node over
// PDLIB_IE_CELL in increasing IE: the Fortran's order. :1691-1698 and :1837-1850 run serially.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include <cmath>
#include <cstdint>

#include "pdlib_fsn2.hpp"
#include "pdlib_params.hpp"
#include "real.hpp"

namespace ww::pdlib {

namespace fsfct2_detail {
// minss/maxss/minsd/maxsd as emitted: a is the destination register.
KOKKOS_INLINE_FUNCTION Real min_ab(Real a, Real b) { return (a < b) ? a : b; }
KOKKOS_INLINE_FUNCTION Real max_ab(Real a, Real b) { return (a > b) ? a : b; }
KOKKOS_INLINE_FUNCTION double min_ab(double a, double b) { return (a < b) ? a : b; }
KOKKOS_INLINE_FUNCTION double max_ab(double a, double b) { return (a > b) ? a : b; }

// THR = TINY(1.0) and the literals of the routine, as the object holds them.
inline constexpr double kThrD = static_cast<double>(kThr);         // .LC44 0x3810000000000000
inline constexpr double kOneSixthD = static_cast<double>(kOneSixth);  // .LC96 0x3fc5555560000000
inline constexpr double kOneThirdD = static_cast<double>(1.0f / 3.0f);  // 1./3., .LC202
static_assert(kThrD == 0x1p-126, "DBLE(TINY(1.0))");
static_assert(kOneSixthD == 0x1.5555560000000p-3, "ONESIXTH widened, 0x3fc5555560000000");
static_assert(kOneThirdD == 0x1.5555560000000p-2, "1./3. widened, 0x3fd5555560000000");

KOKKOS_INLINE_FUNCTION float nan_f() { return Kokkos::bit_cast<float>(0x7fc00000u); }  // .LC74
KOKKOS_INLINE_FUNCTION double nan_d() {  // .LC192
  return Kokkos::bit_cast<double>(static_cast<std::uint64_t>(0x7ff8000000000000ull));
}
KOKKOS_INLINE_FUNCTION float inf_f() { return Kokkos::bit_cast<float>(0x7f800000u); }
KOKKOS_INLINE_FUNCTION double inf_d() {
  return Kokkos::bit_cast<double>(static_cast<std::uint64_t>(0x7ff0000000000000ull));
}
}  // namespace fsfct2_detail

/// PDLIB_TRIA(ne), REAL(rkind=4) (PDLIB/yownodepool.F90:80, allocated
/// yowpdlibmain.F90:1369, set :1400): element area. Not in PdlibGeometry.
template <class MemSpace>
using Fsfct2TriaView = Kokkos::View<const Real*, Kokkos::LayoutLeft, MemSpace>;
/// C(npa,2) and AC(npa), the dummy arguments (:1582, :1584).
template <class MemSpace>
using Fsfct2CView = Kokkos::View<const Real**, Kokkos::LayoutLeft, MemSpace>;
template <class MemSpace>
using Fsfct2AcView = Kokkos::View<Real*, Kokkos::LayoutLeft, MemSpace>;

/// The automatic arrays of :1611-1626 in their declared widths, allocated once
/// and reused; uip/uim/wii sized npa (header, "Extents"); four C++ entries.
template <class MemSpace>
struct Fsfct2Work {
  using memory_space = MemSpace;
  using F1 = Kokkos::View<Real*, Kokkos::LayoutLeft, MemSpace>;
  using F2 = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;
  using D1 = Kokkos::View<double*, Kokkos::LayoutLeft, MemSpace>;
  using D2 = Kokkos::View<double**, Kokkos::LayoutLeft, MemSpace>;
  D2 kelem, flall;          ///< KELEM(3,NE), FLALL(3,NE)
  D1 nm;                    ///< NM(NE)
  D1 kksum, st, pp, pm;     ///< KKSUM, ST, PP, PM (npa)
  D2 theta_l, theta_ace;    ///< THETA_L(3,NE), THETA_ACE(3,NE)
  D1 uim;                   ///< UIM, (npa) here, (NE) in Fortran
  D2 wii;                   ///< WII(2,NPA)
  F1 dtsi, u, ul;           ///< DTSI, U, UL (npa)
  F1 uip;                   ///< UIP, (npa) here, (NE) in Fortran
  F2 ustari;                ///< USTARI(2,NPA)
  F1 lim_max;               ///< not in Fortran: MAXVAL(USTARI(1,NI)) per element (NE)
  D1 lim_min;               ///< not in Fortran: DBLE(MINVAL(USTARI(2,NI))) per element (NE)
  D1 tmp1;                  ///< not in Fortran: TMP1 of :1796 per element (NE)
  Kokkos::View<Real, MemSpace> dtmax;  ///< not in Fortran: DTMAX out of the LCALC loop

  static Fsfct2Work allocate(int npa, int ne) {
    Fsfct2Work w;
    w.kelem = D2("fsfct2_kelem", 3, ne);
    w.flall = D2("fsfct2_flall", 3, ne);
    w.nm = D1("fsfct2_nm", ne);
    w.kksum = D1("fsfct2_kksum", npa);
    w.st = D1("fsfct2_st", npa);
    w.pp = D1("fsfct2_pp", npa);
    w.pm = D1("fsfct2_pm", npa);
    w.theta_l = D2("fsfct2_theta_l", 3, ne);
    w.theta_ace = D2("fsfct2_theta_ace", 3, ne);
    w.uim = D1("fsfct2_uim", npa);
    w.wii = D2("fsfct2_wii", 2, npa);
    w.dtsi = F1("fsfct2_dtsi", npa);
    w.u = F1("fsfct2_u", npa);
    w.ul = F1("fsfct2_ul", npa);
    w.uip = F1("fsfct2_uip", npa);
    w.ustari = F2("fsfct2_ustari", 2, npa);
    w.lim_max = F1("fsfct2_lim_max", ne);
    w.lim_min = D1("fsfct2_lim_min", ne);
    w.tmp1 = D1("fsfct2_tmp1", ne);
    w.dtmax = Kokkos::View<Real, MemSpace>("fsfct2_dtmax");
    return w;
  }
};

/// :1655-1698. The element pass, and when lcalc the KKSUM gather and the serial
/// CFL loop that updates CFLXYMAX(1:np). Returns this rank's DTMAX, the FIN(1)
/// of :1699 (DBLE(10.E10) stored to REAL when not lcalc, never read then).
template <class ExecSpace>
Real fsfct2_elements_cfl(const ExecSpace& exec,
                         const PdlibGeometry<typename ExecSpace::memory_space>& g,
                         const PdlibState<typename ExecSpace::memory_space>& s,
                         const Fsfct2Work<typename ExecSpace::memory_space>& w,
                         const Fsfct2CView<typename ExecSpace::memory_space>& c, bool lcalc,
                         Real dt) {
  using Policy = Kokkos::RangePolicy<ExecSpace>;
  using fsfct2_detail::max_ab;
  using fsfct2_detail::min_ab;
  const auto ine = g.ine;
  const auto ien = g.ien;
  const auto kelem = w.kelem;
  const auto flall = w.flall;
  const auto nm = w.nm;

  Kokkos::parallel_for(
      "pdlib_fsfct2_elements", Policy(exec, 0, g.ne), KOKKOS_LAMBDA(const int e) {
        const int i1 = ine(0, e) - 1;  // :1656-1658
        const int i2 = ine(1, e) - 1;
        const int i3 = ine(2, e) - 1;
        // :1659-1660 in REAL, then widened (LAMBDA is REAL*8).
        const double l1 = static_cast<double>(((c(i1, 0) + c(i2, 0)) + c(i3, 0)) * kOneSixth);
        const double l2 = static_cast<double>(((c(i1, 1) + c(i2, 1)) + c(i3, 1)) * kOneSixth);
        // :1661-1663 in double.
        const double k1 = l1 * static_cast<double>(ien(0, e)) + l2 * static_cast<double>(ien(1, e));
        const double k2 = l1 * static_cast<double>(ien(2, e)) + l2 * static_cast<double>(ien(3, e));
        const double k3 = l1 * static_cast<double>(ien(4, e)) + l2 * static_cast<double>(ien(5, e));
        // :1665, SUM inlined as ((m1 + 0) + m2) + m3.
        double sum = min_ab(k1, 0.0) + 0.0;
        sum = sum + min_ab(k2, 0.0);
        sum = sum + min_ab(k3, 0.0);
        nm(e) = -(1.0 / min_ab(sum, -fsfct2_detail::kThrD));
        // :1666.
        kelem(0, e) = max_ab(k1, 0.0);
        kelem(1, e) = max_ab(k2, 0.0);
        kelem(2, e) = max_ab(k3, 0.0);
        // :1667-1672 in REAL (two products, one add), widened at the store.
        const double fl11 = static_cast<double>(c(i2, 0) * ien(0, e) + c(i2, 1) * ien(1, e));
        const double fl12 = static_cast<double>(c(i3, 0) * ien(0, e) + c(i3, 1) * ien(1, e));
        const double fl21 = static_cast<double>(c(i3, 0) * ien(2, e) + c(i3, 1) * ien(3, e));
        const double fl22 = static_cast<double>(c(i1, 0) * ien(2, e) + c(i1, 1) * ien(3, e));
        const double fl31 = static_cast<double>(c(i1, 0) * ien(4, e) + c(i1, 1) * ien(5, e));
        const double fl32 = static_cast<double>(c(i2, 0) * ien(4, e) + c(i2, 1) * ien(5, e));
        // :1673-1681 in double.
        const double fl111 = 2.0 * fl11 + fl12;
        const double fl112 = 2.0 * fl12 + fl11;
        const double fl211 = 2.0 * fl21 + fl22;
        const double fl212 = 2.0 * fl22 + fl21;
        const double fl311 = 2.0 * fl31 + fl32;
        const double fl312 = 2.0 * fl32 + fl31;
        flall(0, e) = fl311 + fl212;
        flall(1, e) = fl111 + fl312;
        flall(2, e) = fl211 + fl112;
      });
  exec.fence();

  if (!lcalc) return kDtmaxInit;  // :1639, unused when not LCALC

  // :1685-1689, KKSUM = ZERO then the scatter-add, as a gather in IE order.
  const auto kksum = w.kksum;
  const auto cell_off = g.cell_off;
  const auto ie_cell = g.ie_cell;
  const auto pos_cell = g.pos_cell;
  Kokkos::parallel_for(
      "pdlib_fsfct2_kksum", Policy(exec, 0, g.npa), KOKKOS_LAMBDA(const int ip) {
        double acc = 0.0;
        for (int j = cell_off(ip); j < cell_off(ip + 1); ++j)
          acc = acc + kelem(pos_cell(j) - 1, ie_cell(j) - 1);
        kksum(ip) = acc;
      });
  exec.fence();

  // :1690-1698, serial (header). IP_glob = iplg(IP) of :1692 is dead.
  const int np = g.np;
  const int fsbccfl = g.fsbccfl;
  const auto iobp = g.iobp_loc;
  const auto iobdp = g.iobdp_loc;
  const auto si = g.si;
  const auto cflxymax = s.cflxymax;
  const auto dtmax_out = w.dtmax;
  Kokkos::parallel_for(
      "pdlib_fsfct2_cfl", Policy(exec, 0, 1), KOKKOS_LAMBDA(const int) {
        const double dt_d = static_cast<double>(dt);
        Real dtmax = kDtmaxInit;        // :1639
        Real dtmaxexp = kDtmaxExpInit;  // :1690
        for (int ip = 0; ip < np; ++ip) {
          if (iobp(ip) == 1 || fsbccfl != 0) {  // :1693
            const double p = static_cast<double>(iobdp(ip)) * kksum(ip);  // :1694, double
            const double d = max_ab(p, kSiFloor);
            dtmaxexp = static_cast<Real>(static_cast<double>(si(ip)) / d);
            dtmax = min_ab(dtmax, dtmaxexp);  // :1695
          }
          const double q = dt_d / static_cast<double>(dtmaxexp);  // :1697
          cflxymax(ip) = static_cast<Real>(max_ab(q, static_cast<double>(cflxymax(ip))));
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

/// :1713-1715: DTSI(IP) = DBLE(DT)/DBLE(ITER(IK,ITH))/PDLIB_SI(IP), IP = 1..npa;
/// DT/ITER formed once, in double, rounded to REAL at the store.
template <class ExecSpace>
void fsfct2_dtsi(const ExecSpace& exec, const PdlibGeometry<typename ExecSpace::memory_space>& g,
                 const Fsfct2Work<typename ExecSpace::memory_space>& w, Real dt, int iter) {
  const double q = static_cast<double>(dt) / static_cast<double>(iter);
  const auto si = g.si;
  const auto dtsi = w.dtsi;
  Kokkos::parallel_for(
      "pdlib_fsfct2_dtsi", Kokkos::RangePolicy<ExecSpace>(exec, 0, g.npa),
      KOKKOS_LAMBDA(const int ip) { dtsi(ip) = static_cast<Real>(q / static_cast<double>(si(ip))); });
  exec.fence();
}

/// One sub-step IT (1-based) of :1717-1855, everything before the exchange of
/// :1863. The caller performs PDLIB_exchange1DREAL(AC) after it returns.
/// kRef1 adds :1805, which reads refpars3 = REFPARS(3), and selects REF1's
/// MIN/MAX operand order at :1762-1763 (header).
template <bool kRef1 = false, class ExecSpace>
void fsfct2_substep(const ExecSpace& exec,
                    const PdlibGeometry<typename ExecSpace::memory_space>& g,
                    const PdlibBoundary<typename ExecSpace::memory_space>& b,
                    const Fsfct2Work<typename ExecSpace::memory_space>& w,
                    const Fsfct2TriaView<typename ExecSpace::memory_space>& tria, int isp,
                    int ith, int it, int iter, Real rd10, Real rd20, Real dt,
                    const Fsfct2AcView<typename ExecSpace::memory_space>& ac,
                    [[maybe_unused]] Real refpars3 = kZero) {
  using Policy = Kokkos::RangePolicy<ExecSpace>;
  using fsfct2_detail::max_ab;
  using fsfct2_detail::min_ab;
  const int npa = g.npa;
  const int ne = g.ne;
  const auto ine = g.ine;
  const auto cell_off = g.cell_off;
  const auto ie_cell = g.ie_cell;
  const auto pos_cell = g.pos_cell;
  const auto iobpa = g.iobpa_loc;
  const auto iobpd = g.iobpd_loc;
  const auto iobdp = g.iobdp_loc;
  const int ith0 = ith - 1;
  const auto kelem = w.kelem;
  const auto flall = w.flall;
  const auto nm = w.nm;
  const auto st = w.st;
  const auto pp = w.pp;
  const auto pm = w.pm;
  const auto theta_l = w.theta_l;
  const auto theta_ace = w.theta_ace;
  const auto uim = w.uim;
  const auto wii = w.wii;
  const auto dtsi = w.dtsi;
  const auto u = w.u;
  const auto ul = w.ul;
  const auto uip = w.uip;
  const auto ustari = w.ustari;
  const auto lim_max = w.lim_max;
  const auto lim_min = w.lim_min;
  const auto tmp1 = w.tmp1;

  // :1719, U = DBLE(AC) into a REAL array: a copy.
  Kokkos::parallel_for(
      "pdlib_fsfct2_u", Policy(exec, 0, npa), KOKKOS_LAMBDA(const int ip) { u(ip) = ac(ip); });
  exec.fence();

  // :1724-1739, :1741-1743 per element.
  Kokkos::parallel_for(
      "pdlib_fsfct2_theta", Policy(exec, 0, ne), KOKKOS_LAMBDA(const int e) {
        const double u1 = static_cast<double>(u(ine(0, e) - 1));
        const double u2 = static_cast<double>(u(ine(1, e) - 1));
        const double u3 = static_cast<double>(u(ine(2, e) - 1));
        // :1725. q6 = ONESIXTH*DOT_PRODUCT, FT = -q6.
        double d1 = u1 * flall(0, e) + 0.0;
        d1 = d1 + flall(1, e) * u2;
        d1 = d1 + u3 * flall(2, e);
        const double q6 = d1 * fsfct2_detail::kOneSixthD;
        const double ft = -q6;
        // :1726, DOT_PRODUCT(KELEM,U(NI)) - FT, emitted as + q6.
        const double k1 = kelem(0, e), k2 = kelem(1, e), k3 = kelem(2, e);
        double d2 = k1 * u1 + 0.0;
        d2 = d2 + k2 * u2;
        d2 = d2 + k3 * u3;
        const double utilde = nm(e) * (d2 + q6);
        // :1727.
        double t1 = k1 * (u1 - utilde);
        double t2 = k2 * (u2 - utilde);
        double t3 = k3 * (u3 - utilde);
        if (Kokkos::fabs(q6) > 0.0) {  // :1728, ABS(FT) = ABS(q6)
          // :1729, (-THETA_L)/q6.
          const double b1 = (-t1) / q6;
          const double b2 = (-t2) / q6;
          const double b3 = (-t3) / q6;
          if (b1 < 0.0 || b2 < 0.0 || b3 < 0.0) {  // :1730
            const double bh1 = b1 + 0.5 * b2;  // :1731-1733
            const double bh2 = b2 + 0.5 * b3;
            const double bh3 = b3 + 0.5 * b1;
            // :1734-1736, MIN(BH,1.d0-BH',1.d0) as min_ab(1-BH', min_ab(BH,1)).
            const double c1 = max_ab(min_ab(1.0 - bh2, min_ab(bh1, 1.0)), 0.0);
            const double c2 = max_ab(min_ab(1.0 - bh3, min_ab(bh2, 1.0)), 0.0);
            const double c3 = max_ab(min_ab(1.0 - bh1, min_ab(bh3, 1.0)), 0.0);
            t1 = ft * c1;  // :1737
            t2 = ft * c2;
            t3 = ft * c3;
          }
        }
        theta_l(0, e) = t1;
        theta_l(1, e) = t2;
        theta_l(2, e) = t3;
        // :1741, DT/(2.*TRIA) in REAL, widened; (that*KELEM + 1./3.) * FT.
        const double wl = static_cast<double>(dt / (static_cast<Real>(2.0f) * tria(e)));
        // :1743.
        theta_ace(0, e) = (k1 * wl + fsfct2_detail::kOneThirdD) * ft - t1;
        theta_ace(1, e) = (k2 * wl + fsfct2_detail::kOneThirdD) * ft - t2;
        theta_ace(2, e) = (k3 * wl + fsfct2_detail::kOneThirdD) * ft - t3;
      });
  exec.fence();

  // :1720-1722 (= ZERO), :1740, :1744-1745 as gathers in IE order (header).
  Kokkos::parallel_for(
      "pdlib_fsfct2_st_pp_pm", Policy(exec, 0, npa), KOKKOS_LAMBDA(const int ip) {
        const double dd = static_cast<double>(dtsi(ip));
        double s_acc = 0.0, p_acc = 0.0, m_acc = 0.0;
        for (int j = cell_off(ip); j < cell_off(ip + 1); ++j) {
          const int e = ie_cell(j) - 1;
          const int k = pos_cell(j) - 1;
          s_acc = s_acc + theta_l(k, e);
          const double a = -theta_ace(k, e);
          p_acc = p_acc + max_ab(a, 0.0) * dd;
          m_acc = m_acc + min_ab(a, 0.0) * dd;
        }
        st(ip) = s_acc;
        pp(ip) = p_acc;
        pm(ip) = m_acc;
      });
  exec.fence();

  // :1754-1756, in double, rounded to REAL.
  Kokkos::parallel_for(
      "pdlib_fsfct2_ul", Policy(exec, 0, npa), KOKKOS_LAMBDA(const int ip) {
        const double x = static_cast<double>(u(ip)) -
                         (static_cast<double>(dtsi(ip)) * st(ip)) *
                             static_cast<double>(1 - static_cast<int>(iobpa(ip)));
        const double v = (max_ab(x, 0.0) * static_cast<double>(iobpd(ith0, ip))) *
                         static_cast<double>(iobdp(ip));
        ul(ip) = static_cast<Real>(v);
      });
  exec.fence();

  // :1762-1763, operand order by index and switch (header).
  const int nvn = (npa > 3) ? (npa & ~3) : 0;
  const int max_swap = kRef1 ? -1 : nvn;
  const int min_swap_a = !kRef1 ? -1 : (npa > 3 ? nvn : (npa == 1 ? 0 : 1));
  const int min_swap_b = (kRef1 && npa > 3) ? nvn + 1 : -1;
  Kokkos::parallel_for(
      "pdlib_fsfct2_ustari", Policy(exec, 0, npa), KOKKOS_LAMBDA(const int ip) {
        const Real a = u(ip);
        const Real l = ul(ip);
        ustari(0, ip) = (ip == max_swap) ? max_ab(l, a) : max_ab(a, l);
        ustari(1, ip) = (ip == min_swap_a || ip == min_swap_b) ? min_ab(l, a) : min_ab(a, l);
      });
  exec.fence();

  // :1769-1770, MAXVAL/MINVAL of the element's three corners (GIMPLE order).
  Kokkos::parallel_for(
      "pdlib_fsfct2_minmaxval", Policy(exec, 0, ne), KOKKOS_LAMBDA(const int e) {
        const int n1 = ine(0, e) - 1, n2 = ine(1, e) - 1, n3 = ine(2, e) - 1;
        const Real ninf = -fsfct2_detail::inf_f();
        const Real pinf = fsfct2_detail::inf_f();
        const Real a1 = ustari(0, n1), a2 = ustari(0, n2), a3 = ustari(0, n3);
        Real mx;
        if (a1 >= ninf) {
          const Real m = (a1 < a2) ? a2 : a1;
          mx = (m < a3) ? a3 : m;
        } else if (a2 >= ninf) {
          mx = (a2 < a3) ? a3 : a2;
        } else if (a3 >= ninf) {
          mx = a3;
        } else {
          mx = fsfct2_detail::nan_f();
        }
        lim_max(e) = mx;
        const Real c1 = ustari(1, n1), c2 = ustari(1, n2), c3 = ustari(1, n3);
        double mn;
        if (c1 <= pinf) {
          const Real m = (c1 > c2) ? c2 : c1;
          mn = static_cast<double>((c3 < m) ? c3 : m);
        } else if (c2 <= pinf) {
          mn = static_cast<double>((c3 < c2) ? c3 : c2);
        } else if (c3 <= pinf) {
          mn = static_cast<double>(c3);
        } else {
          mn = fsfct2_detail::nan_d();
        }
        lim_min(e) = mn;
      });
  exec.fence();

  // :1765-1771, UIP = 0., UIM = 0., then the MAX/MIN gathers in IE order; the
  // third corner takes the other operand order (header).
  Kokkos::parallel_for(
      "pdlib_fsfct2_uip_uim", Policy(exec, 0, npa), KOKKOS_LAMBDA(const int ip) {
        Real p_acc = static_cast<Real>(0.0f);
        double m_acc = 0.0;
        for (int j = cell_off(ip); j < cell_off(ip + 1); ++j) {
          const int e = ie_cell(j) - 1;
          const Real lx = lim_max(e);
          const double ln = lim_min(e);
          if (pos_cell(j) == 3) {
            p_acc = max_ab(lx, p_acc);
            m_acc = min_ab(ln, m_acc);
          } else {
            p_acc = max_ab(p_acc, lx);
            m_acc = min_ab(m_acc, ln);
          }
        }
        uip(ip) = p_acc;
        uim(ip) = m_acc;
      });
  exec.fence();

  // :1773-1774 for S = 1..min(NE,npa) (header, "Extents"); the third
  // scalar-tail element of the NE-trip loop swaps the THR operands.
  const int nve = (ne > 3) ? (ne & ~3) : 0;
  const int tail3 = nve + 2;
  const int ns = (ne < npa) ? ne : npa;
  Kokkos::parallel_for(
      "pdlib_fsfct2_wii", Policy(exec, 0, ns), KOKKOS_LAMBDA(const int sx) {
        const double thr = fsfct2_detail::kThrD;
        const double den1 = (sx == tail3) ? max_ab(thr, pp(sx)) : max_ab(pp(sx), thr);
        wii(0, sx) = min_ab(static_cast<double>(uip(sx) - ul(sx)) / den1, 1.0);
        const double den2 = (sx == tail3) ? min_ab(-thr, pm(sx)) : min_ab(pm(sx), -thr);
        wii(1, sx) = min_ab((uim(sx) - static_cast<double>(ul(sx))) / den2, 1.0);
      });
  exec.fence();

  // :1777-1796, TMP and TMP1 = MINVAL(TMP) per element (GIMPLE order).
  Kokkos::parallel_for(
      "pdlib_fsfct2_tmp1", Policy(exec, 0, ne), KOKKOS_LAMBDA(const int e) {
        const int n1 = ine(0, e) - 1, n2 = ine(1, e) - 1, n3 = ine(2, e) - 1;
        const double t1 = (theta_ace(0, e) < 0.0) ? wii(0, n1) : wii(1, n1);
        const double t2 = (theta_ace(1, e) < 0.0) ? wii(0, n2) : wii(1, n2);
        const double t3 = (theta_ace(2, e) < 0.0) ? wii(0, n3) : wii(1, n3);
        const double pinf = fsfct2_detail::inf_d();
        double r;
        if (t1 <= pinf) {
          const double m = (t1 > t2) ? t2 : t1;
          r = (t3 < m) ? t3 : m;
        } else if (t2 <= pinf) {
          r = (t3 < t2) ? t3 : t2;
        } else if (t3 <= pinf) {
          r = t3;
        } else {
          r = fsfct2_detail::nan_d();
        }
        tmp1(e) = r;
      });
  exec.fence();

  // :1776, :1797-1799, ST = ZERO then the gather in IE order.
  Kokkos::parallel_for(
      "pdlib_fsfct2_st2", Policy(exec, 0, npa), KOKKOS_LAMBDA(const int ip) {
        double acc = 0.0;
        for (int j = cell_off(ip); j < cell_off(ip + 1); ++j) {
          const int e = ie_cell(j) - 1;
          acc = acc + theta_ace(pos_cell(j) - 1, e) * tmp1(e);
        }
        st(ip) = acc;
      });
  exec.fence();

  // :1802-1807 and :1809 (AC = REAL(U), a copy). kRef1 adds :1805: AC(IP)
  // still holds the value before :1809. REFPARS(3) .LT. 0.5 is tested once,
  // as the object does (seta before the loop); an if constexpr inside an
  // extended lambda cannot capture under nvcc.
  const int keep_ac = (kRef1 && refpars3 < static_cast<Real>(0.5f)) ? 1 : 0;
  Kokkos::parallel_for(
      "pdlib_fsfct2_update", Policy(exec, 0, npa), KOKKOS_LAMBDA(const int ip) {
        const double x = static_cast<double>(ul(ip)) -
                         (static_cast<double>(dtsi(ip)) * st(ip)) *
                             static_cast<double>(1 - static_cast<int>(iobpa(ip)));
        const double v = (max_ab(x, 0.0) * static_cast<double>(iobpd(ith0, ip))) *
                         static_cast<double>(iobdp(ip));
        Real un = static_cast<Real>(v);
        if (keep_ac != 0 && iobpd(ith0, ip) == 0 && iobpa(ip) == 0) un = ac(ip);
        u(ip) = un;
        ac(ip) = un;
      });
  exec.fence();

  // :1820-1850, serial in IBI order (header).
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
      "pdlib_fsfct2_boundary", Policy(exec, 0, 1), KOKKOS_LAMBDA(const int) {
        Real rd1;
        Real rd2 = rd20;  // :1822
        if (rd2 > kRd2Floor) {
          // :1821, DT*REAL(ITER-IT), / REAL(ITER), subtracted from RD10.
          rd1 = rd10 - (dt * static_cast<Real>(iter - it)) / static_cast<Real>(iter);
          rd2 = min_ab(max_ab(rd1 / rd2, kZero), static_cast<Real>(1.0f));  // :1824
          rd1 = static_cast<Real>(1.0f) - rd2;                               // :1825
        } else {
          rd1 = kZero;                    // :1827
          rd2 = static_cast<Real>(1.0f);  // :1828
        }
        for (int ibi = 1; ibi <= nbi; ++ibi) {
          const int isea = isbpi(ibi - 1);
          const int ip_glob = mapsf(isea - 1, 0);  // :1838
          const int jx = ipgl_npa(ip_glob - 1);    // :1839
          if (jx > 0) {
            // :1841, (RD1*BBPI0 + RD2*BBPIN) * CLATS(ISBPI(IBI)).
            ac(jx - 1) = (rd1 * bbpi0(isp0, ibi) + rd2 * bbpin(isp0, ibi)) * clats(isea);
          }
        }
      });
  exec.fence();
}

/// PDLIB_W3XYPFSFCT2(ISP, C, LCALC, RD10, RD20, DT, AC), the whole routine.
/// allreduce_min(Real dtmax) -> Real is the shim's MPI_ALLREDUCE of :1700;
/// exchange(ac) is its PDLIB_exchange1DREAL(AC) of :1863. Both are host
/// callables, called with every kernel before them fenced; a device-resident
/// AC must be brought to the host and back by the exchange callable.
/// kRef1 = true is the routine compiled with W3_REF1; refpars3 is REFPARS(3) of
/// the grid (w3gdatmd.F90:726), read only then.
template <bool kRef1 = false, class ExecSpace, class AllreduceMin, class Exchange>
void w3xypfsfct2(const ExecSpace& exec, const PdlibGeometry<typename ExecSpace::memory_space>& g,
                 const PdlibState<typename ExecSpace::memory_space>& s,
                 const PdlibBoundary<typename ExecSpace::memory_space>& b,
                 const Fsfct2Work<typename ExecSpace::memory_space>& w,
                 const Fsfct2TriaView<typename ExecSpace::memory_space>& tria, int isp,
                 const Fsfct2CView<typename ExecSpace::memory_space>& c, bool lcalc, Real rd10,
                 Real rd20, Real dt, const Fsfct2AcView<typename ExecSpace::memory_space>& ac,
                 AllreduceMin&& allreduce_min, Exchange&& exchange, Real refpars3 = kZero) {
  const int ith = 1 + (isp - 1) % g.nth;  // :1637
  const int ik = 1 + (isp - 1) / g.nth;   // :1638
  using HostInt = Kokkos::View<int, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
  const auto iter_ik_ith = Kokkos::subview(s.iter, ik - 1, ith - 1);

  const Real dtmax = fsfct2_elements_cfl(exec, g, s, w, c, lcalc, dt);  // :1655-1698
  int iter = 0;
  if (lcalc) {
    const Real dtmax_gl = allreduce_min(dtmax);  // :1699-1701, MPI in the shim
    iter = fsn2_iter(dt, dtmax_gl);              // :1702-1710, the same statements as FSN2's
    HostInt h(&iter);
    Kokkos::deep_copy(exec, iter_ik_ith, h);  // ITER(IK,ITH) =
    exec.fence();
  } else {
    HostInt h(&iter);
    Kokkos::deep_copy(exec, h, iter_ik_ith);
    exec.fence();
  }

  fsfct2_dtsi(exec, g, w, dt, iter);  // :1713-1715
  // :1717, the trip count is ITER(IK,ITH) read once.
  for (int it = 1; it <= iter; ++it) {
    fsfct2_substep<kRef1>(exec, g, b, w, tria, isp, ith, it, iter, rd10, rd20, dt, ac, refpars3);
    exchange(ac);  // :1863, PDLIB_exchange1DREAL(AC) in the shim
  }
}

}  // namespace ww::pdlib
