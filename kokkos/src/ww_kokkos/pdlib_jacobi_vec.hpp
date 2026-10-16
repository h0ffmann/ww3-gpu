// kokkos/src/ww_kokkos/pdlib_jacobi_vec.hpp
// calcARRAY_JACOBI_VEC: the geographic advection of the implicit N-scheme for every
// spectral component, added into ASPAR_JAC (CSR) and B_JAC of the nodes this rank owns,
// on top of what the caller left there. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd_pdlib.F90:3531-3787 at NOAA-EMC/WW3 03adda91;
// A. Roland, M. Dutour-Sikiric. Evidence: kokkos/port_report/CALCARRAY_JACOBI_VEC_KOKKOS.md.
// kRef1 is the W3_REF1 build (:3743-3754); W3_MGP and NOCGTABLE are not ported.
// SPDX-License-Identifier: LGPL-3.0-or-later
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
  /// PDLIB_TRIA03(ne) (yownodepool.F90:80): one third of each element's area.
  Kokkos::View<const Real*, Kokkos::LayoutLeft, MemSpace> tria03;
  /// PDLIB_POSI(3,COUNT_MAX) (yownodepool.F90:85): CSR positions K (1-based) in row IP of
  /// the node and of the next and previous corner of its J-th element. posi(k-1, J-1).
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
  /// VA(NSPEC,0:NSEALM) (w3wdatmd.F90:491): va(ISP-1, IP), IP the local node 1..np.
  Kokkos::View<const Real**, Kokkos::LayoutLeft, MemSpace> va;
};

/// CXY(2,NPA), KP(3,NE), DELTAL(3,NE) (:3646-3654) with a third extent, the ISP per pass
/// (1 is the Fortran's memory); the bits do not depend on it.
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

/// calcARRAY_JACOBI_VEC(DTG,FACX,FACY,VGX,VGY), :3531-3787: adds the advection of every ISP
/// into aspar_jac and b_jac (b_jac = 0 at dry nodes). Returns after a fence.
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
          // :3715-3717 in double, narrowed, then the sign flip.
          const double d11 = static_cast<double>(fl11), d12 = static_cast<double>(fl12);
          const double d21 = static_cast<double>(fl21), d22 = static_cast<double>(fl22);
          const double d31 = static_cast<double>(fl31), d32 = static_cast<double>(fl32);
          const double os = static_cast<double>(kOneSixth);
          const Real crfs1 = -static_cast<Real>((2.0 * d31 + d32 + d21 + 2.0 * d22) * os);
          const Real crfs2 = -static_cast<Real>((2.0 * d32 + 2.0 * d11 + d12 + d31) * os);
          const Real crfs3 = -static_cast<Real>((2.0 * d12 + 2.0 * d21 + d22 + d11) * os);
          // :3718-3719, the + 0. kept: it turns -0 into +0.
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
          // :3724, minss with 0 as source; (CRFS-KP)/MIN(..) narrowed to float, exact.
          const Real s = ((min_ss(k1, kZero) + kZero) + min_ss(k2, kZero)) + min_ss(k3, kZero);
          const Real den = min_ss(s, -kThr);
          deltal(0, ie0, b) = (crfs1 - kp1) / den;
          deltal(1, ie0, b) = (crfs2 - kp2) / den;
          deltal(2, ie0, b) = (crfs3 - kp3) / den;
        });

    // :3728-3770, IP = 1..np. POSI holds positions in CSR row IP only (yowpdlibmain.F90:
    // 1618-1630): no two threads touch one entry, and each walks its J in the Fortran's order.
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
