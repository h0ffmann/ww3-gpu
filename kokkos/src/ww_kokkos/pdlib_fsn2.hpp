// kokkos/src/ww_kokkos/pdlib_fsn2.hpp
// PDLIB_W3XYPFSN2, ported to Kokkos: explicit N-scheme (fluctuation splitting)
// advection in geographical space of one spectral component on the
// unstructured mesh of this MPI rank. Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3profsmd_pdlib.F90:868-1200 at NOAA-EMC/WW3
// 03adda91 (identical in the WW3/ fork at c9a6a0c6); authors A. Roland and
// M. Dutour-Sikiric. Called by PDLIB_W3XYPUG (:837) when FSN. Compiled as
// regtests/ww3_tp2.17/input/switch_PDLIB builds it (kRef1 = false), and as
// regtests/ww3_tp2.17/input_deep/switch_PDLIB and NOAA's switch_NWS_rwps build
// it with REF1 (kRef1 = true). W3_S (:913, :956, :988, tracing) and
// W3_DEBUGSOLVER are in none of those switches and are not ported.
// The mesh and module arrays are pdlib_params.hpp.
//
// W3_REF1, the template flag kRef1 of fsn2_substep and w3xypfsn2, read against
// the -O3 object and GIMPLE of w3profsmd_pdlib.F90 built with a copy of
// input_deep/switch_PDLIB (WW3's CMake Release, gfortran 16.2, 2026-10-16):
//   :923-928  USE REFPARS, NX: REFPARS(3) is the argument refpars3, read only
//             when kRef1.
//   :959-961, :1002-1004  IOBPDR(NX) = (1-IOBP_LOC)*(1-IOBPD_LOC(ITH,:)): a
//             local the routine never reads. The REF1 object keeps the malloc,
//             the store loop (NX trips, LHS extent) and the free, and nothing
//             reads the buffer, so it changes no value and is not translated.
//             With NX > npa (more than one rank) the loop reads IOBP_LOC and
//             IOBPD_LOC past their extent npa (:596-598) ⚠, a WW3 defect that
//             cannot change a result.
//   :1125-1127  after :1124, IF (REFPARS(3) < 0.5 .AND. IOBPD_LOC(ITH,IP) == 0
//             .AND. IOBPA_LOC(IP) == 0) U(IP) = AC(IP): the old AC, read before
//             the store of :1134. The object tests 0.5 > REFPARS(3) (comiss,
//             seta: false for a NaN) and (IOBPA | IOBPD) == 0, and takes AC(IP)
//             instead of the :1124 value; :1124's arithmetic is the same as
//             without REF1 (mulss, maxss %0,x, cvtss2sd, two mulsd, cvtsd2ss),
//             in both loop copies (span 1 and strided). The C++ computes :1124,
//             then overrides it, as the source orders it: the same bits.
//
// The MPI calls stay in Fortran. The routine has two, and the driver
// w3xypfsn2() below stops at each and calls back into the shim:
//   :1062-1064  only when LCALC: FIN(1)=DTMAX; MPI_ALLREDUCE(FIN, FOUT, 1,
//               rtype=MPI_REAL4, MPI_MIN, MPI_COMM_WCMP); DTMAX_GL=FOUT(1).
//               Between fsn2_elements_cfl() and fsn2_set_iter().
//   :1188       PDLIB_exchange1DREAL(AC) (PDLIB/yowexchangeModule.F90:250),
//               once at the end of every sub-step IT = 1..ITER, after the
//               boundary update; the next sub-step starts from the exchanged
//               AC. After each fsn2_substep(), before the next one.
// On one rank the exchange has no neighbour domain and the reduction returns
// DTMAX, but the call sites are the same.
//
// Precision. Every array is REAL (rkind = 4, pdlib_params.hpp); REAL(8) appears
// only where the source writes a double literal or DBLE, and the object keeps
// it there, so double is used exactly at:
//   :1033-1038  FL111.. = 2.d0*FLa+FLb: cvtps2pd, addpd x,x, addpd, cvtpd2ps;
//   :1057       SI / MAX(DBLE(10.E-10), KKSUM*IOBDP): cvtss2sd, maxsd, divsd, cvtsd2ss;
//   :1060       MAX(CFLXYMAX, DBLE(DT)/DTMAXEXP): divsd, maxsd, cvtsd2ss;
//   :1066       MOD(CFLXY, 1.0d0): x87 fprem on CFLXY, cvtsd2ss, then ABS;
//   :1081       DBLE(DT)/DBLE(ITER)/SI: divsd, divsd (divpd when vectorised), cvtsd2ss;
//   :1124       MAX(..)*DBLE(IOBPD)*IOBDP: the MAX in REAL, then two mulsd, cvtsd2ss.
// gfortran narrowed two double expressions to REAL, both exactly (a double
// rounding of one IEEE + - * / of two REALs is innocuous, 53 >= 2*24+2):
//   :1025  -1.D0/MIN(..) is divss 1.0/x then a sign flip (xorps .LC42);
//   :1065  DBLE(DT)/DTMAX_GL is divss.
// The C++ writes the narrowed forms, as the object computes them.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3profsmd_pdlib.F90 (WW3's CMake Release flags, switch_PDLIB; the -fdump
// object disassembles identically to the CMake one): no FMA; libm only
// lroundf (NINT, :1068-1072) and the inline fprem; the products of :1021-1023
// and :1027-1032 are two mulss and one addss each, written in source order
// (the vector lanes commute operands of + and *, which changes no bits).
// Vectorised (fopt-info): :1080-1081 (DTSI, 4 lanes, element-wise divpd) and
// basic-block SLP at :1021, :1026, :1039 (element-wise lanes); no reduction is
// vectorised, so every sum below is the scalar order. MIN/MAX in the operand
// order of the emitted minss/maxss/minsd/maxsd (dest = dest OP src ? dest : src):
//   :1025  minss %0,k: k<0?k:0 per corner; minss -THR,s: s<-THR?s:-THR
//   :1026  maxss/maxps %0,k: k>0?k:0
//   :1057  maxsd floor,p: p>floor?p:floor          :1058 minss e,d: d<e?d:e
//   :1060  maxsd cfl,q: q>cfl?q:cfl (the span-1 loop version that runs; the
//          strided version, for a non-contiguous IOBDP_LOC, has cfl>q?cfl:q)
//   :1124  maxss %0,x: x>0?x:0                     :1149 maxss 0,q; minss 1,t
// The C++ object (openmp-release flags, -ffp-contract=off) has no FMA, the same
// + 0. adds and in-order accumulations, and calls lroundf and the inline fprem
// as the Fortran does.
//
// CUDA (nvcc 12.9, --fmad=false, sm_89, compiled, never run): the PTX has no
// fma, divisions are div.rn, the + 0. adds are kept. nvcc turns each MIN/MAX
// whose second operand is a constant (0, -THR, 1, DBLE(10.E-10)) into
// min/max.f32/f64: the same result for a NaN first operand. For zeros, the PTX
// ISA (§9.7.3.11-12, read 2026-10-16) gives min.f32(-0,+0) = -0 where minss in
// this operand order gives +0; the only such tie is at :1025, whose result goes
// through a + 0. add and later sums that erase the sign, so no bit changes.
// max.f32 gives +0 as maxss does. The MIN/MAX of two variables (:1058, :1060)
// stay selp.
//
// Why the parallel passes give the Fortran's bits.
//   Elements (:1015-1042): each IE writes only KELEM(:,IE), FLALL(:,IE),
//   NM(IE) from C, PDLIB_IEN and constants: no element reads another's output.
//   KKSUM (:1048-1052) and ST (:1108-1113) are scatter-adds, element by element
//   into nodes. For one node IP the Fortran adds into KKSUM(IP) once per element
//   that holds it, in increasing IE, starting from ZERO: ((0+t1)+t2)+...; the
//   other nodes' stores in between do not touch KKSUM(IP). PDLIB_IE_CELL lists
//   the elements of each node in increasing IE (yowpdlibmain.F90:1478-1514;
//   check_cell_adjacency verifies it), so a pass parallel over nodes that
//   starts at 0 and adds the same terms in IE_CELL order performs the same
//   float additions on the same values. No atomics: each node is one thread.
//   ST's term KELEM(k,IE)*(U(NI(k))-UTILDE(IE)) reads U, which nothing writes
//   during that loop, so UTILDE(IE) is computed once per element in a pass
//   before (utilde, a C++ work array) and has the bits the Fortran recomputes
//   per element.
//   Nodes (:1080-1082, :1123-1134): each IP reads and writes only its own entries.
//   Serial: the LCALC node loop (:1053-1061), since DTMAXEXP carries from one
//   IP to the next when IOBP_LOC(IP) /= 1 and DTMAX is a running MIN whose
//   result depends on order for NaN and signed zeros; and the boundary loop
//   (:1162-1175), where two IBI on one node leave the last write. One thread
//   runs each, in the Fortran's order. Kernels on one execution-space instance
//   run in order; the fences make each pass's writes visible to the next.
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

  // :1048-1052, KKSUM = ZERO then the scatter-add, as a gather in IE order.
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

  // :1053-1061, serial (header). IP_glob = iplg(IP) of :1055 is dead.
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
  const Real cflxy = dt / dtmax_gl;  // :1065, divss (narrowed, header)
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
  // gather in IE order (header).
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
    // The same statement, then :1126 (header). A lambda of its own: a name in a
    // discarded if-constexpr branch is still captured, and the kRef1 = false
    // closure above must stay as it was.
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

  // :1145-1176, serial in IBI order (header).
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

/// PDLIB_W3XYPFSN2(ISP, C, LCALC, RD10, RD20, DT, AC), the whole routine.
/// allreduce_min(Real dtmax) -> Real is the shim's MPI_ALLREDUCE of :1063;
/// exchange(ac) is its PDLIB_exchange1DREAL(AC) of :1188. Both are host
/// callables, called with every kernel before them fenced; a device-resident
/// AC must be brought to the host and back by the exchange callable.
/// kRef1 = true is the routine compiled with W3_REF1 (header); refpars3 is
/// REFPARS(3) of the grid (w3gdatmd.F90:726), read only then.
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
