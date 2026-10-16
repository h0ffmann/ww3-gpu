// kokkos/src/ww_kokkos/pdlib_jacobi_spectral.hpp
// CALCARRAY_JACOBI_SPECTRAL_1: adds the upwind frequency-shift (method 1 or 2) and
// refraction coefficients to the diagonal block ASPAR_JAC(:,PDLIB_I_DIAG(IP)) and keeps
// CAS_SIG, CWNB_SIG_M2, CAD_THE for the solver. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd_pdlib.F90:4413-4557 at NOAA-EMC/WW3 03adda91;
// A. Roland, M. Dutour-Sikiric. Evidence:
// kokkos/port_report/CALCARRAY_JACOBI_SPECTRAL_1_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "pdlib_params.hpp"
#include "real.hpp"

namespace ww::pdlib {

namespace jspec_detail {
// SSE MIN/MAX as emitted (dest OP src ? dest : src): a NaN or a +0/-0 tie in `a` gives `b`.
// The object puts ZERO on different sides in the vector body and the scalar tail, so the
// operand order below depends on the 0-based ISP i of n = NSPEC.
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
  /// PDLIB_I_DIAG(npa) (yownodepool.F90:85): 1-based CSR position of row IP's diagonal.
  Kokkos::View<const int*, Kokkos::LayoutLeft, MemSpace> i_diag;
  /// ASPAR_JAC(NSPEC, PDLIB_NNZ), REAL (w3profsmd_pdlib.F90:114, :7589).
  W2 aspar_jac;
  /// CAS_SIG(NSPEC, NSEAL) (:115, :7600): allocated and written only for FreqShiftMethod 1.
  W2 cas_sig;
  /// CWNB_SIG_M2(1-NTH:NSPEC, NSEAL) (:116, :7603), FreqShiftMethod 2:
  /// cwnb_sig_m2(isp + nth - 1, ip - 1) = CWNB_SIG_M2(ISP,IP).
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

/// CALCARRAY_JACOBI_SPECTRAL_1(DTG), :4413-4557, one thread per node: each writes only its
/// diagonal column and its own columns. prop supplies const KOKKOS_FUNCTION members (1-based
/// ip, isea; prop_parall.hpp, prop_freq_shift.hpp), each writing its array in full:
///   freq_shift(ip, isea, cas, dmm, dtg)            CAS(1:NSPEC), DMM(0:NK2)
///   freq_shift_m2(ip, isea, cwnb_m2, dwni_m2, dtg) CWNB_M2(1-NTH:NSPEC), DWNI_M2(1:NK)
///   refraction_pr3(ip, isea, dtg, cad, do_limiter) CAD(1:NSPEC)
/// The callee writes straight into CAS_SIG/CWNB_SIG_M2/CAD_THE(:,IP), as the Fortran copies.
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
                for (int ik = 1; ik <= nk; ++ik) {  // :4501-4504, B_SIG fused per ISP
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
