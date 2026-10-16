// kokkos/src/ww_kokkos/pdlib_explicit_block.hpp
// PDLIB_EXPLICIT_BLOCK, ported to Kokkos: the explicit N-scheme for the
// geographic advection of every spectral component on the unstructured mesh of
// this MPI rank, frequency by frequency, all directions at once, with sub-steps
// set by the CFL limit. Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3profsmd_pdlib.F90:6408-6685 at NOAA-EMC/WW3
// 03adda91, switch PDLIB (A. Roland, M. Dutour-Sikiric). Called only by
// PDLIB_W3XYPUG_BLOCK_EXPLICIT (:2881), which passes RD10 = DSEC21(TBPI0,TIME)
// and RD20 = DSEC21(TBPI0,TBPIN) when FLBPI, else 1. and 0. (:2873-2879); that
// is called by W3WAVE with FACY = FACX and LCALC = UGDTUPDATE (w3wavemd.F90:1925).
// The module arrays FLALL1-3, KELEM1-3, NM (NTH,NE,NK), DTSI (NPA), all REAL*8,
// and ITER (NK) (:119-123) are allocated by BLOCK_SOLVER_EXPLICIT_INIT
// (:6743-6746) and keep their values between calls: with LCALC false the
// routine reuses them.
//
// Compile-time variants. The #ifdefs of :6408-6685 are W3_S (STRACE, not ported),
// W3_PDLIB (the USEs, always on here) and W3_REF1 (:6471-6473 USE, :6492-6494
// UOLD, :6642-6644 the restore). Of the 11 switch files with PDLIB under
// regtests/ww3_tp2.17/input*/, regtests/ww3_ufs1.1/input_unstr/ and
// model/bin/switch_NWS_rwps, 6 add REF1 and 1 TRNK, which this routine does not
// test (v, grep -w, 2026-10-16). REF1 is the template flag kRef1.
//
// What it calls.
//   :6513  WAVNU3 (w3dispmd.F90:348-454), out of line in the object
//          (__w3dispmd_MOD_wavnu3): called here as ww::disp::wavnu3 of
//          disp_wavnu3.hpp, a device function, per node. It has no #ifdef and no
//          per-grid state, so there is no hook. ⚠ on a GPU its powf, expf, tanhf,
//          sinhf are libdevice's, not glibc's (disp_wavnu3.hpp).
//   No W3SRCE piece, no W3SPR4, no CALCARRAY_*: the explicit block only advects.
// The calls into Fortran are a Hooks object (see pdlib_explicit_block below); the
// driver stops at each, in this order, per IK:
//   :6599  MPI_ALLREDUCE(FIN,FOUT,1,rtype,MPI_MIN,MPI_COMM_WCMP), only when
//          LCALC; rtype is MPI_REAL4 (PDLIB/yowdatapool.F90:77) on REAL FIN:
//          hooks.allreduce_min(dtmaxout) returns DTMAXGL.
//   :6626  PDLIB_exchange2DREAL(U)                   hooks.exchange_u()
//   then per sub-step IT = 1..ITER(IK), after the update and the boundary:
//   :6670  PDLIB_exchange2DREAL(U)                   hooks.exchange_u()
// U is ExplWork::u (NTH,NPA); the hook exchanges it in place. There is no print.
//
// Order. Every pass is parallel over the index it writes, except:
//   :6588-6596  DTMAX(ITH) is a running minimum over IP and DTMAXOUT is the
//               MINVAL after the last node that passes the test: one thread, IP
//               = 1..NPA in order (expl_dtmax).
//   :6577-6584 (KKSUM) and :6630-6638 (ST) scatter element sums into nodes. The
//               C++ gathers instead: node IP walks the elements that hold it in
//               increasing IE (PDLIB_IE_CELL, the order check_cell_adjacency of
//               pdlib_params.hpp verifies), so each node receives the same terms
//               in the Fortran's order, rounded the same way; no atomics. UTILDE
//               of an element is recomputed by each of its three nodes from the
//               same operands: the same bits.
//   :6660-6666  U(ITH,JX) = boundary value: parallel over ITH, serial over IBI
//               (the last IBI wins, as in the Fortran).
// A kernel here has no reduction and no atomic, so its bits do not depend on the
// execution space or the thread count.
//
// Precision (REAL is float, REAL*8 double). double: KELEM1-3, FLALL1-3, NM,
// DTSI. Everything else float. Mixed operations as the GIMPLE has them:
//   :6544-6546  KELEM = LAMBDAX*IEN + LAMBDAY*IEN in float, stored widened;
//   :6550       NM = -(1/DBLE(MIN(SUM,-THR))), SUM = ((MIN(K1,0)+0)+MIN(K2,0))
//               +MIN(K3,0) in float (the +0 is kept: it turns -0 into +0);
//   :6551-6553  KELEM = DBLE(MAX(K,0));
//   :6563-6568  FL111 = REAL(2*DBLE(FL11) + DBLE(FL12)), double then narrowed;
//   :6570-6572  FLALL = DBLE((FL311+FL212)*ONESIXTH) + KELEM, the sum in double;
//   :6580-6582  KKSUM = REAL(DBLE(KKSUM) + KELEM);
//   :6602       CFLXY = DBLE(DTG)/DTMAXGL into a REAL: the object divides in float
//               (divss), the same bits since a double quotient of floats narrows to
//               the float quotient;
//   :6603       REST = ABS(REAL(fmod(DBLE(CFLXY),1d0))); NINT is lroundf;
//   :6613       DTSI = (DBLE(DTMAXGL)/DBLE(ITER))/DBLE(SI), the first quotient once;
//   :6633       UTILDE = REAL(NM*((FLALL1*U1 + FLALL2*U2) + FLALL3*U3)) in double;
//   :6634-6636  ST = REAL(DBLE(ST) + KELEM*DBLE(U - UTILDE)), U - UTILDE in float;
//   :6641       U = REAL(DBLE(IOBDP)*(MAX(DBLE(U) - (DBLE(ST)*DTSI)*DBLE(1-IOBPA),
//               0)*DBLE(IOBPD))): the operand order of the GIMPLE, the same
//               association as the source.
// Constants: ONESIXTH = 1.0/6.0 as REAL (kOneSixth, 0x3e2aaaab); THR = 1.0E-12
// as REAL (0x2b8cbccc), not W3PARALL's TINY; 1.E10; 0.001 (kRd2Floor);
// HUGE(1.) for MINVAL of an empty DTMAX.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3profsmd_pdlib.F90 (WW3's CMake flags, switch_PDLIB; the -fdump object equals
// the CMake one byte for byte, v), and the REF1 object (switch of
// regtests/ww3_tp2.17/input_deep, same check, v): no FMA; the one libm call is
// lroundf (NINT); fmod is inlined as an x87 fprem loop, exact like the C++'s fmod
// call, so the same bits. MIN/MAX in the operand order of the emitted
// minss/maxss/minps/maxps/maxsd/maxpd (dest = dest OP src ? dest : src; min_ss(a,b)
// below is a < b ? a : b with a the destination):
//   :6550-6553  min_ss(K,0), max_ss(K,0), min_ss(SUM,-THR): vector and scalar alike;
//   :6591       max_ss(KKSUM*IOBDP, THR): vector and scalar alike;
//   :6592       vector body (minps): min_ss(DTMAX, DTMAXEXP); scalar tail (minss
//               with DTMAX from memory): min_ss(DTMAXEXP, DTMAX). The tail is
//               element ITH = NTH when NTH is odd (4-wide, then 2-wide, then one
//               scalar; NTH = 1 all scalar). A NaN or a +0/-0 tie gives a
//               different result in the two paths, so both are kept;
//   :6594       MINVAL: the first DTMAX <= +Inf, then x < lim ? x : lim, NaN if
//               all are NaN (GIMPLE, not a MIN_EXPR);
//   :6641       max_sd(v, 0), vector and scalar alike;
//   :6654       min_ss(max_ss(RD1/RD2, 0), 1).
// The REF1 object has the same roles at every site (v); it computes :6641 only
// where the restore does not hold and stores UOLD there instead.
//
// C++ object (openmp-release flags plus -ffp-contract=off, gcc 16.2): no FMA,
// libm calls fmod and lroundf plus WAVNU3's powf, expf, sinhf; MIN(K,0) is
// cmpnltss/andnps (K < 0 ? K : +0), the others minss/maxss with the operands
// above, the +0 of :6550 kept (v).
// CUDA (nvcc_wrapper, --fmad=false, sm_89, compiled, never run): no fma.rn
// outside WAVNU3 (libdevice); divisions div.rn. nvcc emits min/max.f32 for the
// MIN/MAX against 0 of :6550-6553 and max.f64 for :6641; PTX min(-0,+0) is -0
// where SSE gives +0, harmless at :6550 (the sum turns it into +0), and whether
// max.f64 keeps SSE's +0 for v = -0 at :6641 is not checked ⚠.
//
// What the Fortran leaves undefined (⚠, not reproducible bit for bit):
//   - with LCALC false (UGDTUPDATE is reset at each W3WAVE call, :767, and set on
//     every step with currents, :1188-1210, else on the first step of each call,
//     :973, :1219-1220, and on water-level updates, :1436), the conversions :6623 and :6679 read CGSIG, an automatic array the
//     object mallocs per call and never fills, and with FLBPI :6651 reads
//     DTMAXGL, never assigned. The C++ keeps CGSIG (ExplWork) and DTMAXGL
//     (ExplCarry) from the last call that computed them; the Fortran's are heap
//     or stack contents;
//   - DTMAXOUT (:6594) is assigned only at a node with IOBP_LOC = 1 or FSBCCFL; on
//     a rank with none it keeps its value from the previous IK (the object
//     carries it, a PHI), undefined on the first. ExplCarry keeps it;
//   - with REF1, UOLD (:6493) is never assigned before :6643 reads it. The C++
//     reads ExplWork::uold, zero-filled at allocation and never written.
#pragma once

#include <Kokkos_Core.hpp>

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include "disp_wavnu3.hpp"
#include "pdlib_params.hpp"
#include "real.hpp"

namespace ww::pdlib {

namespace expl_detail {
// SSE MIN/MAX as emitted (dest OP src ? dest : src): a NaN or a +0/-0 tie in
// `a` gives `b`. Each call passes the operands in the order read from the object.
KOKKOS_INLINE_FUNCTION Real min_ss(Real a, Real b) { return (a < b) ? a : b; }
KOKKOS_INLINE_FUNCTION Real max_ss(Real a, Real b) { return (a > b) ? a : b; }
KOKKOS_INLINE_FUNCTION double max_sd(double a, double b) { return (a > b) ? a : b; }
KOKKOS_INLINE_FUNCTION double dbl(Real x) { return static_cast<double>(x); }

// THR = 1.0E-12, a REAL PARAMETER (:6497): 0x2b8cbccc, the GIMPLE's
// 9.999999960041972e-13.
inline constexpr Real kThr = 1.0E-12f;
static_assert(static_cast<double>(kThr) == 9.999999960041972002500187954865396022796630859375e-13,
              "THR = 1.0E-12 as REAL");
// DTMAXEXP = 1.E10, DTMAX = 1.E10 (:6586-6587).
inline constexpr Real kDtmaxInit = 1.E10f;
static_assert(kDtmaxInit == kDtmaxExpInit, "1.E10 as REAL, 0x501502f9");
}  // namespace expl_detail

/// The run-time settings the routine reads: its dummy arguments and module
/// scalars, host values.
struct ExplParams {
  Real facx = 0, facy = 0, dtg = 0;  ///< FACX, FACY, DTG, dummies (VGX, VGY, IMOD unread)
  Real rd10 = 0, rd20 = 0;          ///< RD10, RD20, dummies (:2873-2879)
  bool lcalc = false;               ///< LCALC (UGDTUPDATE)
  bool flcur = false;               ///< W3IDATMD FLCUR
  Real refpars3 = 0;                ///< REFPARS(3), W3GDATMD under W3_REF1
};

/// Arrays this routine reads that PdlibGeometry lacks, LayoutLeft, bases kept.
template <class MemSpace>
struct ExplFields {
  using memory_space = MemSpace;
  using R1 = Kokkos::View<const Real*, Kokkos::LayoutLeft, MemSpace>;
  R1 sig;  ///< SIG(0:NK+1), REAL (w3gdatmd.F90): sig(ik)
  R1 dw;   ///< DW(0:NSEA), REAL (w3adatmd.F90:1004): dw(isea)
  R1 cx;   ///< CX(0:NSEA), REAL (w3adatmd.F90:1008): cx(isea)
  R1 cy;   ///< CY(0:NSEA), REAL: cy(isea)
  /// VA(NSPEC,0:NSEALM), REAL (w3wdatmd.F90:491): va(isp-1, ip), read and written.
  Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace> va;
};

/// The module arrays of :119-123 (BLOCK_SOLVER_EXPLICIT_INIT, :6743-6746).
/// Written when LCALC, read always; they persist between calls.
template <class MemSpace>
struct ExplArrays {
  using memory_space = MemSpace;
  using D3 = Kokkos::View<double***, Kokkos::LayoutLeft, MemSpace>;
  D3 kelem1, kelem2, kelem3;  ///< KELEM1-3(NTH,NE,NK), REAL*8: (ith-1, ie-1, ik-1)
  D3 flall1, flall2, flall3;  ///< FLALL1-3(NTH,NE,NK), REAL*8
  D3 nm;                      ///< NM(NTH,NE,NK), REAL*8
  Kokkos::View<double*, Kokkos::LayoutLeft, MemSpace> dtsi;  ///< DTSI(NPA), REAL*8
  /// ITER(NK), integer: iter(ik-1). A control count, kept on the host.
  Kokkos::View<int*, Kokkos::LayoutLeft, Kokkos::HostSpace> iter;

  static ExplArrays allocate(int nth, int ne, int nk, int npa) {
    ExplArrays a;
    a.kelem1 = D3("expl_kelem1", nth, ne, nk);
    a.kelem2 = D3("expl_kelem2", nth, ne, nk);
    a.kelem3 = D3("expl_kelem3", nth, ne, nk);
    a.flall1 = D3("expl_flall1", nth, ne, nk);
    a.flall2 = D3("expl_flall2", nth, ne, nk);
    a.flall3 = D3("expl_flall3", nth, ne, nk);
    a.nm = D3("expl_nm", nth, ne, nk);
    a.dtsi = Kokkos::View<double*, Kokkos::LayoutLeft, MemSpace>("expl_dtsi", npa);
    a.iter = Kokkos::View<int*, Kokkos::LayoutLeft, Kokkos::HostSpace>("expl_iter", nk);
    return a;
  }
};

/// The Fortran's automatic arrays (:6483-6493), allocated once by the caller and
/// kept between calls (see the header on CGSIG). LayoutLeft: (ith-1, ip-1).
template <class MemSpace>
struct ExplWork {
  using memory_space = MemSpace;
  using R1 = Kokkos::View<Real*, Kokkos::LayoutLeft, MemSpace>;
  using R2 = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;
  R1 ksig, cgsig;      ///< KSIG(NPA), CGSIG(NPA)
  R2 cxx, cyy, kksum;  ///< CXX, CYY, KKSUM (NTH,NPA)
  R2 u, st;            ///< U, ST (NTH,NPA); u is what hooks.exchange_u() exchanges
  R2 uold;             ///< UOLD(NTH,NPA), W3_REF1: read, never written (header)
  R1 dtmax;            ///< DTMAX(NTH)
  Kokkos::View<Real, MemSpace> dtmaxout;  ///< DTMAXOUT, read back after expl_dtmax

  static ExplWork allocate(int nth, int npa) {
    ExplWork w;
    w.ksig = R1("expl_ksig", npa);
    w.cgsig = R1("expl_cgsig", npa);
    w.cxx = R2("expl_cxx", nth, npa);
    w.cyy = R2("expl_cyy", nth, npa);
    w.kksum = R2("expl_kksum", nth, npa);
    w.u = R2("expl_u", nth, npa);
    w.st = R2("expl_st", nth, npa);
    w.uold = R2("expl_uold", nth, npa);
    w.dtmax = R1("expl_dtmax", nth);
    w.dtmaxout = Kokkos::View<Real, MemSpace>("expl_dtmaxout");
    return w;
  }
};

/// Host scalars the Fortran leaves undefined where they are first read (header):
/// kept from one IK, and one call, to the next.
struct ExplCarry {
  Real dtmaxout = 0;  ///< DTMAXOUT
  Real dtmaxgl = 0;   ///< DTMAXGL
};

/// :6512-6514: WAVNU3(SIG(IK), DW(IPLG(IP)), KSIG(IP), CGSIG(IP)), per IP.
template <class ExecSpace>
void expl_wavnu3(const ExecSpace& exec, int ik,
                 const PdlibGeometry<typename ExecSpace::memory_space>& g,
                 const ExplFields<typename ExecSpace::memory_space>& f,
                 const ExplWork<typename ExecSpace::memory_space>& w) {
  const auto iplg = g.iplg;
  const auto sig = f.sig;
  const auto dw = f.dw;
  const auto ksig = w.ksig;
  const auto cgsig = w.cgsig;
  Kokkos::parallel_for(
      "pdlib_expl_wavnu3", Kokkos::RangePolicy<ExecSpace>(exec, 0, g.npa),
      KOKKOS_LAMBDA(const int i) {
        Real k = 0, cg = 0;
        ww::disp::wavnu3(sig(ik), dw(iplg(i)), k, cg);
        ksig(i) = k;
        cgsig(i) = cg;
      });
  exec.fence();
}

/// :6516-6531: CXX = ((CGSIG*FACX)*ECOS)/CLATS, CYY = (CGSIG*FACY)*ESIN; with
/// FLCUR at IOBP_LOC > 0, CXX += (FACX*CX)/CLATS and CYY += FACY*CY. ISEA is
/// IPLG(IP), as the Fortran writes it.
template <class ExecSpace>
void expl_velocities(const ExecSpace& exec, const ExplParams& p,
                     const PdlibGeometry<typename ExecSpace::memory_space>& g,
                     const ExplFields<typename ExecSpace::memory_space>& f,
                     const ExplWork<typename ExecSpace::memory_space>& w) {
  const int nth = g.nth;
  const Real facx = p.facx, facy = p.facy;
  const bool flcur = p.flcur;
  const auto iplg = g.iplg;
  const auto ecos = g.ecos;
  const auto esin = g.esin;
  const auto clats = g.clats;
  const auto iobp = g.iobp_loc;
  const auto cx = f.cx;
  const auto cy = f.cy;
  const auto cgsig = w.cgsig;
  const auto cxx = w.cxx;
  const auto cyy = w.cyy;
  Kokkos::parallel_for(
      "pdlib_expl_velocities", Kokkos::RangePolicy<ExecSpace>(exec, 0, g.npa),
      KOKKOS_LAMBDA(const int i) {
        const int isea = iplg(i);
        const Real cl = clats(isea);
        const Real cgx = cgsig(i) * facx;
        const Real cgy = cgsig(i) * facy;
        for (int t = 0; t < nth; ++t) {
          cxx(t, i) = (cgx * ecos(t)) / cl;
          cyy(t, i) = cgy * esin(t);
        }
        if (flcur && iobp(i) > 0) {
          const Real dx = (facx * cx(isea)) / cl;
          const Real dy = facy * cy(isea);
          for (int t = 0; t < nth; ++t) {
            cxx(t, i) = cxx(t, i) + dx;
            cyy(t, i) = cyy(t, i) + dy;
          }
        }
      });
  exec.fence();
}

/// :6533-6574, per element IE: KELEM1-3, NM and FLALL1-3 of frequency IK. Each
/// element writes only its own column; LAMBDAX/Y and FL** are per (ITH,IE).
template <class ExecSpace>
void expl_elements(const ExecSpace& exec, int ik,
                   const PdlibGeometry<typename ExecSpace::memory_space>& g,
                   const ExplArrays<typename ExecSpace::memory_space>& a,
                   const ExplWork<typename ExecSpace::memory_space>& w) {
  using expl_detail::dbl;
  using expl_detail::kThr;
  using expl_detail::max_ss;
  using expl_detail::min_ss;
  const int nth = g.nth;
  const int k = ik - 1;
  const auto ine = g.ine;
  const auto ien = g.ien;
  const auto cxx = w.cxx;
  const auto cyy = w.cyy;
  const auto kelem1 = a.kelem1, kelem2 = a.kelem2, kelem3 = a.kelem3;
  const auto flall1 = a.flall1, flall2 = a.flall2, flall3 = a.flall3;
  const auto nm = a.nm;
  Kokkos::parallel_for(
      "pdlib_expl_elements", Kokkos::RangePolicy<ExecSpace>(exec, 0, g.ne),
      KOKKOS_LAMBDA(const int e) {
        const int i1 = ine(0, e) - 1, i2 = ine(1, e) - 1, i3 = ine(2, e) - 1;
        const Real n1 = ien(0, e), n2 = ien(1, e), n3 = ien(2, e);
        const Real n4 = ien(3, e), n5 = ien(4, e), n6 = ien(5, e);
        for (int t = 0; t < nth; ++t) {
          const Real lx = ((cxx(t, i1) + cxx(t, i2)) + cxx(t, i3)) * kOneSixth;  // :6542
          const Real ly = ((cyy(t, i1) + cyy(t, i2)) + cyy(t, i3)) * kOneSixth;  // :6543
          const Real k1 = lx * n1 + ly * n2;  // :6544-6549, stored widened and read back
          const Real k2 = lx * n3 + ly * n4;
          const Real k3 = lx * n5 + ly * n6;
          Real s = min_ss(k1, kZero) + kZero;  // :6550, SUM(MIN(ZERO,KTMP))
          s = s + min_ss(k2, kZero);
          s = s + min_ss(k3, kZero);
          nm(t, e, k) = -(1.0 / static_cast<double>(min_ss(s, -kThr)));
          const double ke1 = static_cast<double>(max_ss(k1, kZero));  // :6551-6553
          const double ke2 = static_cast<double>(max_ss(k2, kZero));
          const double ke3 = static_cast<double>(max_ss(k3, kZero));
          kelem1(t, e, k) = ke1;
          kelem2(t, e, k) = ke2;
          kelem3(t, e, k) = ke3;
          const Real fl11 = cxx(t, i2) * n1 + cyy(t, i2) * n2;  // :6556-6561
          const Real fl12 = cxx(t, i3) * n1 + cyy(t, i3) * n2;
          const Real fl21 = cxx(t, i3) * n3 + cyy(t, i3) * n4;
          const Real fl22 = cxx(t, i1) * n3 + cyy(t, i1) * n4;
          const Real fl31 = cxx(t, i1) * n5 + cyy(t, i1) * n6;
          const Real fl32 = cxx(t, i2) * n5 + cyy(t, i2) * n6;
          const Real fl111 = static_cast<Real>(2.0 * dbl(fl11) + dbl(fl12));  // :6563-6568
          const Real fl112 = static_cast<Real>(2.0 * dbl(fl12) + dbl(fl11));
          const Real fl211 = static_cast<Real>(2.0 * dbl(fl21) + dbl(fl22));
          const Real fl212 = static_cast<Real>(2.0 * dbl(fl22) + dbl(fl21));
          const Real fl311 = static_cast<Real>(2.0 * dbl(fl31) + dbl(fl32));
          const Real fl312 = static_cast<Real>(2.0 * dbl(fl32) + dbl(fl31));
          flall1(t, e, k) = dbl((fl311 + fl212) * kOneSixth) + ke1;  // :6570-6572
          flall2(t, e, k) = dbl((fl111 + fl312) * kOneSixth) + ke2;
          flall3(t, e, k) = dbl((fl211 + fl112) * kOneSixth) + ke3;
        }
      });
  exec.fence();
}

namespace expl_detail {
// KELEM<pos>(ITH,IE,IK), pos = 1..3, the corner that holds the node.
template <class D3>
KOKKOS_INLINE_FUNCTION double kelem_at(const D3& k1, const D3& k2, const D3& k3, int pos, int t,
                                       int e, int k) {
  return (pos == 1) ? k1(t, e, k) : ((pos == 2) ? k2(t, e, k) : k3(t, e, k));
}
}  // namespace expl_detail

/// :6576-6584: KKSUM(ITH,IP) = sum over the elements of IP, in increasing IE, of
/// the KELEM of its corner, each add REAL(DBLE(KKSUM) + KELEM). Gather form.
template <class ExecSpace>
void expl_kksum(const ExecSpace& exec, int ik,
                const PdlibGeometry<typename ExecSpace::memory_space>& g,
                const ExplArrays<typename ExecSpace::memory_space>& a,
                const ExplWork<typename ExecSpace::memory_space>& w) {
  const int nth = g.nth;
  const int k = ik - 1;
  const auto cell_off = g.cell_off;
  const auto ie_cell = g.ie_cell;
  const auto pos_cell = g.pos_cell;
  const auto kelem1 = a.kelem1, kelem2 = a.kelem2, kelem3 = a.kelem3;
  const auto kksum = w.kksum;
  Kokkos::parallel_for(
      "pdlib_expl_kksum", Kokkos::RangePolicy<ExecSpace>(exec, 0, g.npa),
      KOKKOS_LAMBDA(const int i) {
        for (int t = 0; t < nth; ++t) {
          Real s = kZero;
          for (int j = cell_off(i); j < cell_off(i + 1); ++j) {
            const double ke =
                expl_detail::kelem_at(kelem1, kelem2, kelem3, pos_cell(j), t, ie_cell(j) - 1, k);
            s = static_cast<Real>(static_cast<double>(s) + ke);
          }
          kksum(t, i) = s;
        }
      });
  exec.fence();
}

/// :6586-6596 on one thread, IP = 1..NPA in order. dtmaxout_in is DTMAXOUT as
/// the previous IK left it; returns DTMAXOUT.
template <class ExecSpace>
Real expl_dtmax(const ExecSpace& exec, Real dtmaxout_in,
                const PdlibGeometry<typename ExecSpace::memory_space>& g,
                const ExplWork<typename ExecSpace::memory_space>& w) {
  using expl_detail::kDtmaxInit;
  using expl_detail::kThr;
  using expl_detail::max_ss;
  using expl_detail::min_ss;
  const int nth = g.nth, npa = g.npa;
  const bool fsbccfl = g.fsbccfl != 0;
  const auto iobp = g.iobp_loc;
  const auto iobdp = g.iobdp_loc;
  const auto si = g.si;
  const auto kksum = w.kksum;
  const auto dtmax = w.dtmax;
  const auto out = w.dtmaxout;
  Kokkos::parallel_for(
      "pdlib_expl_dtmax", Kokkos::RangePolicy<ExecSpace>(exec, 0, 1), KOKKOS_LAMBDA(const int) {
        Real dtmaxout = dtmaxout_in;
        for (int t = 0; t < nth; ++t) dtmax(t) = kDtmaxInit;  // :6587
        for (int ip = 1; ip <= npa; ++ip) {
          if (iobp(ip - 1) == 1 || fsbccfl) {  // :6589
            const Real dp = static_cast<Real>(iobdp(ip - 1));
            const Real sip = si(ip - 1);
            for (int ith = 1; ith <= nth; ++ith) {
              const Real e = sip / max_ss(kksum(ith - 1, ip - 1) * dp, kThr);  // :6591
              const bool tail = (nth % 2 == 1) && (ith == nth);  // header, :6592
              dtmax(ith - 1) = tail ? min_ss(e, dtmax(ith - 1)) : min_ss(dtmax(ith - 1), e);
            }
            // :6594, MINVAL as the GIMPLE computes it.
            if (nth <= 0) {
              dtmaxout = FLT_MAX;
            } else {
              int s = 1;
              Real lim = Kokkos::Experimental::quiet_NaN<Real>::value;
              for (; s <= nth; ++s) {
                if (dtmax(s - 1) <= Kokkos::Experimental::infinity<Real>::value) {
                  lim = dtmax(s - 1);
                  break;
                }
              }
              for (int r = s + 1; r <= nth; ++r) {
                const Real x = dtmax(r - 1);
                lim = (x < lim) ? x : lim;
              }
              dtmaxout = lim;
            }
          }
        }
        out() = dtmaxout;
      });
  Real host = 0;
  Kokkos::deep_copy(exec, host, w.dtmaxout);
  exec.fence();
  return host;
}

/// :6612-6614: DTSI(IP) = (DBLE(DTMAXGL)/DBLE(ITER(IK)))/DBLE(PDLIB_SI(IP)).
template <class ExecSpace>
void expl_dtsi(const ExecSpace& exec, Real dtmaxgl, int iter,
               const PdlibGeometry<typename ExecSpace::memory_space>& g,
               const ExplArrays<typename ExecSpace::memory_space>& a) {
  const double q = static_cast<double>(dtmaxgl) / static_cast<double>(iter);
  const auto si = g.si;
  const auto dtsi = a.dtsi;
  Kokkos::parallel_for(
      "pdlib_expl_dtsi", Kokkos::RangePolicy<ExecSpace>(exec, 0, g.npa),
      KOKKOS_LAMBDA(const int i) { dtsi(i) = q / static_cast<double>(si(i)); });
  exec.fence();
}

/// :6619-6625: U(ITH,IP) = (VA(ISP,IP)/CGSIG(IP))*CLATS(IPLG(IP)).
template <class ExecSpace>
void expl_to_u(const ExecSpace& exec, int ik,
               const PdlibGeometry<typename ExecSpace::memory_space>& g,
               const ExplFields<typename ExecSpace::memory_space>& f,
               const ExplWork<typename ExecSpace::memory_space>& w) {
  const int nth = g.nth;
  const int off = (ik - 1) * nth;
  const auto iplg = g.iplg;
  const auto clats = g.clats;
  const auto va = f.va;
  const auto cgsig = w.cgsig;
  const auto u = w.u;
  Kokkos::parallel_for(
      "pdlib_expl_to_u", Kokkos::RangePolicy<ExecSpace>(exec, 0, g.npa),
      KOKKOS_LAMBDA(const int i) {
        const Real cl = clats(iplg(i));
        for (int t = 0; t < nth; ++t) u(t, i) = (va(off + t, i + 1) / cgsig(i)) * cl;
      });
  exec.fence();
}

/// :6629-6638: ST(ITH,IP), gathered over the elements of IP in increasing IE;
/// UTILDE of each element from U as it stands (U is not written here).
template <class ExecSpace>
void expl_st(const ExecSpace& exec, int ik,
             const PdlibGeometry<typename ExecSpace::memory_space>& g,
             const ExplArrays<typename ExecSpace::memory_space>& a,
             const ExplWork<typename ExecSpace::memory_space>& w) {
  const int nth = g.nth;
  const int k = ik - 1;
  const auto ine = g.ine;
  const auto cell_off = g.cell_off;
  const auto ie_cell = g.ie_cell;
  const auto pos_cell = g.pos_cell;
  const auto kelem1 = a.kelem1, kelem2 = a.kelem2, kelem3 = a.kelem3;
  const auto flall1 = a.flall1, flall2 = a.flall2, flall3 = a.flall3;
  const auto nm = a.nm;
  const auto u = w.u;
  const auto st = w.st;
  Kokkos::parallel_for(
      "pdlib_expl_st", Kokkos::RangePolicy<ExecSpace>(exec, 0, g.npa),
      KOKKOS_LAMBDA(const int i) {
        for (int t = 0; t < nth; ++t) {
          Real s = kZero;  // :6629
          const Real ui = u(t, i);
          for (int j = cell_off(i); j < cell_off(i + 1); ++j) {
            const int e = ie_cell(j) - 1;
            const int pos = pos_cell(j);
            const double u1 = static_cast<double>(u(t, ine(0, e) - 1));
            const double u2 = static_cast<double>(u(t, ine(1, e) - 1));
            const double u3 = static_cast<double>(u(t, ine(2, e) - 1));
            const Real ut = static_cast<Real>(
                nm(t, e, k) *
                ((flall1(t, e, k) * u1 + flall2(t, e, k) * u2) + flall3(t, e, k) * u3));  // :6633
            const double ke = expl_detail::kelem_at(kelem1, kelem2, kelem3, pos, t, e, k);
            s = static_cast<Real>(static_cast<double>(s) +
                                  ke * static_cast<double>(ui - ut));  // :6634-6636
          }
          st(t, i) = s;
        }
      });
  exec.fence();
}

/// :6639-6646: the update and, with W3_REF1, the restore from UOLD, per IP.
template <bool kRef1, class ExecSpace>
void expl_update_u(const ExecSpace& exec, const ExplParams& p,
                   const PdlibGeometry<typename ExecSpace::memory_space>& g,
                   const ExplArrays<typename ExecSpace::memory_space>& a,
                   const ExplWork<typename ExecSpace::memory_space>& w) {
  using expl_detail::max_sd;
  const int nth = g.nth;
  const Real refpars3 = p.refpars3;
  const auto iobpa = g.iobpa_loc;
  const auto iobpd = g.iobpd_loc;
  const auto iobdp = g.iobdp_loc;
  const auto dtsi = a.dtsi;
  const auto u = w.u;
  const auto st = w.st;
  const auto uold = w.uold;
  Kokkos::parallel_for(
      "pdlib_expl_update_u", Kokkos::RangePolicy<ExecSpace>(exec, 0, g.npa),
      KOKKOS_LAMBDA(const int i) {
        const double dt = dtsi(i);
        const int pa = static_cast<int>(iobpa(i));
        const double act = static_cast<double>(1 - pa);
        const double dp = static_cast<double>(iobdp(i));
        for (int t = 0; t < nth; ++t) {
          if (kRef1) {  // :6643
            if (refpars3 < Real(0.5) && iobpd(t, i) == 0 && pa == 0) {
              u(t, i) = uold(t, i);
              continue;
            }
          }
          const double v = static_cast<double>(u(t, i)) - (static_cast<double>(st(t, i)) * dt) * act;
          const double m = max_sd(v, 0.0) * static_cast<double>(iobpd(t, i));  // :6641
          u(t, i) = static_cast<Real>(dp * m);
        }
      });
  exec.fence();
}

/// :6648-6668: the open-boundary values into U at sub-step IT, parallel over ITH,
/// IBI in order. RD1/RD2 are recomputed per ITH as the Fortran does.
template <class ExecSpace>
void expl_boundary_u(const ExecSpace& exec, int ik, int it, int iter, Real dtmaxgl,
                     const ExplParams& p,
                     const PdlibGeometry<typename ExecSpace::memory_space>& g,
                     const PdlibBoundary<typename ExecSpace::memory_space>& b,
                     const ExplWork<typename ExecSpace::memory_space>& w) {
  using expl_detail::max_ss;
  using expl_detail::min_ss;
  const int nth = g.nth, nbi = b.nbi;
  const Real rd10 = p.rd10, rd20 = p.rd20;
  const auto isbpi = b.isbpi;
  const auto bbpi0 = b.bbpi0;
  const auto bbpin = b.bbpin;
  const auto mapsf = g.mapsf;
  const auto ipgl_npa = g.ipgl_npa;
  const auto clats = g.clats;
  const auto u = w.u;
  Kokkos::parallel_for(
      "pdlib_expl_boundary_u", Kokkos::RangePolicy<ExecSpace>(exec, 1, nth + 1),
      KOKKOS_LAMBDA(const int ith) {
        const int isp = ith + (ik - 1) * nth;  // :6650
        Real rd1 = rd10 - (dtmaxgl * static_cast<Real>(iter - it)) / static_cast<Real>(iter);  // :6651
        Real rd2 = rd20;
        if (rd2 > kRd2Floor) {                                                 // :6653
          rd2 = min_ss(max_ss(rd1 / rd2, kZero), static_cast<Real>(1.0f));  // :6654
          rd1 = static_cast<Real>(1.0f) - rd2;
        } else {
          rd1 = kZero;
          rd2 = static_cast<Real>(1.0f);
        }
        for (int ibi = 1; ibi <= nbi; ++ibi) {  // :6660
          const int isea = isbpi(ibi - 1);
          const int jx = ipgl_npa(mapsf(isea - 1, 0) - 1);  // :6661-6662
          if (jx > 0)
            u(ith - 1, jx - 1) =
                (rd1 * bbpi0(isp - 1, ibi) + rd2 * bbpin(isp - 1, ibi)) * clats(isea);  // :6664
        }
      });
  exec.fence();
}

/// :6675-6681: VA(ISP,IP) = (U(ITH,IP)*CGSIG(IP))/CLATS(IPLG(IP)).
template <class ExecSpace>
void expl_to_va(const ExecSpace& exec, int ik,
                const PdlibGeometry<typename ExecSpace::memory_space>& g,
                const ExplFields<typename ExecSpace::memory_space>& f,
                const ExplWork<typename ExecSpace::memory_space>& w) {
  const int nth = g.nth;
  const int off = (ik - 1) * nth;
  const auto iplg = g.iplg;
  const auto clats = g.clats;
  const auto va = f.va;
  const auto cgsig = w.cgsig;
  const auto u = w.u;
  Kokkos::parallel_for(
      "pdlib_expl_to_va", Kokkos::RangePolicy<ExecSpace>(exec, 0, g.npa),
      KOKKOS_LAMBDA(const int i) {
        const Real cl = clats(iplg(i));
        const Real cg = cgsig(i);
        for (int t = 0; t < nth; ++t) va(off + t, i + 1) = (u(t, i) * cg) / cl;
      });
  exec.fence();
}

/// PDLIB_EXPLICIT_BLOCK(IMOD, FACX, FACY, RD10, RD20, DTG, VGX, VGY, LCALC),
/// :6408-6685. A Hooks type provides, on the host:
///   Real allreduce_min(Real dtmaxout);  :6598-6600, returns DTMAXGL
///   void exchange_u();                  :6626, :6670, PDLIB_exchange2DREAL on w.u
/// kRef1: the switch's W3_REF1. The mesh must pass check_cell_adjacency
/// (pdlib_params.hpp) for the gathers to add in the Fortran's order.
template <bool kRef1, class ExecSpace, class Hooks>
void pdlib_explicit_block(const ExecSpace& exec, const ExplParams& p,
                          const PdlibGeometry<typename ExecSpace::memory_space>& g,
                          const PdlibBoundary<typename ExecSpace::memory_space>& b,
                          const ExplFields<typename ExecSpace::memory_space>& f,
                          const ExplArrays<typename ExecSpace::memory_space>& a,
                          const ExplWork<typename ExecSpace::memory_space>& w, ExplCarry& c,
                          Hooks& hooks) {
  using expl_detail::kThr;
  for (int ik = 1; ik <= g.nk; ++ik) {  // :6508
    if (p.lcalc) {                      // :6510
      expl_wavnu3(exec, ik, g, f, w);      // :6512-6514
      expl_velocities(exec, p, g, f, w);   // :6516-6531
      expl_elements(exec, ik, g, a, w);    // :6533-6574
      expl_kksum(exec, ik, g, a, w);       // :6576-6584
      c.dtmaxout = expl_dtmax(exec, c.dtmaxout, g, w);  // :6586-6596
      c.dtmaxgl = hooks.allreduce_min(c.dtmaxout);      // :6598-6600
      const Real cflxy = p.dtg / c.dtmaxgl;             // :6602 (header)
      const Real rest =
          std::fabs(static_cast<Real>(std::fmod(static_cast<double>(cflxy), 1.0)));  // :6603
      const int n = std::abs(static_cast<int>(std::lround(cflxy)));
      int iter;
      if (rest < kThr) {                            // :6604-6605
        iter = n;
      } else if (rest > kThr && rest < Real(0.5)) {  // :6606-6607
        iter = n + 1;
      } else {  // :6608-6609
        iter = n;
      }
      a.iter(ik - 1) = iter;
      expl_dtsi(exec, c.dtmaxgl, iter, g, a);  // :6612-6614
    }
    expl_to_u(exec, ik, g, f, w);  // :6619-6625
    hooks.exchange_u();            // :6626
    const int iter = a.iter(ik - 1);
    for (int it = 1; it <= iter; ++it) {  // :6628
      expl_st(exec, ik, g, a, w);                  // :6629-6638
      expl_update_u<kRef1>(exec, p, g, a, w);      // :6639-6646
      if (b.flbpi != 0) expl_boundary_u(exec, ik, it, iter, c.dtmaxgl, p, g, b, w);  // :6648-6668
      hooks.exchange_u();                          // :6670
    }
    expl_to_va(exec, ik, g, f, w);  // :6675-6681
  }
}

}  // namespace ww::pdlib
