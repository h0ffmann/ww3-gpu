// kokkos/src/ww_kokkos/pdlib_jacobi_spectral.hpp
// CALCARRAY_JACOBI_SPECTRAL_1, ported to Kokkos: the spectral-space part of the
// implicit (block Jacobi / Gauss-Seidel) system on the unstructured mesh. For
// each node it adds to the diagonal block ASPAR_JAC(:,PDLIB_I_DIAG(IP)) the
// upwind coefficients of the frequency shift (method 1 or 2) and of refraction,
// and keeps the velocities for the solver (CAS_SIG, CWNB_SIG_M2, CAD_THE).
// Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3profsmd_pdlib.F90:4413-4557 at NOAA-EMC/WW3
// 03adda91 (identical in the WW3/ fork, v, cmp, 2026-10-16); authors A. Roland
// and M. Dutour-Sikiric. Called by PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK (:5781-5783)
// when FSFREQSHIFT .OR. FSREFRACTION and IMEM == 1 (w3parall.F90:90, a
// PARAMETER). Compiled as regtests/ww3_tp2.17/input/switch_PDLIB builds it:
// W3_S, W3_DEBUGFREQSHIFT and W3_DEBUGREFRACTION (:4469-4471, :4517-4519,
// :4545-4548) are not compiled and not ported; a kernel never prints. The mesh
// arrays are pdlib_params.hpp; what it lacks is JacSpecArrays below.
//
// Not translated here, called through the Prop template argument (the routines
// of w3parall.F90 that prop_parall.hpp translates). Assumed interface, each a
// const KOKKOS_FUNCTION member of Prop, ip and isea passed as the Fortran's
// 1-based IP and ISEA, every array a pointer to the first element of a
// contiguous Fortran array, written in full by the callee (intent(out)):
//   void freq_shift(int ip, int isea, Real* cas, Real* dmm, Real dtg)
//        PROP_FREQ_SHIFT(IP, ISEA, CAS, DMM, DTG), w3parall.F90:604-739;
//        cas = CAS(1:NSPEC), dmm = DMM(0:NK2) (dmm[0] is DMM(0)).
//   void freq_shift_m2(int ip, int isea, Real* cwnb_m2, Real* dwni_m2, Real dtg)
//        PROP_FREQ_SHIFT_M2, w3parall.F90:754-894; cwnb_m2 = CWNB_M2(1-NTH:NSPEC)
//        (cwnb_m2[0] is CWNB_M2(1-NTH)), dwni_m2 = DWNI_M2(1:NK).
//   void refraction_pr3(int ip, int isea, Real dtg, Real* cad, bool do_limiter)
//        PROP_REFRACTION_PR3, w3parall.F90:447-589; cad = CAD(1:NSPEC).
// The callees fill CAS, CWNB_M2 and CAD completely, and the Fortran then copies
// each into CAS_SIG(:,IP), CWNB_SIG_M2(:,IP), CAD_THE(:,IP) unchanged (:4512,
// :4532, :4550). Here the callee writes straight into that column, which holds
// the same bits; DMM and DWNI_M2 go to the per-node work columns of JacSpecWork.
//
// Precision. Every variable is REAL (float): DTG, eSI, eVal, the automatic
// arrays (:4477-4481), FACHFA (w3gdatmd.F90:799), ZERO = 0.0d0 as a REAL
// PARAMETER (w3parall.F90:94, kZero), ASPAR_JAC and the three saved arrays
// (w3profsmd_pdlib.F90:114-116). No double appears in the routine's GIMPLE.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3profsmd_pdlib.F90 (WW3's CMake Release flags, switch_PDLIB; the
// -fdump-tree-optimized object disassembles identically to the CMake one). No
// FMA, no libm call, no reciprocal multiply; every expression is the source's,
// left to right:
//   :4503  CP_SIG/DMM(IK-1) - CM_SIG/DMM(IK): divss, divss, subss;
//   :4506  B_SIG + (CM_SIG/DMM(NK))*FACHFA: divss, mulss, addss;
//   :4508, :4554  ASPAR + B*eSI: mulss/mulps then addss/addps;
//   :4521-4522  ASPAR - eSI*(DWNI_M2(IK)*(MIN(..) - MAX(..))): subss, mulss, mulss, subss;
//   :4525-4527  ASPAR + eSI*((DWNI_M2(NK)*MIN(..))*FACHFA);
//   :4551-4553  CP = DTG*MAX(ZERO,CAD), CM = DTG*MIN(ZERO,CAD), B = CP - CM.
// The vectorised loops (fopt-info) are the element-wise :4497, :4498, :4508,
// :4551-4554: no reduction anywhere, so lanes add nothing to the order.
//
// MIN/MAX. With a NaN or a +0/-0 tie the SSE result depends on which operand
// is the destination (dest OP src ? dest : src), and the object puts ZERO on
// different sides in the vector body and the scalar tail of one loop. The
// result is therefore a function of the element's position in NSPEC, and is
// written so (n = NSPEC, x the array element, "x>0?x:0" means maxss/maxps with
// x as destination):
//   :4497 MAX(ZERO,CAS): v = (n <= 3) ? 0 : 4*(n/4); x>0?x:0 for i <= v,
//         0>x?0:x for i = v+1, v+2 (the tail of up to three, unrolled);
//   :4498 MIN(ZERO,CAS): x<0?x:0 for i < v and i = v+1, 0<x?0:x for i = v, v+2;
//   :4551 MAX(ZERO,CAD): x>0?x:0, except 0>x?0:x for i = n-1 when n is odd
//         (4-wide body, one 2-wide step, one scalar step);
//   :4552 MIN(ZERO,CAD): x<0?x:0, except 0<x?0:x for i = n-1 when n is odd;
//   :4521 MIN(CWNB_M2(ISP-NTH),ZERO), :4525: x<0?x:0; MAX(CWNB_M2(ISP),ZERO): x>0?x:0.
// i is the 0-based ISP. The products by DTG and eSI are written in the
// object's operand order; IEEE * and + commute, so only a NaN payload could
// tell them apart.
//
// Why the parallel pass gives the Fortran's bits. One thread per node IP. Every
// write of node IP goes to column PDLIB_I_DIAG(IP) of ASPAR_JAC, which is the
// diagonal entry of row IP of the CSR pattern (yowpdlibmain.F90:1625), so no
// two nodes write the same entry, and to column IP of CAS_SIG, CWNB_SIG_M2,
// CAD_THE and of the work arrays. Each node reads only inputs no node writes
// here, and the automatic arrays are filled in full for every IP before they
// are read, so nothing carries from one IP to the next. Inside a node the
// Fortran's loop order is kept, and each ASPAR_JAC entry receives its
// freq-shift update before its refraction update, as at :4508/:4523/:4527
// before :4554. Method 1's B_SIG is fused into the ASPAR update per ISP: B_SIG(ISP)
// depends on CP_SIG(ISP) and CM_SIG(ISP) only, the same float operations in the
// same order, without the NSPEC array a kernel may not allocate. No atomics.
#pragma once

#include <Kokkos_Core.hpp>

#include "pdlib_params.hpp"
#include "real.hpp"

namespace ww::pdlib {

namespace jspec_detail {
// SSE MIN/MAX as emitted: dest OP src ? dest : src; a NaN or a +0/-0 tie in
// `a` gives `b`. Each call passes the operands in the order read from the object.
KOKKOS_INLINE_FUNCTION Real min_ss(Real a, Real b) { return (a < b) ? a : b; }
KOKKOS_INLINE_FUNCTION Real max_ss(Real a, Real b) { return (a > b) ? a : b; }

// :4497 CP_SIG = MAX(ZERO,CAS), element i (0-based) of n.
KOKKOS_INLINE_FUNCTION Real cp_sig(int i, int n, Real x) {
  const int v = (n <= 3) ? 0 : 4 * (n / 4);
  return (i <= v) ? max_ss(x, kZero) : max_ss(kZero, x);
}
// :4498 CM_SIG = MIN(ZERO,CAS).
KOKKOS_INLINE_FUNCTION Real cm_sig(int i, int n, Real x) {
  const int v = (n <= 3) ? 0 : 4 * (n / 4);
  return (i < v || i == v + 1) ? min_ss(x, kZero) : min_ss(kZero, x);
}
// :4551 MAX(ZERO,CAD) and :4552 MIN(ZERO,CAD).
KOKKOS_INLINE_FUNCTION Real cp_the(int i, int n, Real x) {
  return ((n % 2) == 1 && i == n - 1) ? max_ss(kZero, x) : max_ss(x, kZero);
}
KOKKOS_INLINE_FUNCTION Real cm_the(int i, int n, Real x) {
  return ((n % 2) == 1 && i == n - 1) ? min_ss(kZero, x) : min_ss(x, kZero);
}
}  // namespace jspec_detail

/// Scalars of W3GDATMD, W3IDATMD and the module itself that the routine reads.
struct JacSpecConfig {
  int nspec = 0;               ///< W3GDATMD NSPEC = NK*NTH
  int nk2 = 0;                 ///< W3GDATMD NK2, the upper bound of DMM(0:NK2)
  bool fsfreqshift = false;    ///< W3GDATMD FSFREQSHIFT (w3gdatmd.F90:1046)
  bool fsrefraction = false;   ///< W3GDATMD FSREFRACTION (:1045)
  bool flcur = false;          ///< W3IDATMD FLCUR
  bool fllev = false;          ///< W3IDATMD FLLEV
  int freq_shift_method = 2;   ///< FreqShiftMethod, module INTEGER = 2, never reassigned (:125)
  Real fachfa = 0;             ///< W3GDATMD FACHFA, REAL (:799)
};

/// The arrays of w3profsmd_pdlib and yowNodepool the routine reads or writes
/// that PdlibGeometry lacks. LayoutLeft, Fortran extents.
template <class MemSpace>
struct JacSpecArrays {
  using memory_space = MemSpace;
  using W2 = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;
  /// PDLIB_I_DIAG(npa), integer (PDLIB/yownodepool.F90:85, filled
  /// yowpdlibmain.F90:1625): 1-based position K of the diagonal entry of row
  /// IP in the CSR arrays, the column of ASPAR_JAC.
  Kokkos::View<const int*, Kokkos::LayoutLeft, MemSpace> i_diag;
  /// ASPAR_JAC(NSPEC, PDLIB_NNZ), REAL (w3profsmd_pdlib.F90:114, :7589).
  W2 aspar_jac;
  /// CAS_SIG(NSPEC, NSEAL), REAL (:115, :7600); allocated only when
  /// FreqShiftMethod == 1, written only then.
  W2 cas_sig;
  /// CWNB_SIG_M2(1-NTH:NSPEC, NSEAL), REAL (:116, :7603); allocated only when
  /// FreqShiftMethod == 2. cwnb_sig_m2(isp + nth - 1, ip - 1) = CWNB_SIG_M2(ISP,IP).
  W2 cwnb_sig_m2;
  /// CAD_THE(NSPEC, NSEAL), REAL (:115, :7597).
  W2 cad_the;
};

/// The Fortran's automatic arrays that the callees fill and the routine reads,
/// one column per node (a kernel does not allocate). Not Fortran arrays.
template <class MemSpace>
struct JacSpecWork {
  using memory_space = MemSpace;
  using W2 = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;
  W2 dmm;      ///< DMM(0:NK2) of node IP: dmm(k, ip-1) = DMM(k), (NK2+1, np)
  W2 dwni_m2;  ///< DWNI_M2(NK) of node IP: (NK, np)

  static JacSpecWork allocate(int nk, int nk2, int np) {
    JacSpecWork w;
    w.dmm = W2("jspec_dmm", nk2 + 1, np);
    w.dwni_m2 = W2("jspec_dwni_m2", nk, np);
    return w;
  }
};

/// CALCARRAY_JACOBI_SPECTRAL_1(DTG), :4413-4557. prop supplies the three
/// w3parall routines (see the header comment).
template <class ExecSpace, class Prop>
void calcarray_jacobi_spectral_1(const ExecSpace& exec,
                                 const PdlibGeometry<typename ExecSpace::memory_space>& g,
                                 const JacSpecArrays<typename ExecSpace::memory_space>& a,
                                 const JacSpecWork<typename ExecSpace::memory_space>& w,
                                 const JacSpecConfig& c, const Prop& prop, Real dtg) {
  using Policy = Kokkos::RangePolicy<ExecSpace>;
  using jspec_detail::cm_sig;
  using jspec_detail::cm_the;
  using jspec_detail::cp_sig;
  using jspec_detail::cp_the;
  using jspec_detail::max_ss;
  using jspec_detail::min_ss;

  const bool lsig = c.flcur || c.fllev;  // :4486
  const bool do_freq = c.fsfreqshift && lsig;
  const bool do_refr = c.fsrefraction;
  const int method = c.freq_shift_method;
  const int nth = g.nth;
  const int nk = g.nk;
  const int nspec = c.nspec;
  const Real fachfa = c.fachfa;
  const auto iplg = g.iplg;
  const auto mapfs = g.mapfs;
  const auto si = g.si;
  const auto iobp = g.iobp_loc;
  const auto iobdp = g.iobdp_loc;
  const auto iobpa = g.iobpa_loc;
  const auto i_diag = a.i_diag;
  const auto aspar = a.aspar_jac;
  const auto cas_sig = a.cas_sig;
  const auto cwnb_sig = a.cwnb_sig_m2;
  const auto cad_the = a.cad_the;
  const auto dmm = w.dmm;
  const auto dwni = w.dwni_m2;
  constexpr bool kDoLimiterRefraction = false;  // :4483, never set

  Kokkos::parallel_for(
      "pdlib_jacobi_spectral_1", Policy(exec, 0, g.np), KOKKOS_LAMBDA(const int p) {
        const int ip = p + 1;
        const int ip_glob = iplg(p);              // :4489
        const int isea = mapfs(0, ip_glob - 1);   // :4490
        const Real esi = si(p);                   // :4491
        const int kd = i_diag(p) - 1;             // PDLIB_I_DIAG(IP)
        const bool active = iobp(p) == 1 && iobdp(p) == 1 && iobpa(p) == 0;

        if (do_freq) {
          if (method == 1) {  // :4493-4512
            Real* cas = &cas_sig(0, p);
            if (active) {
              Real* d = &dmm(0, p);
              prop.freq_shift(ip, isea, cas, d, dtg);  // :4496
              for (int ith = 1; ith <= nth; ++ith) {
                for (int ik = 1; ik <= nk; ++ik) {  // :4501-4504
                  const int i = ith + (ik - 1) * nth - 1;
                  const Real cp = cp_sig(i, nspec, cas[i]);
                  const Real cm = cm_sig(i, nspec, cas[i]);
                  Real b = cp / d[ik - 1] - cm / d[ik];
                  if (ik == nk) b = b + (cm / d[nk]) * fachfa;  // :4505-4506
                  aspar(i, kd) = aspar(i, kd) + b * esi;           // :4508
                }
              }
            } else {
              for (int i = 0; i < nspec; ++i) cas[i] = kZero;  // :4510, CAS=0
            }
          } else if (method == 2) {  // :4513-4532
            Real* cw = &cwnb_sig(0, p);  // cw[isp + nth - 1] = CWNB_M2(ISP)
            if (active) {
              Real* dw = &dwni(0, p);
              prop.freq_shift_m2(ip, isea, cw, dw, dtg);  // :4515
              for (int ith = 1; ith <= nth; ++ith) {
                for (int ik = 1; ik <= nk; ++ik) {  // :4519-4524
                  const int isp = ith + (ik - 1) * nth;
                  const Real ev =
                      dw[ik - 1] * (min_ss(cw[isp - 1], kZero) - max_ss(cw[isp + nth - 1], kZero));
                  aspar(isp - 1, kd) = aspar(isp - 1, kd) - esi * ev;
                }
                // :4525-4527
                const Real ev = (dw[nk - 1] * min_ss(cw[ith + (nk - 1) * nth + nth - 1], kZero)) * fachfa;
                const int ith0 = nspec - nth;
                aspar(ith0 + ith - 1, kd) = aspar(ith0 + ith - 1, kd) + esi * ev;
              }
            } else {
              for (int i = 0; i < nspec + nth; ++i) cw[i] = kZero;  // :4530, CWNB_M2=0
            }
          }
        }

        if (do_refr) {  // :4537-4555
          Real* cad = &cad_the(0, p);
          if (active) {
            prop.refraction_pr3(ip, isea, dtg, cad, kDoLimiterRefraction);  // :4542
          } else {
            for (int i = 0; i < nspec; ++i) cad[i] = kZero;  // :4544
          }
          for (int i = 0; i < nspec; ++i) {  // :4551-4554
            const Real cp = cp_the(i, nspec, cad[i]) * dtg;
            const Real cm = cm_the(i, nspec, cad[i]) * dtg;
            const Real b = cp - cm;
            aspar(i, kd) = aspar(i, kd) + b * esi;
          }
        }
      });
}

}  // namespace ww::pdlib
