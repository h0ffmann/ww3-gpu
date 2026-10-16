// kokkos/src/ww_kokkos/pdlib_jacobi_vec.hpp
// calcARRAY_JACOBI_VEC, ported to Kokkos: the geographical-advection part of
// the implicit N-scheme system of the block Jacobi/Gauss-Seidel solver, for
// every spectral component: adds to the CSR matrix ASPAR_JAC(NSPEC,PDLIB_NNZ)
// and the right-hand side B_JAC(NSPEC,NSEAL) of the nodes this rank owns.
// Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3profsmd_pdlib.F90:3531-3787 at NOAA-EMC/WW3
// 03adda91 (the WW3/ fork's file is byte-identical, v, cmp 2026-10-16); authors
// A. Roland and M. Dutour-Sikiric. Called by PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK
// (:5767) when IMEM == 1, after ASPAR_JAC = ZERO and B_JAC = ZERO (:5741-5748,
// when .NOT. LSLOC) and CALCARRAY_JACOBI_SOURCE_1 (:5756): this kernel adds to
// what is there.
//
// Switches: the routine's preprocessor blocks, and what each one does here.
//   W3_REF1 (USE of IOBP_LOC :3583-3585, INTEGER IBR :3641-3643, IBR :3731-3733,
//     the branch :3743-3754): at nodes with IBR == 1, DTK = KP*DTG without IB1
//     and B_JAC + TRIA03*VA without IB2. Ported as the template flag kRef1
//     (default false, REF0). Every ww3_tp2.17 switch under input_deep/ and
//     input_deep_restart/ has REF1, and so does model/bin/switch_NWS_rwps
//     (byte-identical to input_deep/switch_PDLIB, v, cmp); matrix.base runs
//     input_deep as `-s PDLIB -i input_deep -g e`. 8 switch files with PDLIB
//     under regtests/*/input*/ and model/bin/ have REF1 (v, grep).
//   W3_DB1, W3_BT1, W3_BT4, W3_BT8, W3_BT9, W3_IC1, W3_IC2, W3_IC3, W3_TR1
//     (:3605-3629): one USE of a source-term module each, nothing else. None of
//     those modules names ASPAR_JAC, B_JAC, POS_TRICK, FSGEOADVECT or memunit
//     (v, grep), so no host-associated name is shadowed, and the arithmetic
//     GIMPLE of the CXY and element loops is the same, statement for statement,
//     with BT4 TR0 IC0 and with BT1 TR1 IC4 (v, normalised diff).
//   W3_MGP (:3634, :3699-3703, the only use of VGX, VGY): no switch file with
//     PDLIB in regtests/*/input*/ or model/bin/ has MGP (v, grep). Not ported, so
//     VGX and VGY are not arguments.
//   NOCGTABLE (:3586, :3652, :3688): not a WW3 switch. CMake defines W3_<name>
//     for each switch (model/src/CMakeLists.txt:29), so no switch file can set
//     the bare NOCGTABLE, and none names it (v, grep). Not ported.
//   W3_S, W3_DEBUGSRC, W3_DEBUGSOLVER: tracing and prints, not ported.
// Checked statically against two objects of WW3's CMake Release build of
// w3profsmd_pdlib.F90 (gfortran 16.2, the -fdump object disassembles identically
// to the CMake one): regtests/ww3_tp2.17/input/switch_PDLIB (REF0 BT4 DB1 TR0
// IC0, kRef1 = false) and input_deep/switch_PDLIB = switch_NWS_rwps (REF1 BT1
// DB1 TR1 IC4, kRef1 = true). The other tp2.17 combinations (switch_RWPS with
// BT4, switch_RWPSBS1 with BS1) differ from the second only in USE-only or
// unrelated switches and were not built (⚠). print_memcheck (:3676,
// :3781) is empty without W3_MEMCHECK (w3servmd.F90:2501-2516) and the store
// memunit = 50000+IAPROC (:3663) is module state: both stay in Fortran. The
// routine has no MPI call; the exchanges of the solver are its caller's.
//
// Module data it reads, besides pdlib_params.hpp (INE, PDLIB_IEN, PDLIB_CCON,
// iplg, IOBPD_LOC, IOBPA_LOC, IOBDP_LOC, CLATS, ECOS, ESIN, np, npa, ne, NTH):
//   PDLIB_TRIA03(ne), PDLIB_POSI(3,COUNT_MAX)   JacobiVecMesh below
//   PDLIB_IE_CELL2(I,IP), PDLIB_POS_CELL2(I,IP) = PDLIB_IE_CELL(J),
//     PDLIB_POS_CELL(J) with J = sum(CCON(1:IP-1)) + I (yowpdlibmain.F90:
//     1505-1515 fills both from the same CELLVERTEX in the same loop), so
//     g.ie_cell(g.cell_off(IP-1)+I-1) and g.pos_cell(...).
//   POS_TRICK(3,2), set to constants in BLOCK_SOLVER_INIT (:6821-6826): jvec_detail::pos_trick.
//   FSGEOADVECT = FLCX .AND. FLCY (:330-333), FLCUR (W3IDATMD), CG(0:NK+1,0:NSEA),
//   CX, CY(0:NSEA), VA(NSPEC,0:NSEALM): JacobiVecIn.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3profsmd_pdlib.F90 (WW3's CMake Release flags, switch_PDLIB):
//   :3674-3675  CCOSA = FACX*ECOS, CSINA = FACX*ESIN (FACX in both, as written):
//               mulps, element-wise; recomputed here per ISP, same product.
//   :3691-3698  (CCOS*CG1)/CLATS and CSIN*CG1 (divss, a true division); with
//               FLCUR, + ((FACX*CX)/CLATS)*REAL(IOBDP) and + (FACY*CY)*REAL(IOBDP)
//               (basic-block SLP on the pair, element-wise lanes).
//   :3709-3714  FLxy = IEN(a)*CXYY(1,n) + IEN(b)*CXYY(2,n), two mulss, one addss.
//   :3715-3717  REAL(8): each FL widened (cvtss2sd), 2.0d0*FL as addsd x,x, the
//               sums in source order (+ commuted in :3716-3717, no bit change),
//               times ONESIXTH widened (.LC96 = 0x3fc5555560000000), cvtsd2ss,
//               then the sign flip (xorps .LC42).
//   :3718-3719  SUM(CXYY(i,:)) is ((c1 + 0.) + c2) + c3 in REAL: the + 0. is
//               kept (addss %xmm10 = +0.; it turns -0 into +0), then * ONESIXTH.
//   :3723       KP = MAX(ZERO,K): maxss %0,k, k > 0 ? k : 0.
//   :3724       SUM(MIN(ZERO,K)) = ((m1 + 0.) + m2) + m3, minss %0,k: k < 0 ? k : 0;
//               MIN(-THR, s): minss .LC102,s: s < -THR ? s : -THR. The REAL(8)
//               (CRFS-KP)*1.d0/MIN(..) was narrowed to subss, divss: exact, a
//               double rounding of one IEEE / of two REALs is innocuous
//               (53 >= 2*24+2), so the C++ writes the float division.
//   :3752-3764  DTK = (KP*DTG)*REAL(IB1); B_JAC + (TRIA03*VA)*REAL(IB2);
//               ((A(I1) + TRIA03) + DTK) - DELTAL*DTK; A(Ik) - DELTAL*DTK.
//   :3745-3746  REF1 object, IBR == 1: DTK = KP*DTG (one mulss);
//               B_JAC + TRIA03*VA (one mulss, one addss); the ASPAR_JAC
//               statements as above. IBR == 0 is :3748-3749, the same as
//               :3752-3753. IBR = (1-IOBPA)*((1-IOBP)*(1-IOBPD)) in INTEGER(4)
//               (INTEGER*2 and *1 widened first): exact, any order.
// No FMA, no libm call, no reciprocal multiply. Vectorised (-fopt-info-vec):
// :3674, :3675 (loops) and :3693, :3696 (SLP), all element-wise; no reduction
// is vectorised, so every sum is the scalar order written here.
//
// Why the parallel passes give the Fortran's bits. The Fortran runs, per ISP:
// a node loop filling CXY(2,npa), an element loop filling KP, DELTAL(3,ne), and
// the assembly loop over IP = 1..np. Here the same three loops are three
// passes, each over (ISP of a batch, IP or IE):
//   CXY: each (ISP,IP) writes CXY(:,IP) of its ISP from inputs only.
//   KP, DELTAL: each (ISP,IE) writes column IE of its ISP from CXY only.
//   Assembly: each (ISP,IP) writes B_JAC(ISP,IP) and ASPAR_JAC(ISP,K) only for
//   K = PDLIB_POSI(1:3,J) of its own J, and those are positions in CSR row IP:
//   ComputeIA_JA_POSI_NNZ searches K in PDLIB_IA(IP)..PDLIB_IA(IP+1)-1 only
//   (yowpdlibmain.F90:1618-1630). Rows of different IP are disjoint, so no two
//   threads touch one entry; within one IP the thread runs I = 1..CCON(IP) and
//   the statements in the Fortran's order, reading and writing ASPAR_JAC in
//   memory each time (a repeated K in a degenerate element still sees the
//   previous store). The running J of :3730-3769 counts CCON(IP) in both
//   branches, so at node IP it starts at g.cell_off(IP-1). Different ISP touch
//   different rows of ASPAR_JAC and B_JAC and their own work columns.
// No reduction, no atomic: the result is the same on any execution space and
// batch size. Kernels on one execution-space instance run in order; the fence
// after each batch makes the work arrays reusable.
//
// Precision: every array is REAL (float); REAL(8) only in CRFS (:3715-3717), as
// the object computes it. Integers IB1 = (1-IOBPA)*IOBPD and IB2 = IOBPD are
// default INTEGER, converted to REAL where multiplied (cvtsi2ss).
//
// The C++ object (openmp-release flags, -ffp-contract=off, g++ 16.2) has the same
// operations in the same order: no FMA, cvtsd2ss then the sign flip, the + 0.
// adds, vmaxss/vminss with the Fortran's operand roles; min_ss(k, 0) is emitted
// as vcmpnltss/vandnps, which gives k < 0 ? k : +0 for every k, NaN included.
//
// CUDA (nvcc_wrapper, sm_89, --fmad=false, compiled, never run): the PTX has no
// fma; divisions are div.rn.f32 (the FFMA in the SASS are its correctly rounded
// expansion); the ternary MIN/MAX become min.f32/max.f32, which return the
// non-NaN operand as the x86 code does. A -0 from min.f32 in the SUM of :3724
// cannot change it: the sum starts with m1 + 0., never -0 in round-to-nearest.
// Whether max.f32(-0, +0) is +0 as maxss gives, for KP, is not checked here (⚠).
#pragma once

#include <Kokkos_Core.hpp>

#include "pdlib_params.hpp"
#include "real.hpp"

namespace ww::pdlib {

namespace jvec_detail {
// SSE MIN/MAX as emitted (dest OP src ? dest : src): a NaN or a +0/-0 tie in
// `a` gives `b`. Each call passes the operands in the order read from the object.
KOKKOS_INLINE_FUNCTION Real min_ss(Real a, Real b) { return (a < b) ? a : b; }
KOKKOS_INLINE_FUNCTION Real max_ss(Real a, Real b) { return (a > b) ? a : b; }

/// POS_TRICK(POS,k), 1-based values (BLOCK_SOLVER_INIT, :6821-6826): the two
/// other corners of an element, counter-clockwise from POS.
KOKKOS_INLINE_FUNCTION int pos_trick(int pos, int k) {
  // pos 1 -> (2,3), 2 -> (3,1), 3 -> (1,2)
  return (k == 1) ? (pos == 3 ? 1 : pos + 1) : (pos == 1 ? 3 : pos - 1);
}
}  // namespace jvec_detail

/// Mesh arrays this routine reads that PdlibGeometry lacks.
template <class MemSpace>
struct JacobiVecMesh {
  using memory_space = MemSpace;
  /// PDLIB_TRIA03(ne), REAL(rkind=4) (PDLIB/yownodepool.F90:80, filled
  /// yowpdlibmain.F90:1411-1415): one third of each element's area.
  Kokkos::View<const Real*, Kokkos::LayoutLeft, MemSpace> tria03;
  /// PDLIB_POSI(3,COUNT_MAX), integer (yownodepool.F90:85, filled
  /// yowpdlibmain.F90:1616-1631): for the J-th (node, element) pair, the CSR
  /// positions K (1-based, values kept) of the node itself, and the next and
  /// previous corner, in row IP. posi(k-1, J-1).
  Kokkos::View<const int**, Kokkos::LayoutLeft, MemSpace> posi;
};

/// Per-call input (W3ADATMD, W3WDATMD, W3IDATMD, module flags, dummy args).
template <class MemSpace>
struct JacobiVecIn {
  using memory_space = MemSpace;
  int nspec = 0;        ///< NSPEC = NK*NTH
  int fsgeoadvect = 0;  ///< FSGEOADVECT, LOGICAL (:126, :330-333), 0/1
  int flcur = 0;        ///< FLCUR, LOGICAL (w3idatmd.F90:261), 0/1
  Real dtg = 0;         ///< DTG, the dummy argument
  Real facx = 0;        ///< FACX
  Real facy = 0;        ///< FACY
  /// CG(0:NK+1,0:NSEA), REAL (w3adatmd.F90:1325): cg(IK, IP_glob), bases kept.
  Kokkos::View<const Real**, Kokkos::LayoutLeft, MemSpace> cg;
  /// CX(0:NSEA), CY(0:NSEA), REAL (w3adatmd.F90:1008): cx(IP_glob).
  Kokkos::View<const Real*, Kokkos::LayoutLeft, MemSpace> cx;
  Kokkos::View<const Real*, Kokkos::LayoutLeft, MemSpace> cy;
  /// VA(NSPEC,0:NSEALM), REAL (w3wdatmd.F90:491): va(ISP-1, IP), the second
  /// base 0 kept, IP the local node 1..np.
  Kokkos::View<const Real**, Kokkos::LayoutLeft, MemSpace> va;
};

/// The Fortran's automatic arrays CXY(2,NPA), KP(3,NE), DELTAL(3,NE)
/// (:3646-3654), with a third extent: the number of ISP done per pass. 1 is the
/// Fortran's memory; NSPEC is three launches per call. Same bits either way.
template <class MemSpace>
struct JacobiVecWork {
  using memory_space = MemSpace;
  using W3 = Kokkos::View<Real***, Kokkos::LayoutLeft, MemSpace>;
  W3 cxy;     ///< cxy(c-1, IP-1, b) = CXY(c,IP) of the b-th ISP of the batch
  W3 kp;      ///< kp(pos-1, IE-1, b) = KP(pos,IE)
  W3 deltal;  ///< deltal(pos-1, IE-1, b) = DELTAL(pos,IE)

  static JacobiVecWork allocate(int npa, int ne, int batch) {
    JacobiVecWork w;
    w.cxy = W3("jvec_cxy", 2, npa, batch);
    w.kp = W3("jvec_kp", 3, ne, batch);
    w.deltal = W3("jvec_deltal", 3, ne, batch);
    return w;
  }
};

/// ASPAR_JAC(NSPEC,PDLIB_NNZ) and B_JAC(NSPEC,NSEAL), REAL (:114, allocated
/// :7589, :7595), read and written: aspar_jac(ISP-1, K-1), b_jac(ISP-1, IP-1).
template <class MemSpace>
using JacobiMatrixView = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;

/// calcARRAY_JACOBI_VEC(DTG,FACX,FACY,VGX,VGY), w3profsmd_pdlib.F90:3531-3787.
/// Adds the advection terms of every ISP = 1..NSPEC into aspar_jac and b_jac
/// (and sets b_jac(ISP,IP) = 0 at dry nodes), as the Fortran does. Returns
/// after a fence. kRef1 is the switch W3_REF1: calcarray_jacobi_vec<true>(...)
/// for a WW3 built with REF1, the default for REF0.
template <bool kRef1 = false, class ExecSpace>
void calcarray_jacobi_vec(const ExecSpace& exec,
                          const PdlibGeometry<typename ExecSpace::memory_space>& g,
                          const JacobiVecMesh<typename ExecSpace::memory_space>& m,
                          const JacobiVecIn<typename ExecSpace::memory_space>& in,
                          const JacobiVecWork<typename ExecSpace::memory_space>& w,
                          const JacobiMatrixView<typename ExecSpace::memory_space>& aspar_jac,
                          const JacobiMatrixView<typename ExecSpace::memory_space>& b_jac) {
  using jvec_detail::max_ss;
  using jvec_detail::min_ss;
  using Policy2 = Kokkos::MDRangePolicy<ExecSpace, Kokkos::Rank<2>>;

  const int nth = g.nth;
  const int npa = g.npa;
  const int np = g.np;
  const int ne = g.ne;
  const int nspec = in.nspec;
  const int batch = static_cast<int>(w.cxy.extent(2));
  if (batch <= 0) return;

  const auto ine = g.ine;
  const auto ien = g.ien;
  const auto ccon = g.ccon;
  const auto cell_off = g.cell_off;
  const auto ie_cell = g.ie_cell;
  const auto pos_cell = g.pos_cell;
  const auto iplg = g.iplg;
  const auto iobpd_loc = g.iobpd_loc;
  const auto iobpa_loc = g.iobpa_loc;
  const auto iobdp_loc = g.iobdp_loc;
  const auto iobp_loc = g.iobp_loc;  // read only when kRef1
  const auto clats = g.clats;
  const auto ecos = g.ecos;
  const auto esin = g.esin;
  const auto tria03 = m.tria03;
  const auto posi = m.posi;
  const auto cg = in.cg;
  const auto cx = in.cx;
  const auto cy = in.cy;
  const auto va = in.va;
  const auto cxy = w.cxy;
  const auto kp = w.kp;
  const auto deltal = w.deltal;
  const int flcur = in.flcur;
  const int fsgeo = in.fsgeoadvect;
  const Real dtg = in.dtg;
  const Real facx = in.facx;
  const Real facy = in.facy;

  for (int isp0 = 1; isp0 <= nspec; isp0 += batch) {
    const int nb = (nspec - isp0 + 1 < batch) ? nspec - isp0 + 1 : batch;

    // :3687-3704, IP = 1..NPA.
    Kokkos::parallel_for(
        "pdlib_jacobi_vec_cxy", Policy2(exec, {0, 0}, {nb, npa}),
        KOKKOS_LAMBDA(const int b, const int ip0) {
          const int isp = isp0 + b;
          const int ith = 1 + (isp - 1) % nth;
          const int ik = 1 + (isp - 1) / nth;
          const Real ccos = facx * ecos(ith - 1);  // CCOSA(ITH), :3674, :3683
          const Real csin = facx * esin(ith - 1);  // CSINA(ITH), :3675, :3684
          const int ip_glob = iplg(ip0);
          const Real cg1 = cg(ik, ip_glob);
          Real c1 = (ccos * cg1) / clats(ip_glob);
          Real c2 = csin * cg1;
          if (flcur) {
            const Real dp = static_cast<Real>(iobdp_loc(ip0));
            c1 = c1 + ((facx * cx(ip_glob)) / clats(ip_glob)) * dp;
            c2 = c2 + (facy * cy(ip_glob)) * dp;
          }
          cxy(0, ip0, b) = c1;
          cxy(1, ip0, b) = c2;
        });

    // :3706-3726, IE = 1..NE.
    Kokkos::parallel_for(
        "pdlib_jacobi_vec_elements", Policy2(exec, {0, 0}, {nb, ne}),
        KOKKOS_LAMBDA(const int b, const int ie0) {
          const int n1 = ine(0, ie0) - 1;
          const int n2 = ine(1, ie0) - 1;
          const int n3 = ine(2, ie0) - 1;
          const Real x1 = cxy(0, n1, b), x2 = cxy(0, n2, b), x3 = cxy(0, n3, b);
          const Real y1 = cxy(1, n1, b), y2 = cxy(1, n2, b), y3 = cxy(1, n3, b);
          const Real i1 = ien(0, ie0), i2 = ien(1, ie0), i3 = ien(2, ie0);
          const Real i4 = ien(3, ie0), i5 = ien(4, ie0), i6 = ien(5, ie0);
          const Real fl11 = x2 * i1 + y2 * i2;
          const Real fl12 = x3 * i1 + y3 * i2;
          const Real fl21 = x3 * i3 + y3 * i4;
          const Real fl22 = x1 * i3 + y1 * i4;
          const Real fl31 = x1 * i5 + y1 * i6;
          const Real fl32 = x2 * i5 + y2 * i6;
          const double d11 = static_cast<double>(fl11), d12 = static_cast<double>(fl12);
          const double d21 = static_cast<double>(fl21), d22 = static_cast<double>(fl22);
          const double d31 = static_cast<double>(fl31), d32 = static_cast<double>(fl32);
          const double os = static_cast<double>(kOneSixth);
          const Real crfs1 = -static_cast<Real>((2.0 * d31 + d32 + d21 + 2.0 * d22) * os);
          const Real crfs2 = -static_cast<Real>((2.0 * d32 + 2.0 * d11 + d12 + d31) * os);
          const Real crfs3 = -static_cast<Real>((2.0 * d12 + 2.0 * d21 + d22 + d11) * os);
          const Real lambda1 = (((x1 + kZero) + x2) + x3) * kOneSixth;
          const Real lambda2 = (((y1 + kZero) + y2) + y3) * kOneSixth;
          const Real k1 = lambda1 * i1 + lambda2 * i2;
          const Real k2 = lambda1 * i3 + lambda2 * i4;
          const Real k3 = lambda1 * i5 + lambda2 * i6;
          const Real kp1 = max_ss(k1, kZero);
          const Real kp2 = max_ss(k2, kZero);
          const Real kp3 = max_ss(k3, kZero);
          kp(0, ie0, b) = kp1;
          kp(1, ie0, b) = kp2;
          kp(2, ie0, b) = kp3;
          const Real s = ((min_ss(k1, kZero) + kZero) + min_ss(k2, kZero)) + min_ss(k3, kZero);
          const Real den = min_ss(s, -kThr);
          deltal(0, ie0, b) = (crfs1 - kp1) / den;
          deltal(1, ie0, b) = (crfs2 - kp2) / den;
          deltal(2, ie0, b) = (crfs3 - kp3) / den;
        });

    // :3728-3770, IP = 1..np: row IP of ASPAR_JAC and B_JAC(ISP,IP) only.
    Kokkos::parallel_for(
        "pdlib_jacobi_vec_assemble", Policy2(exec, {0, 0}, {nb, np}),
        KOKKOS_LAMBDA(const int b, const int ip0) {
          const int isp = isp0 + b;
          const int s0 = isp - 1;
          const int ith = 1 + (isp - 1) % nth;
          const int iobpd = static_cast<int>(iobpd_loc(ith - 1, ip0));
          const int ib1 = (1 - static_cast<int>(iobpa_loc(ip0))) * iobpd;
          const int ib2 = iobpd;
          const int j0 = cell_off(ip0);
          const int nc = ccon(ip0);
          if (static_cast<int>(iobdp_loc(ip0)) == 1) {
            const Real rib1 = static_cast<Real>(ib1);
            const Real rib2 = static_cast<Real>(ib2);
            for (int i = 0; i < nc; ++i) {
              const int j = j0 + i;
              const int ie0 = ie_cell(j) - 1;
              const int pos = pos_cell(j);
              Real dtk;
              if (kRef1) {  // a constant: nvcc refuses a first capture inside if constexpr
                // :3731-3733, :3744-3750
                const int ibr = (1 - static_cast<int>(iobp_loc(ip0))) * (1 - iobpd) *
                                (1 - static_cast<int>(iobpa_loc(ip0)));
                if (ibr == 1) {
                  dtk = kp(pos - 1, ie0, b) * dtg;
                  b_jac(s0, ip0) = b_jac(s0, ip0) + tria03(ie0) * va(s0, ip0 + 1);
                } else {
                  dtk = (kp(pos - 1, ie0, b) * dtg) * rib1;
                  b_jac(s0, ip0) = b_jac(s0, ip0) + (tria03(ie0) * va(s0, ip0 + 1)) * rib2;
                }
              } else {
                // :3752-3753
                dtk = (kp(pos - 1, ie0, b) * dtg) * rib1;
                b_jac(s0, ip0) = b_jac(s0, ip0) + (tria03(ie0) * va(s0, ip0 + 1)) * rib2;
              }
              const int k1 = posi(0, j) - 1;
              if (fsgeo) {
                const int k2 = posi(1, j) - 1;
                const int k3 = posi(2, j) - 1;
                aspar_jac(s0, k1) = ((aspar_jac(s0, k1) + tria03(ie0)) + dtk) -
                                    dtk * deltal(pos - 1, ie0, b);
                aspar_jac(s0, k2) =
                    aspar_jac(s0, k2) - dtk * deltal(jvec_detail::pos_trick(pos, 1) - 1, ie0, b);
                aspar_jac(s0, k3) =
                    aspar_jac(s0, k3) - dtk * deltal(jvec_detail::pos_trick(pos, 2) - 1, ie0, b);
              } else {
                aspar_jac(s0, k1) = aspar_jac(s0, k1) + tria03(ie0);
              }
            }
          } else {
            for (int i = 0; i < nc; ++i) {
              const int j = j0 + i;
              const int k1 = posi(0, j) - 1;
              aspar_jac(s0, k1) = aspar_jac(s0, k1) + tria03(ie_cell(j) - 1);
            }
            b_jac(s0, ip0) = kZero;
          }
        });
    exec.fence();
  }
}

}  // namespace ww::pdlib
