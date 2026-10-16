// kokkos/src/ww_kokkos/pdlib_fspsi2.hpp
// PDLIB_W3XYPFSPSI2, ported to Kokkos: explicit PSI-scheme fluctuation-splitting
// advection of one spectral component ISP on the unstructured (PDLIB) mesh of
// this MPI rank. Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3profsmd_pdlib.F90:1202-1502 at NOAA-EMC/WW3
// 03adda91 (A. Roland, M. Dutour-Sikiric), switch PDLIB of
// regtests/ww3_tp2.17/input/switch_PDLIB; called through PDLIB_W3XYPUG. The fork
// WW3/ (c9a6a0c6) has the routine byte for byte (v, diff, 2026-10-16). That
// switch is kRef1 = false; regtests/ww3_tp2.17/input_deep/switch_PDLIB and
// NOAA's switch_NWS_rwps add REF1, kRef1 = true. W3_S (:1289, :1314, tracing)
// and W3_DEBUGSOLVER are in none of those switches and are not ported; IP_glob
// of :1378 is dead.
//
// W3_REF1, the template flag kRef1 of fspsi2_substep and pdlib_w3xypfspsi2,
// read against the -O3 object and GIMPLE of w3profsmd_pdlib.F90 built with a
// copy of input_deep/switch_PDLIB (CMake Release, gfortran 16.2, 2026-10-16):
//   :1255-1260  USE REFPARS, NX: REFPARS(3) is the argument refpars3, read
//             only when kRef1.
//   :1292-1294, :1327-1329  IOBPDR(NX) = (1-IOBP_LOC)*(1-IOBPD_LOC(ITH,:)): a
//             local never read. The REF1 object keeps the malloc, the store
//             loop (NX trips) and the free, and nothing reads the buffer: not
//             translated, no value depends on it. With NX > npa it reads
//             IOBP_LOC and IOBPD_LOC past their extent npa ⚠ (WW3's, harmless
//             to the results).
//   :1438-1440  after :1437, IF (REFPARS(3) < 0.5 .AND. IOBPD_LOC(ITH,IP) == 0
//             .AND. IOBPA_LOC(IP) == 0) U(IP) = AC(IP), the AC before :1442.
//             The object tests 0.5 > REFPARS(3) (comiss, seta: false for a
//             NaN) and (IOBPA | IOBPD) == 0 and takes AC(IP). REF1 stops the
//             vectorisation of :1437 (two scalar copies, span 1 and strided,
//             with the same operations: mulss, maxss %0,d, then *IOBPD and
//             *IOBDP in REAL, element-wise as the vector lanes were). The C++
//             computes :1437 and then overrides it, as the source orders it.
//
// MPI stays in Fortran. The launchers stop where the Fortran calls MPI, and the
// shim makes the calls between them, in this order:
//   :1386  MPI_ALLREDUCE(DTMAX, MPI_MIN, MPI_COMM_WCMP), only when LCALC: between
//          fspsi2_lcalc_local (returns this rank's DTMAX) and fspsi2_set_iter
//          (takes DTMAX_GL);
//   :1488  PDLIB_exchange1DREAL(AC), once per sub-step IT = 1..ITER(IK,ITH),
//          after fspsi2_substep(IT) and before fspsi2_substep(IT+1).
// pdlib_w3xypfspsi2 at the end runs the whole routine in that order with the two
// calls passed in as callables.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3profsmd_pdlib.F90 (WW3's CMake Release flags, switch_PDLIB; the function's
// disassembly is identical to the CMake object's):
//   * every variable is REAL (rkind = 4, PDLIB/yowdatapool.F90:48); double
//     exactly where the object computes in double: FL111..FL312 (2.d0*FL+FL,
//     :1359-1364), BETAHAT and the BET1 clamp (0.5d0, 1.d0, :1416-1421),
//     DTMAXEXP and CFLXYMAX (DBLE, :1380, :1383), DTSI (:1400);
//   * gfortran narrowed two DBLE statements to float, and so does this file:
//     NM = -(1.0f/MIN(...)) (:1351, divss) and CFLXY = DT/DTMAX_GL (:1388, divss);
//     both give the same bits as the double form (one correctly rounded op);
//   * SUM and DOT_PRODUCT are in-order sums that start from +0.0, kept as
//     (0 + x1) + x2 + x3 (:1351, :1410-1411);
//   * BET1 = THETA_L/FT is emitted as (-THETA_L)/(-FT) and FT*BET1 as BET1*FT
//     (:1414, :1422); written that way (equal bits, NaN sign aside);
//   * MOD(CFLXY,1.0d0) is inline x87 fprem (exact), NINT is lroundf (:1389-1391);
//   * every MIN/MAX follows the emitted minss/maxss/minsd/maxsd: a<b?a:b with
//     the accumulated value as a; MIN(BETAHAT,1.d0-BETAHAT,1.d0) is emitted as
//     MIN(MIN(BETAHAT,1),1-BETAHAT);
//   * constants: ONESIXTH 0x3e2aaaab, -THR 0x80800000, THR 0x00800000, 1.E10
//     0x501502f9, 10.E10 0x51ba43b7, DBLE(10.E-10) 0x3e112e0be0000000, 0.001
//     0x3a83126f, 0.5d0, 1.d0;
//   * no FMA anywhere (SSE baseline, contraction off). The vectorised loops (the
//     K-value SLP at :1345-1366, DTSI at :1400, the node update at :1437) are
//     element-wise, no reduction, so lanes give the scalar bits.
//
// Summation order. KKSUM(NI) = KKSUM(NI) + KELEM(:,IE) (:1373-1374) and
// ST(NI) = ST(NI) + THETA_L (:1427) add over IE = 1..NE in increasing IE, and
// the object does them one node at a time in that order. Here they are written
// in gather form: one thread per node walks PDLIB_IE_CELL/PDLIB_POS_CELL, which
// list the elements of each node in increasing IE (yowpdlibmain.F90:1478-1514;
// check_cell_adjacency in pdlib_params.hpp verifies it), and adds from +0.0. Each
// node receives the same additions in the same order, so the bits are the
// Fortran's on any execution space, with no atomics. The per-element values
// (THETA_L) are written to work.theta first by a parallel loop over elements.
//
// Two loops stay serial (one thread): the LCALC loop over IP (:1377-1384),
// because DTMAXEXP carries from the last node that set it into CFLXYMAX of the
// following nodes and the MIN chain of DTMAX is order dependent for NaN; and the
// boundary loop over IBI (:1462-1476), where two IBI on the same node would keep
// the last store.
#pragma once

#include <Kokkos_Core.hpp>

#include <cmath>
#include <cstdint>

#include "pdlib_params.hpp"
#include "real.hpp"

namespace ww::pdlib {

/// The automatic arrays of PDLIB_W3XYPFSPSI2 (:1303-1309), plus theta, which
/// holds THETA_L of every element so the gather can add it in IE order, and
/// dtmax, the 0-d result of the LCALC loop. LayoutLeft, Fortran extents:
///   kelem(k-1, ie-1) = KELEM(k,IE), (3, NE);   flall, theta likewise
///   nm(ie-1) = NM(IE);   dtsi, u, st, kksum: (npa)
template <class MemSpace>
struct Fspsi2Work {
  using memory_space = MemSpace;
  using W1 = Kokkos::View<Real*, Kokkos::LayoutLeft, MemSpace>;
  using W2 = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;
  W2 kelem, flall, theta;
  W1 nm, dtsi, u, st, kksum;
  Kokkos::View<Real, MemSpace> dtmax;
};

template <class MemSpace>
Fspsi2Work<MemSpace> make_fspsi2_work(int ne, int npa) {
  Fspsi2Work<MemSpace> w;
  w.kelem = typename Fspsi2Work<MemSpace>::W2("fspsi2_kelem", 3, ne);
  w.flall = typename Fspsi2Work<MemSpace>::W2("fspsi2_flall", 3, ne);
  w.theta = typename Fspsi2Work<MemSpace>::W2("fspsi2_theta", 3, ne);
  w.nm = typename Fspsi2Work<MemSpace>::W1("fspsi2_nm", ne);
  w.dtsi = typename Fspsi2Work<MemSpace>::W1("fspsi2_dtsi", npa);
  w.u = typename Fspsi2Work<MemSpace>::W1("fspsi2_u", npa);
  w.st = typename Fspsi2Work<MemSpace>::W1("fspsi2_st", npa);
  w.kksum = typename Fspsi2Work<MemSpace>::W1("fspsi2_kksum", npa);
  w.dtmax = Kokkos::View<Real, MemSpace>("fspsi2_dtmax");
  return w;
}

namespace fspsi2_detail {
// The emitted minss/maxss (and sd) of AT&T `min b,a`: a<b?a:b, a>b?a:b. A NaN
// or a +0/-0 tie gives b. Each call passes a as the register the object keeps.
KOKKOS_INLINE_FUNCTION Real min_ab(Real a, Real b) { return (a < b) ? a : b; }
KOKKOS_INLINE_FUNCTION Real max_ab(Real a, Real b) { return (a > b) ? a : b; }
KOKKOS_INLINE_FUNCTION double min_ab(double a, double b) { return (a < b) ? a : b; }
KOKKOS_INLINE_FUNCTION double max_ab(double a, double b) { return (a > b) ? a : b; }

/// FL*11 = 2.d0*FL1+FL2 in double, stored to REAL (:1359-1364).
KOKKOS_INLINE_FUNCTION Real two_plus(Real a, Real b) {
  return static_cast<Real>(2.0 * static_cast<double>(a) + static_cast<double>(b));
}

/// BET1(i) = MAX(ZERO, MIN(BETAHAT(i), 1.d0-BETAHAT(j), 1.d0)) (:1419-1421) as
/// emitted: minsd 1,bh; minsd (1-bhj),m; maxsd 0,m; cvtsd2ss.
KOKKOS_INLINE_FUNCTION Real bet_clamp(Real bhi, Real bhj) {
  double m = min_ab(static_cast<double>(bhi), 1.0);
  m = min_ab(m, 1.0 - static_cast<double>(bhj));
  m = max_ab(m, 0.0);
  return static_cast<Real>(m);
}
}  // namespace fspsi2_detail

/// :1338-1367, K-values, N-values and Simpson weights of element ie (1-based).
template <class Geo, class CView, class Work>
KOKKOS_INLINE_FUNCTION void fspsi2_kvalues_elem(int ie, const Geo& g, const CView& c,
                                                const Work& w) {
  using fspsi2_detail::max_ab;
  using fspsi2_detail::min_ab;
  using fspsi2_detail::two_plus;
  const int e = ie - 1;
  const int i1 = g.ine(0, e) - 1;
  const int i2 = g.ine(1, e) - 1;
  const int i3 = g.ine(2, e) - 1;
  const Real ien1 = g.ien(0, e), ien2 = g.ien(1, e), ien3 = g.ien(2, e);
  const Real ien4 = g.ien(3, e), ien5 = g.ien(4, e), ien6 = g.ien(5, e);
  // :1345-1346, ONESIXTH*((C1+C2)+C3)
  const Real lam1 = ((c(i1, 0) + c(i2, 0)) + c(i3, 0)) * kOneSixth;
  const Real lam2 = ((c(i1, 1) + c(i2, 1)) + c(i3, 1)) * kOneSixth;
  const Real k1 = lam1 * ien1 + lam2 * ien2;  // :1347-1349
  const Real k2 = lam1 * ien3 + lam2 * ien4;
  const Real k3 = lam1 * ien5 + lam2 * ien6;
  // :1351, SUM(MIN(ZERO,KTMP)) from +0.0, minss 0,k (k<0?k:0), then minss -THR,s.
  Real s = kZero + min_ab(k1, kZero);
  s = s + min_ab(k2, kZero);
  s = s + min_ab(k3, kZero);
  w.nm(e) = -(Real(1) / min_ab(s, -kThr));
  w.kelem(0, e) = max_ab(k1, kZero);  // :1352, maxss 0,k
  w.kelem(1, e) = max_ab(k2, kZero);
  w.kelem(2, e) = max_ab(k3, kZero);
  const Real fl11 = c(i2, 0) * ien1 + c(i2, 1) * ien2;  // :1353-1358
  const Real fl12 = c(i3, 0) * ien1 + c(i3, 1) * ien2;
  const Real fl21 = c(i3, 0) * ien3 + c(i3, 1) * ien4;
  const Real fl22 = c(i1, 0) * ien3 + c(i1, 1) * ien4;
  const Real fl31 = c(i1, 0) * ien5 + c(i1, 1) * ien6;
  const Real fl32 = c(i2, 0) * ien5 + c(i2, 1) * ien6;
  const Real fl111 = two_plus(fl11, fl12);  // :1359-1364
  const Real fl112 = two_plus(fl12, fl11);
  const Real fl211 = two_plus(fl21, fl22);
  const Real fl212 = two_plus(fl22, fl21);
  const Real fl311 = two_plus(fl31, fl32);
  const Real fl312 = two_plus(fl32, fl31);
  w.flall(0, e) = fl311 + fl212;  // :1365-1367
  w.flall(1, e) = fl111 + fl312;
  w.flall(2, e) = fl211 + fl112;
}

/// :1409-1426, THETA_L of element ie (1-based) from U; :1427 is the gather.
template <class Geo, class Work>
KOKKOS_INLINE_FUNCTION void fspsi2_theta_elem(int ie, const Geo& g, const Work& w) {
  using fspsi2_detail::bet_clamp;
  const int e = ie - 1;
  const Real u1 = w.u(g.ine(0, e) - 1);
  const Real u2 = w.u(g.ine(1, e) - 1);
  const Real u3 = w.u(g.ine(2, e) - 1);
  const Real f1 = w.flall(0, e), f2 = w.flall(1, e), f3 = w.flall(2, e);
  // :1410, FT = -ONESIXTH*DOT_PRODUCT(U(NI),FLALL(:,IE)); negft is -FT.
  const Real negft = (((u1 * f1 + kZero) + u2 * f2) + u3 * f3) * kOneSixth;
  Real th1 = kZero, th2 = kZero, th3 = kZero;
  if (Kokkos::fabs(negft) > kZero) {  // :1413, ABS(FT) .GT. 0.0d0
    const Real k1 = w.kelem(0, e), k2 = w.kelem(1, e), k3 = w.kelem(2, e);
    // :1411, NM*(DOT_PRODUCT(KELEM,U(NI)) - FT)
    const Real dku = ((k1 * u1 + kZero) + k2 * u2) + k3 * u3;
    const Real utilde = w.nm(e) * (negft + dku);
    th1 = (u1 - utilde) * k1;  // :1412
    th2 = (u2 - utilde) * k2;
    th3 = (u3 - utilde) * k3;
    Real b1 = (-th1) / negft;  // :1414, THETA_L/FT
    Real b2 = (-th2) / negft;
    Real b3 = (-th3) / negft;
    if (b1 < kZero || b2 < kZero || b3 < kZero) {  // :1415
      const Real ft = -negft;
      const double d1 = static_cast<double>(b1);
      const double d2 = static_cast<double>(b2);
      const double d3 = static_cast<double>(b3);
      const Real bh1 = static_cast<Real>(d1 + d2 * 0.5);  // :1416-1418
      const Real bh2 = static_cast<Real>(d2 + d3 * 0.5);
      const Real bh3 = static_cast<Real>(d3 + d1 * 0.5);
      b1 = bet_clamp(bh1, bh2);  // :1419-1421
      b2 = bet_clamp(bh2, bh3);
      b3 = bet_clamp(bh3, bh1);
      th1 = b1 * ft;  // :1422
      th2 = b2 * ft;
      th3 = b3 * ft;
    }
  }
  w.theta(0, e) = th1;
  w.theta(1, e) = th2;
  w.theta(2, e) = th3;
}

/// :1369-1385 before the MPI_ALLREDUCE. Fills KKSUM, updates CFLXYMAX(1:npa)
/// and returns this rank's DTMAX. Call only when LCALC.
template <class ExecSpace>
Real fspsi2_lcalc_local(const ExecSpace& exec,
                        const PdlibGeometry<typename ExecSpace::memory_space>& g,
                        const PdlibState<typename ExecSpace::memory_space>& state, Real dt,
                        const Fspsi2Work<typename ExecSpace::memory_space>& w) {
  using Policy = Kokkos::RangePolicy<ExecSpace>;
  const auto geo = g;
  const auto kelem = w.kelem;
  const auto kksum = w.kksum;
  // :1371-1375 in gather form, increasing IE per node from +0.0.
  Kokkos::parallel_for(
      "fspsi2_kksum", Policy(exec, 0, geo.npa), KOKKOS_LAMBDA(const int p) {
        Real s = kZero;
        for (int j = geo.cell_off(p); j < geo.cell_off(p + 1); ++j)
          s = s + kelem(geo.pos_cell(j) - 1, geo.ie_cell(j) - 1);
        kksum(p) = s;
      });
  // :1376-1384, serial: DTMAXEXP carries across IP.
  const auto cflxymax = state.cflxymax;
  const auto dtmax_out = w.dtmax;
  const int npa = geo.npa;
  const int fsbccfl = geo.fsbccfl;
  const double dtd = static_cast<double>(dt);
  Kokkos::parallel_for(
      "fspsi2_lcalc", Policy(exec, 0, 1), KOKKOS_LAMBDA(const int) {
        using fspsi2_detail::max_ab;
        using fspsi2_detail::min_ab;
        Real dtmax = kDtmaxInit;        // :1325
        Real dtmaxexp = kDtmaxExpInit;  // :1376
        for (int p = 0; p < npa; ++p) {
          if (geo.iobp_loc(p) == 1 || fsbccfl != 0) {  // :1379
            // :1380, MAX(DBLE(10.E-10), KKSUM*IOBDP_LOC) is maxsd 1e-9,p.
            const double prod =
                static_cast<double>(kksum(p) * static_cast<Real>(geo.iobdp_loc(p)));
            dtmaxexp =
                static_cast<Real>(static_cast<double>(geo.si(p)) / max_ab(prod, kSiFloor));
            dtmax = min_ab(dtmax, dtmaxexp);  // :1381, minss e,dtmax
          }
          // :1383, maxsd cfl,(DT/E) on the path taken when IOBDP_LOC is
          // contiguous (span 1); the strided copy has the operands swapped,
          // which differs only for a NaN.
          const double r = dtd / static_cast<double>(dtmaxexp);
          cflxymax(p) = static_cast<Real>(max_ab(r, static_cast<double>(cflxymax(p))));
        }
        dtmax_out() = dtmax;
      });
  exec.fence();
  Real dtmax = 0;
  Kokkos::deep_copy(dtmax, w.dtmax);
  return dtmax;
}

/// :1387-1396 on the host: ITER(IK,ITH) from DTMAX_GL, the result of the
/// MPI_ALLREDUCE of :1386.
inline int fspsi2_iter_value(Real dt, Real dtmax_gl) {
  const Real cflxy = dt / dtmax_gl;  // :1388, divss
  // :1389, ABS(MOD(CFLXY,1.0d0)): fprem on the widened float, exact.
  const Real rest = std::fabs(static_cast<Real>(std::fmod(static_cast<double>(cflxy), 1.0)));
  const int n = static_cast<int>(std::lroundf(cflxy));  // :1391, NINT
  const int an = (n < 0) ? -n : n;
  if (rest < kThr) return an;                             // :1390-1391
  if (rest > kThr && static_cast<double>(rest) < 0.5) return an + 1;  // :1392-1393
  return an;                                               // :1394-1395
}

/// Stores ITER(IK,ITH) (1-based ik, ith) and returns it.
template <class ExecSpace>
int fspsi2_set_iter(const ExecSpace& exec,
                    const PdlibState<typename ExecSpace::memory_space>& state, int ik, int ith,
                    Real dt, Real dtmax_gl) {
  const int niter = fspsi2_iter_value(dt, dtmax_gl);
  Kokkos::deep_copy(exec, Kokkos::subview(state.iter, ik - 1, ith - 1), niter);
  exec.fence();
  return niter;
}

/// :1399-1401, DTSI = DBLE(DT)/DBLE(ITER)/PDLIB_SI in double, stored to REAL.
template <class ExecSpace>
void fspsi2_dtsi(const ExecSpace& exec, const PdlibGeometry<typename ExecSpace::memory_space>& g,
                 Real dt, int niter, const Fspsi2Work<typename ExecSpace::memory_space>& w) {
  const double q = static_cast<double>(dt) / static_cast<double>(niter);
  const auto si = g.si;
  const auto dtsi = w.dtsi;
  Kokkos::parallel_for(
      "fspsi2_dtsi", Kokkos::RangePolicy<ExecSpace>(exec, 0, g.npa),
      KOKKOS_LAMBDA(const int p) { dtsi(p) = static_cast<Real>(q / static_cast<double>(si(p))); });
}

/// :1338-1367 for every element.
template <class ExecSpace, class CView>
void fspsi2_kvalues(const ExecSpace& exec, const PdlibGeometry<typename ExecSpace::memory_space>& g,
                    const CView& c, const Fspsi2Work<typename ExecSpace::memory_space>& w) {
  const auto geo = g;
  const auto work = w;
  Kokkos::parallel_for(
      "fspsi2_kvalues", Kokkos::RangePolicy<ExecSpace>(exec, 1, geo.ne + 1),
      KOKKOS_LAMBDA(const int ie) { fspsi2_kvalues_elem(ie, geo, c, work); });
}

/// One sub-step IT (1..niter) of :1405-1476, up to the PDLIB_exchange1DREAL(AC)
/// of :1488, which the caller makes after this returns. kRef1 adds :1439, which
/// reads refpars3 = REFPARS(3).
template <bool kRef1 = false, class ExecSpace, class AcView>
void fspsi2_substep(const ExecSpace& exec, const PdlibGeometry<typename ExecSpace::memory_space>& g,
                    const PdlibBoundary<typename ExecSpace::memory_space>& bnd, int isp, int it,
                    int niter, Real dt, Real rd10, Real rd20, const AcView& ac,
                    const Fspsi2Work<typename ExecSpace::memory_space>& w,
                    [[maybe_unused]] Real refpars3 = kZero) {
  using Policy = Kokkos::RangePolicy<ExecSpace>;
  const auto geo = g;
  const auto work = w;
  const int ith = 1 + (isp - 1) % geo.nth;
  Kokkos::deep_copy(exec, w.u, ac);  // :1405, U = DBLE(AC), REAL to REAL
  Kokkos::parallel_for(
      "fspsi2_theta", Policy(exec, 1, geo.ne + 1),
      KOKKOS_LAMBDA(const int ie) { fspsi2_theta_elem(ie, geo, work); });
  // :1406 ST = ZERO, :1427 ST(NI) += THETA_L in increasing IE, then :1436-1441
  // and :1442 AC = REAL(U), node by node.
  if constexpr (!kRef1) {
    Kokkos::parallel_for(
        "fspsi2_update", Policy(exec, 0, geo.npa), KOKKOS_LAMBDA(const int p) {
          Real s = kZero;
          for (int j = geo.cell_off(p); j < geo.cell_off(p + 1); ++j)
            s = s + work.theta(geo.pos_cell(j) - 1, geo.ie_cell(j) - 1);
          work.st(p) = s;
          // :1437, (DTSI*ST)*(1-IOBPA); maxss 0,d; *IOBPD; *IOBDP.
          const Real d =
              work.u(p) - (work.dtsi(p) * s) * static_cast<Real>(1 - int(geo.iobpa_loc(p)));
          const Real m = fspsi2_detail::max_ab(d, kZero);
          const Real up = (m * static_cast<Real>(geo.iobpd_loc(ith - 1, p))) *
                          static_cast<Real>(geo.iobdp_loc(p));
          work.u(p) = up;
          ac(p) = up;
        });
  } else {
    // The same statements, then :1439 (header). A lambda of its own: a name in
    // a discarded if-constexpr branch is still captured, and the kRef1 = false
    // closure above must stay as it was.
    Kokkos::parallel_for(
        "fspsi2_update", Policy(exec, 0, geo.npa), KOKKOS_LAMBDA(const int p) {
          Real s = kZero;
          for (int j = geo.cell_off(p); j < geo.cell_off(p + 1); ++j)
            s = s + work.theta(geo.pos_cell(j) - 1, geo.ie_cell(j) - 1);
          work.st(p) = s;
          const Real d =
              work.u(p) - (work.dtsi(p) * s) * static_cast<Real>(1 - int(geo.iobpa_loc(p)));
          const Real m = fspsi2_detail::max_ab(d, kZero);
          Real up = (m * static_cast<Real>(geo.iobpd_loc(ith - 1, p))) *
                    static_cast<Real>(geo.iobdp_loc(p));
          // REFPARS(3) .LT. 0.5 (REAL); AC(IP) still holds the value before :1442.
          if (refpars3 < static_cast<Real>(0.5f) && geo.iobpd_loc(ith - 1, p) == 0 &&
              geo.iobpa_loc(p) == 0)
            up = ac(p);
          work.u(p) = up;
          ac(p) = up;
        });
  }
  if (bnd.flbpi == 0) return;  // :1446
  // :1447-1456 on the host. ITER(IK,ITH) re-read here is niter.
  Real rd1 = rd10 - (dt * static_cast<Real>(niter - it)) / static_cast<Real>(niter);
  Real rd2 = rd20;
  if (rd2 > kRd2Floor) {
    // MIN(1.,MAX(0.,RD1/RD2)) as maxss 0,r then minss 1,m.
    rd2 = fspsi2_detail::min_ab(fspsi2_detail::max_ab(rd1 / rd2, Real(0)), Real(1));
    rd1 = Real(1) - rd2;
  } else {
    rd1 = Real(0);
    rd2 = Real(1);
  }
  const auto b = bnd;
  const int nbi = bnd.nbi;
  // :1462-1476, serial over IBI: the last IBI on a node wins, as in the Fortran.
  Kokkos::parallel_for(
      "fspsi2_boundary", Policy(exec, 0, 1), KOKKOS_LAMBDA(const int) {
        for (int ibi = 1; ibi <= nbi; ++ibi) {
          const int isea = b.isbpi(ibi - 1);
          const int ip_glob = geo.mapsf(isea - 1, 0);
          const int jx = geo.ipgl_npa(ip_glob - 1);
          if (jx > 0)
            ac(jx - 1) = (rd1 * b.bbpi0(isp - 1, ibi) + rd2 * b.bbpin(isp - 1, ibi)) *
                         geo.clats(isea);
        }
      });
}

/// PDLIB_W3XYPFSPSI2(ISP, C, LCALC, RD10, RD20, DT, AC) in the Fortran's order.
/// allreduce_min(Real) -> Real performs :1386 and exchange(ac) performs :1488;
/// both are called after a fence, on the host. c(ip-1, j-1) = C(IP,J), (npa, 2);
/// ac(ip-1) = AC(IP), (npa). kRef1 = true is the routine compiled with W3_REF1;
/// refpars3 is REFPARS(3) of the grid (w3gdatmd.F90:726), read only then.
template <bool kRef1 = false, class ExecSpace, class CView, class AcView, class AllreduceMin,
          class Exchange>
void pdlib_w3xypfspsi2(const ExecSpace& exec, int isp, const CView& c, bool lcalc, Real rd10,
                       Real rd20, Real dt, const AcView& ac,
                       const PdlibGeometry<typename ExecSpace::memory_space>& g,
                       const PdlibState<typename ExecSpace::memory_space>& state,
                       const PdlibBoundary<typename ExecSpace::memory_space>& bnd,
                       const Fspsi2Work<typename ExecSpace::memory_space>& w,
                       AllreduceMin&& allreduce_min, Exchange&& exchange,
                       Real refpars3 = kZero) {
  const int ith = 1 + (isp - 1) % g.nth;  // :1323-1324
  const int ik = 1 + (isp - 1) / g.nth;
  fspsi2_kvalues(exec, g, c, w);
  int niter = 0;
  if (lcalc) {
    const Real dtmax = fspsi2_lcalc_local(exec, g, state, dt, w);
    const Real dtmax_gl = allreduce_min(dtmax);  // :1386
    niter = fspsi2_set_iter(exec, state, ik, ith, dt, dtmax_gl);
  } else {
    exec.fence();
    Kokkos::deep_copy(niter, Kokkos::subview(state.iter, ik - 1, ith - 1));
  }
  fspsi2_dtsi(exec, g, dt, niter, w);
  for (int it = 1; it <= niter; ++it) {  // :1403, the bound read once
    fspsi2_substep<kRef1>(exec, g, bnd, isp, it, niter, dt, rd10, rd20, ac, w, refpars3);
    exec.fence();
    exchange(ac);  // :1488
  }
}

}  // namespace ww::pdlib
