// kokkos/src/ww_kokkos/pdlib_jgs_block.hpp
// PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK: the implicit block Gauss-Seidel (or Jacobi) solver of
// the unstructured-grid action balance, all components of a node at once, on this MPI
// rank's mesh. Phase 1, translated, not regtested. WW3 heritage:
// WW3-noaa/model/src/w3profsmd_pdlib.F90:5463-6406 at NOAA-EMC/WW3 03adda91; A. Roland,
// M. Dutour-Sikiric. Evidence: kokkos/port_report/PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK_KOKKOS.md.
// kRef1, kTrnk, kSt4 are the switches W3_REF1, W3_TRNK, W3_ST4. IMEM = 1 and LSLOC are
// PARAMETERs (w3parall.F90:89-90): their other branches are dead and not ported. No W3SRCE
// call: with LSLOC the source terms arrive in B_JAC and ASPAR_JAC (w3srcemd.F90:1601-1654).
// The sweep and the norm are order dependent (Gauss-Seidel, running sums): one thread, in order.
// ⚠ Undefined in the Fortran, zero here: the limiter's TAUWX, TAUWY, LLWS (USTAR, USTDIR on
// the first JSEA); the norm's DMM under FSFREQSHIFT without LSIG (nothing writes it);
// Sum_L2_GL (:6190 passes a REAL(8) as MPI_REAL4), so allreduce_norm is the call site only.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include <cstdint>

#include "pdlib_params.hpp"
#include "real.hpp"
#include "st4_spr4.hpp"

namespace ww::pdlib {

namespace jgs_detail {
// SSE MIN/MAX as emitted (dest OP src ? dest : src): a NaN or a +0/-0 tie in
// `a` gives `b`. Each call passes the operands in the order read from the object.
KOKKOS_INLINE_FUNCTION double max_sd(double a, double b) { return (a > b) ? a : b; }
KOKKOS_INLINE_FUNCTION double min_sd(double a, double b) { return (a < b) ? a : b; }
KOKKOS_INLINE_FUNCTION Real max_ss(Real a, Real b) { return (a > b) ? a : b; }

// :5922/5923, :6158/6159, i 1-based: maxpd/minpd for i <= 2*(NSPEC/2), the odd last element
// scalar with 0 as destination.
KOKKOS_INLINE_FUNCTION double cp_sig(double cas, int i, int nspec) {
  return (i <= 2 * (nspec / 2)) ? max_sd(cas, 0.0) : max_sd(0.0, cas);
}
KOKKOS_INLINE_FUNCTION double cm_sig(double cas, int i, int nspec) {
  return (i <= 2 * (nspec / 2)) ? min_sd(cas, 0.0) : min_sd(0.0, cas);
}

// 0.0081*0.1 (:6318) folded to one REAL.
inline constexpr Real kDamNum = 0x1.a8ac5ep-11f;
static_assert(kDamNum == 0.0081f * 0.1f, "0.0081*0.1 folded as REAL");
// 1./TPI (:6330) and TPIINV (:6331), constants.F90:73-75: 0x3e22f983.
inline constexpr Real kTpiInv = 0x1.45f306p-3f;
static_assert(kTpiInv == 1.0f / (2.0f * kPi), "1./TPI as REAL");
// 1E-06*GRAV (:6332), GRAV = 9.806 (constants.F90:62): 0x37248473, used widened.
inline constexpr Real kGravE6 = 0x1.4908e6p-17f;
static_assert(kGravE6 == 1E-06f * 9.806f, "1E-06*GRAV folded as REAL");
// TINY(1.) widened (:6116, the GIMPLE's 1.1754943508222875e-38).
inline constexpr double kTiny1 = 0x1p-126;

// REAL**INTEGER with a run-time exponent (:6382, W3_TRNK) as libgcc's __powisf2.
KOKKOS_INLINE_FUNCTION Real powisf2(Real x, int m) {
  unsigned int n = (m < 0) ? 0u - static_cast<unsigned int>(m) : static_cast<unsigned int>(m);
  Real y = (n % 2u) ? x : Real(1);
  while ((n >>= 1u) != 0u) {
    x = x * x;
    if (n % 2u) y = y * x;
  }
  return (m < 0) ? Real(1) / y : y;
}
}  // namespace jgs_detail

/// The run-time settings the routine reads (W3GDATMD, W3ODATMD, W3IDATMD, its
/// dummy arguments), host scalars.
struct JgsParams {
  int nth = 0, nk = 0, nspec = 0;  ///< W3GDATMD NTH, NK, NSPEC
  int np = 0, npa = 0, nseal = 0;  ///< yowNodepool np, npa; W3GDATMD NSEAL
  int nx = 0;                      ///< W3GDATMD NX: global node count (:6110)
  Real dtg = 0;                    ///< DTG, dummy
  Real facx = 0, facy = 0, vgx = 0, vgy = 0;  ///< dummies, read only by calcARRAY_JACOBI_VEC
  int imod = 0;                    ///< IMOD, read only by APPLY_BOUNDARY_CONDITION
  bool fsrefraction = false, fsfreqshift = false;  ///< W3GDATMD FSREFRACTION, FSFREQSHIFT
  bool lsig = false;               ///< FLCUR .OR. FLLEV (W3IDATMD, :5649)
  bool flsou = false;              ///< W3GDATMD FLSOU
  int freq_shift_method = 2;       ///< FreqShiftMethod (:125)
  Real refpars3 = 0;               ///< REFPARS(3), W3GDATMD under W3_REF1 (w3gdatmd.F90:724-726)
  int trunk_digits = 0;            ///< B_JGS_TRUNK_DIGITS (w3gdatmd.F90:1058), W3_TRNK only
  bool block_gauss_seidel = true;  ///< B_JGS_BLOCK_GAUSS_SEIDEL
  bool terminate_maxiter = false, terminate_difference = false, terminate_norm = false;
  bool limiter = false;            ///< B_JGS_LIMITER
  int maxiter = 0;                 ///< B_JGS_MAXITER
  double pmin = 0, diff_thr = 0, norm_thr = 0;  ///< B_JGS_PMIN, _DIFF_THR, _NORM_THR, REAL(8)
};

/// Arrays this routine reads that PdlibGeometry (pdlib_params.hpp) lacks.
/// LayoutLeft with the Fortran's extents; stored index values stay 1-based.
template <class MemSpace>
struct JgsMesh {
  using memory_space = MemSpace;
  using I1 = Kokkos::View<const int*, Kokkos::LayoutLeft, MemSpace>;
  using R1 = Kokkos::View<const Real*, Kokkos::LayoutLeft, MemSpace>;
  using R2 = Kokkos::View<const Real**, Kokkos::LayoutLeft, MemSpace>;
  /// PDLIB_IA_P(npa+1) (yownodepool.F90:82): row IP is entries ia_p(ip-1)+1 .. ia_p(ip) of
  /// JA and ASPAR_JAC.
  I1 ia_p;
  /// PDLIB_JA(PDLIB_NNZ) (:81, :1609): local node id 1..npa of each entry.
  I1 ja;
  /// PDLIB_I_DIAG(npa) (:85, :1625): entry K (1-based) where JA(K) = IP.
  I1 i_diag;
  /// JX_TO_JSEA(npa) (w3parall.F90:83, w3profsmd_pdlib.F90:289-306).
  I1 jx_to_jsea;
  /// ListISPprevDir, ListISPnextDir(NSPEC) (w3parall.F90:86): 1-based ISP of the neighbours.
  I1 isp_prev_dir;
  I1 isp_next_dir;
  /// SIG(0:NK+1), DSIP(0:NK+1), REAL (w3gdatmd.F90:2068): sig(ik), dsip(ik).
  R1 sig;
  R1 dsip;
  /// CG(0:NK+1,0:NSEA), WN(0:NK+1,0:NSEA), REAL (w3adatmd.F90:1312, :1325): cg(ik,isea).
  R2 cg;
  R2 wn;
};

/// The module arrays the routine reads and writes; W3SRCE and calcARRAY_JACOBI_VEC write
/// aspar_jac and b_jac first.
template <class MemSpace>
struct JgsArrays {
  using memory_space = MemSpace;
  using R2 = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;
  R2 va;           ///< VA(NSPEC,0:NSEALM) (w3wdatmd.F90:491): va(isp-1, jsea)
  R2 vaold;        ///< VAOLD(NSPEC,NSEAL) (:505): vaold(isp-1, jsea-1)
  R2 aspar_jac;    ///< ASPAR_JAC(NSPEC,PDLIB_NNZ) (w3profsmd_pdlib.F90:7589): (isp-1, k-1)
  R2 b_jac;        ///< B_JAC(NSPEC,NSEAL) (:7595): (isp-1, ip-1)
  /// U_JAC(NSPEC,npa) (:7607), Jacobi only. Must alias (or round-trip) the Fortran's U_JAC:
  /// the sweep skips dry nodes and :6089 copies those columns' old contents into VA.
  R2 u_jac;
  R2 cad_the;      ///< CAD_THE(NSPEC,NSEAL) (:7597)
  R2 cas_sig;      ///< CAS_SIG(NSPEC,NSEAL) (:7600), FreqShiftMethod 1 only
  R2 cwnb_sig_m2;  ///< CWNB_SIG_M2(1-NTH:NSPEC,NSEAL) (:7603): (isp-1+nth, ip-1), method 2
};

/// The locals W3SPR4 reads and writes (:5600, :5604, :5613), kept across JSEA as the
/// Fortran's are.
struct JgsSpr4Locals {
  Real emean = 0, fmean = 0, fmean1 = 0, wnmean = 0, amax = 0;
  Real ustar = 0, ustdir = 0, tauwx = 0, tauwy = 0, cd = 0, z0 = 0, charn = 0;
  Real fmeanws = 0, dlwmean = 0;
};

/// The Fortran's automatic arrays the C++ keeps (:5587-5613), allocated once by
/// the caller. cg1, wn1, aspar_diag and dmm keep their values from one pass to
/// the next, as the Fortran's locals do within one call.
template <class MemSpace>
struct JgsWork {
  using memory_space = MemSpace;
  using D1 = Kokkos::View<double*, Kokkos::LayoutLeft, MemSpace>;
  using W1 = Kokkos::View<Real*, Kokkos::LayoutLeft, MemSpace>;
  using N1 = Kokkos::View<int*, Kokkos::LayoutLeft, MemSpace>;
  N1 lconverged;   ///< LCONVERGED(NSEAL), LOGICAL as 0/1
  D1 esum;         ///< eSum(NSPEC)
  D1 acloc;        ///< ACLOC(NSPEC)
  W1 aspar_diag;   ///< ASPAR_DIAG(NSPEC)
  W1 dmm;          ///< DMM(0:NK2), NK2 = NK+2 (w3gdatmd.F90:2125): dmm(ik)
  W1 cg1;          ///< CG1(0:NK+1): cg1(ik)
  W1 wn1;          ///< WN1(0:NK+1): wn1(ik)
  W1 spec;         ///< SPEC(NSPEC)
  W1 dam;          ///< DAM(NSPEC)
  W1 dam2;         ///< DAM2(NSPEC)
  N1 llws;         ///< LLWS(NSPEC), LOGICAL as 0/1, never assigned in the Fortran
  Kokkos::View<int, MemSpace> is_converged;  ///< the sweep's count, read back
  Kokkos::View<double, MemSpace> sum_l2;     ///< the norm's Sum_L2, read back
  Kokkos::View<JgsSpr4Locals, MemSpace> spr4;

  static JgsWork allocate(int nspec, int nk, int nseal) {
    JgsWork w;
    w.lconverged = N1("jgs_lconverged", nseal);
    w.esum = D1("jgs_esum", nspec);
    w.acloc = D1("jgs_acloc", nspec);
    w.aspar_diag = W1("jgs_aspar_diag", nspec);
    w.dmm = W1("jgs_dmm", nk + 3);
    w.cg1 = W1("jgs_cg1", nk + 2);
    w.wn1 = W1("jgs_wn1", nk + 2);
    w.spec = W1("jgs_spec", nspec);
    w.dam = W1("jgs_dam", nspec);
    w.dam2 = W1("jgs_dam2", nspec);
    w.llws = N1("jgs_llws", nspec);
    w.is_converged = Kokkos::View<int, MemSpace>("jgs_is_converged");
    w.sum_l2 = Kokkos::View<double, MemSpace>("jgs_sum_l2");
    w.spr4 = Kokkos::View<JgsSpr4Locals, MemSpace>("jgs_spr4");
    return w;
  }
};

/// The W3SPR4 call of :6306-6313 onto the Kokkos W3SPR4 (st4_spr4.hpp). spec is
/// SPEC(NSPEC); cg and wn point at CG1(0) and WN1(0), as the Fortran passes
/// them. ⚠ st4_spr4.hpp was checked under switch_lab_shrd, not switch_PDLIB.
struct JgsSpr4St4 {
  st4::Params p;
  st4::Fields f;
  Kokkos::View<const Real*, Kokkos::LayoutLeft, st4::DeviceSpace> u10;   ///< U10(NSEA): u10(isea-1)
  Kokkos::View<const Real*, Kokkos::LayoutLeft, st4::DeviceSpace> u10d;  ///< U10D(NSEA)
  Kokkos::View<Real*, Kokkos::LayoutLeft, st4::DeviceSpace> work;        ///< spr4_work_reals(p)
  KOKKOS_INLINE_FUNCTION void operator()(int isea, const Real* spec, const Real* cg,
                                         const Real* wn, const int* llws,
                                         JgsSpr4Locals& l) const {
    st4::w3spr4_point(p, f, spec, cg, wn, u10(isea - 1), u10d(isea - 1), l.tauwx, l.tauwy, llws,
                      l.ustar, l.ustdir, l.emean, l.fmean, l.fmean1, l.wnmean, l.amax, l.cd, l.z0,
                      l.charn, l.fmeanws, l.dlwmean, work.data(), nullptr);
  }
};

/// For callers that never set FLSOU and B_JGS_LIMITER together.
struct JgsSpr4None {
  KOKKOS_INLINE_FUNCTION void operator()(int, const Real*, const Real*, const Real*, const int*,
                                         JgsSpr4Locals&) const {
    Kokkos::abort("pdlib_jgs_block: B_JGS_LIMITER needs a W3SPR4 callback");
  }
};

/// :5694-5708: VA(ISP,JSEA) = VA(ISP,JSEA) / CG(IK,ISEA) * CLATS(ISEA), a float
/// division then product. The Fortran also stores CG1(IK) here; every later read
/// of CG1(1:NK) follows a newer store, so the C++ omits it.
template <class ExecSpace>
void jgs_to_action(const ExecSpace& exec, const JgsParams& p,
                   const PdlibGeometry<typename ExecSpace::memory_space>& g,
                   const JgsMesh<typename ExecSpace::memory_space>& m,
                   const JgsArrays<typename ExecSpace::memory_space>& a) {
  const int nspec = p.nspec, nth = p.nth;
  const auto iplg = g.iplg;
  const auto mapfs = g.mapfs;
  const auto clats = g.clats;
  const auto cg = m.cg;
  const auto va = a.va;
  Kokkos::parallel_for(
      "pdlib_jgs_to_action", Kokkos::RangePolicy<ExecSpace>(exec, 1, p.nseal + 1),
      KOKKOS_LAMBDA(const int jsea) {
        const int isea = mapfs(0, iplg(jsea - 1) - 1);
        for (int isp = 1; isp <= nspec; ++isp) {
          const int ik = 1 + (isp - 1) / nth;
          va(isp - 1, jsea) = (va(isp - 1, jsea) / cg(ik, isea)) * clats(isea);
        }
      });
  exec.fence();
}

/// :5713: VAOLD = VA(1:NSPEC,1:NSEAL).
template <class ExecSpace>
void jgs_save_vaold(const ExecSpace& exec, const JgsParams& p,
                    const JgsArrays<typename ExecSpace::memory_space>& a) {
  const int nspec = p.nspec;
  const auto va = a.va;
  const auto vaold = a.vaold;
  Kokkos::parallel_for(
      "pdlib_jgs_vaold", Kokkos::RangePolicy<ExecSpace>(exec, 1, p.nseal + 1),
      KOKKOS_LAMBDA(const int jsea) {
        for (int isp = 0; isp < nspec; ++isp) vaold(isp, jsea - 1) = va(isp, jsea);
      });
  exec.fence();
}

/// :5803-5808: LCONVERGED(1:np) = .false.
template <class ExecSpace>
void jgs_reset_converged(const ExecSpace& exec, const JgsParams& p,
                         const JgsWork<typename ExecSpace::memory_space>& w) {
  const auto lconv = w.lconverged;
  Kokkos::parallel_for(
      "pdlib_jgs_lconv", Kokkos::RangePolicy<ExecSpace>(exec, 0, p.np),
      KOKKOS_LAMBDA(const int i) { lconv(i) = 0; });
  exec.fence();
}

/// :5816-6069, one sweep over IP = 1..np on one thread, in order. Returns
/// is_converged, this rank's count.
template <bool kRef1, class ExecSpace>
int jgs_sweep(const ExecSpace& exec, const JgsParams& p,
              const PdlibGeometry<typename ExecSpace::memory_space>& g,
              const JgsMesh<typename ExecSpace::memory_space>& m,
              const JgsArrays<typename ExecSpace::memory_space>& a,
              const JgsWork<typename ExecSpace::memory_space>& w, int nbiter) {
  using jgs_detail::cm_sig;
  using jgs_detail::cp_sig;
  using jgs_detail::max_sd;
  using jgs_detail::min_sd;
  const int np = p.np, nk = p.nk, nth = p.nth, nspec = p.nspec;
  const double dtg = static_cast<double>(p.dtg);
  const bool fsrefraction = p.fsrefraction;
  const bool freqshift = p.fsfreqshift && p.lsig;
  const int method = p.freq_shift_method;
  const bool bgs = p.block_gauss_seidel;
  const bool tdiff = p.terminate_difference;
  const double diff_thr = p.diff_thr;
  const auto iplg = g.iplg;
  const auto mapfs = g.mapfs;
  const auto si = g.si;
  const auto iobdp = g.iobdp_loc;
  const auto iobpd = g.iobpd_loc;
  const auto iobpa = g.iobpa_loc;
  const Real refpars3 = p.refpars3;
  const auto vaold = a.vaold;
  const auto ia_p = m.ia_p;
  const auto ja = m.ja;
  const auto i_diag = m.i_diag;
  const auto jx_to_jsea = m.jx_to_jsea;
  const auto prev_dir = m.isp_prev_dir;
  const auto next_dir = m.isp_next_dir;
  const auto dsip = m.dsip;
  const auto cg = m.cg;
  const auto wn = m.wn;
  const auto va = a.va;
  const auto aspar = a.aspar_jac;
  const auto b_jac = a.b_jac;
  const auto u_jac = a.u_jac;
  const auto cad_the = a.cad_the;
  const auto cas_sig = a.cas_sig;
  const auto cwnb = a.cwnb_sig_m2;
  const auto lconv = w.lconverged;
  const auto esum = w.esum;
  const auto acloc = w.acloc;
  const auto aspar_diag = w.aspar_diag;
  const auto dmm = w.dmm;
  const auto cg1 = w.cg1;
  const auto wn1 = w.wn1;
  const auto count = w.is_converged;
  Kokkos::parallel_for(
      "pdlib_jgs_sweep", Kokkos::RangePolicy<ExecSpace>(exec, 0, 1), KOKKOS_LAMBDA(const int) {
        int is_converged = 0;
        for (int ip = 1; ip <= np; ++ip) {
          const int ip_glob = iplg(ip - 1);
          const int isea = mapfs(0, ip_glob - 1);
          if (iobdp(ip - 1) == 0) {  // :5820-5824
            is_converged = is_converged + 1;
            lconv(ip - 1) = 1;
            continue;
          }
          for (int ik = 0; ik <= nk + 1; ++ik) {  // :5826-5833
            cg1(ik) = cg(ik, isea);
            wn1(ik) = wn(ik, isea);
          }
          const int jsea = jx_to_jsea(ip - 1);  // :5835
          for (int isp = 0; isp < nspec; ++isp) acloc(isp) = static_cast<double>(va(isp, jsea));

          if (lconv(ip - 1) == 0) {  // :5840
            const double esi = static_cast<double>(si(ip - 1));
            // :5868-5894, IMEM == 1
            for (int isp = 0; isp < nspec; ++isp) esum(isp) = static_cast<double>(b_jac(isp, ip - 1));
            const int kd = i_diag(ip - 1) - 1;
            for (int isp = 0; isp < nspec; ++isp) aspar_diag(isp) = aspar(isp, kd);
            for (int i = ia_p(ip - 1) + 1; i <= ia_p(ip); ++i) {
              const int jp = ja(i - 1);
              if (jp != ip) {
                for (int isp = 0; isp < nspec; ++isp) {
                  const double eprod = static_cast<double>(aspar(isp, i - 1) * va(isp, jp));
                  esum(isp) = esum(isp) - eprod;
                }
              }
            }
            if (fsrefraction) {  // :5902-5915
              const double dtsi = dtg * esi;
              for (int isp = 1; isp <= nspec; ++isp) {
                const int pd = prev_dir(isp - 1);
                const int nd = next_dir(isp - 1);
                const double ea_the =
                    -(dtsi * max_sd(static_cast<double>(cad_the(pd - 1, ip - 1)), 0.0));
                const double ec_the = dtsi * min_sd(static_cast<double>(cad_the(nd - 1, ip - 1)), 0.0);
                double e = esum(isp - 1);
                e = e - static_cast<double>(va(pd - 1, ip)) * ea_the;
                e = e - static_cast<double>(va(nd - 1, ip)) * ec_the;
                esum(isp - 1) = e;
              }
            }
            if (freqshift) {  // :5919-5973
              if (method == 1) {
                for (int ik = 0; ik <= nk; ++ik) dmm(ik + 1) = wn1(ik + 1) - wn1(ik);
                dmm(nk + 2) = 0;
                dmm(0) = dmm(1);
                for (int ith = 1; ith <= nth; ++ith) {
                  for (int ik = 2; ik <= nk; ++ik) {
                    const int isp = ith + (ik - 1) * nth;
                    const int ispm1 = ith + (ik - 2) * nth;
                    const double efm1 = static_cast<double>(cg1(ik - 1) / cg1(ik));
                    const double cp =
                        cp_sig(static_cast<double>(cas_sig(ispm1 - 1, ip - 1)), ispm1, nspec);
                    const double ea_sig =
                        -(((cp * esi) / static_cast<double>(dmm(ik - 1))) * efm1);
                    esum(isp - 1) = esum(isp - 1) - static_cast<double>(va(ispm1 - 1, ip)) * ea_sig;
                  }
                  for (int ik = 1; ik <= nk - 1; ++ik) {
                    const int isp = ith + (ik - 1) * nth;
                    const int ispp1 = ith + ik * nth;
                    const double efp1 = static_cast<double>(cg1(ik + 1) / cg1(ik));
                    const double cm =
                        cm_sig(static_cast<double>(cas_sig(ispp1 - 1, ip - 1)), ispp1, nspec);
                    const double ec_sig = ((cm * esi) / static_cast<double>(dmm(ik))) * efp1;
                    esum(isp - 1) = esum(isp - 1) - static_cast<double>(va(ispp1 - 1, ip)) * ec_sig;
                  }
                }
              } else if (method == 2) {
                // CWNB_M2(ISP) = CWNB_SIG_M2(ISP,IP) and DWNI_M2(IK) = DBLE(CG1(IK)/DSIP(IK))
                // (:5946-5949) are read where used: the same values.
                for (int ith = 1; ith <= nth; ++ith) {
                  for (int ik = 2; ik <= nk; ++ik) {
                    const int isp = ith + (ik - 1) * nth;
                    const int ispm1 = ith + (ik - 2) * nth;
                    const double efm1 = static_cast<double>(cg1(ik - 1) / cg1(ik));
                    const double dwni = static_cast<double>(cg1(ik) / dsip(ik));
                    const double c = static_cast<double>(cwnb(ispm1 - 1 + nth, ip - 1));
                    const double ea_sig = -(((dwni * esi) * max_sd(c, 0.0)) * efm1);
                    esum(isp - 1) = esum(isp - 1) - static_cast<double>(va(ispm1 - 1, ip)) * ea_sig;
                  }
                  for (int ik = 1; ik <= nk - 1; ++ik) {
                    const int isp = ith + (ik - 1) * nth;
                    const int ispp1 = ith + ik * nth;
                    const double efp1 = static_cast<double>(cg1(ik + 1) / cg1(ik));
                    const double dwni = static_cast<double>(cg1(ik) / dsip(ik));
                    const double c = static_cast<double>(cwnb(isp - 1 + nth, ip - 1));
                    const double ec_sig = ((dwni * esi) * min_sd(c, 0.0)) * efp1;
                    esum(isp - 1) = esum(isp - 1) - static_cast<double>(va(ispp1 - 1, ip)) * ec_sig;
                  }
                }
              }
            }
            for (int isp = 0; isp < nspec; ++isp)  // :5980
              esum(isp) = esum(isp) / static_cast<double>(aspar_diag(isp));
            if (bgs) {  // :6029-6044
              const Real dp = static_cast<Real>(iobdp(ip - 1));
              for (int isp = 0; isp < nspec; ++isp) va(isp, ip) = dp * static_cast<Real>(esum(isp));
              if (kRef1) {  // :6031-6039, VAOLD(ISP,IP)*IOBDP_LOC(IP) in float
                for (int ik = 1; ik <= nk; ++ik) {
                  for (int ith = 1; ith <= nth; ++ith) {
                    const int isp = ith + (ik - 1) * nth;
                    if (refpars3 < Real(0.5) && iobpd(ith - 1, ip - 1) == 0 && iobpa(ip - 1) == 0)
                      va(isp - 1, ip) = vaold(isp - 1, ip - 1) * dp;
                  }
                }
              }
              for (int isp = 0; isp < nspec; ++isp) esum(isp) = static_cast<double>(va(isp, ip));
            } else {  // :6043
              for (int isp = 0; isp < nspec; ++isp) u_jac(isp, ip - 1) = static_cast<Real>(esum(isp));
            }
          } else {  // :6046
            for (int isp = 0; isp < nspec; ++isp) esum(isp) = static_cast<double>(va(isp, ip));
          }

          if (tdiff) {  // :6049-6069
            double sum_new = 0.0;
            for (int isp = 0; isp < nspec; ++isp) sum_new = sum_new + esum(isp);
            double p_is_converged;
            if (sum_new > 0.0) {
              double s = 0.0;
              for (int isp = 0; isp < nspec; ++isp) s = s + (acloc(isp) - esum(isp));
              p_is_converged = Kokkos::fabs(s) / sum_new;
            } else {
              p_is_converged = 0.0;
            }
            if (p_is_converged < diff_thr && nbiter > 1) {
              is_converged = is_converged + 1;
              lconv(ip - 1) = 1;
            } else {
              lconv(ip - 1) = 0;
            }
          }
        }
        count() = is_converged;
      });
  int host = 0;
  Kokkos::deep_copy(exec, host, w.is_converged);
  exec.fence();
  return host;
}

/// :6089: VA(:,1:NPA) = U_JAC, after the exchange of U_JAC. Dry-node columns
/// carry U_JAC's earlier contents into VA, so u_jac must alias (or round-trip)
/// the Fortran's U_JAC.
template <class ExecSpace>
void jgs_copy_u_jac(const ExecSpace& exec, const JgsParams& p,
                    const JgsArrays<typename ExecSpace::memory_space>& a) {
  const int nspec = p.nspec;
  const auto va = a.va;
  const auto u_jac = a.u_jac;
  Kokkos::parallel_for(
      "pdlib_jgs_u_jac", Kokkos::RangePolicy<ExecSpace>(exec, 1, p.npa + 1),
      KOKKOS_LAMBDA(const int ip) {
        for (int isp = 0; isp < nspec; ++isp) va(isp, ip) = u_jac(isp, ip - 1);
      });
  exec.fence();
}

/// :6131-6189, the residual norm, one thread, IP = 1..np in order. Returns this
/// rank's Sum_L2. Reads ASPAR_DIAG and DMM as the last sweep left them.
template <class ExecSpace>
double jgs_norm(const ExecSpace& exec, const JgsParams& p,
                const PdlibGeometry<typename ExecSpace::memory_space>& g,
                const JgsMesh<typename ExecSpace::memory_space>& m,
                const JgsArrays<typename ExecSpace::memory_space>& a,
                const JgsWork<typename ExecSpace::memory_space>& w) {
  using jgs_detail::cm_sig;
  using jgs_detail::cp_sig;
  using jgs_detail::max_sd;
  using jgs_detail::min_sd;
  const int np = p.np, nk = p.nk, nth = p.nth, nspec = p.nspec;
  const double dtg = static_cast<double>(p.dtg);
  const bool fsrefraction = p.fsrefraction;
  const bool fsfreqshift = p.fsfreqshift;
  const auto iplg = g.iplg;
  const auto mapfs = g.mapfs;
  const auto si = g.si;
  const auto iobp = g.iobp_loc;
  const auto iobpd = g.iobpd_loc;
  const auto ia_p = m.ia_p;
  const auto ja = m.ja;
  const auto prev_dir = m.isp_prev_dir;
  const auto next_dir = m.isp_next_dir;
  const auto cg = m.cg;
  const auto wn = m.wn;
  const auto va = a.va;
  const auto aspar = a.aspar_jac;
  const auto b_jac = a.b_jac;
  const auto cad_the = a.cad_the;
  const auto cas_sig = a.cas_sig;
  const auto esum = w.esum;
  const auto acloc = w.acloc;
  const auto aspar_diag = w.aspar_diag;
  const auto dmm = w.dmm;
  const auto cg1 = w.cg1;
  const auto wn1 = w.wn1;
  const auto out = w.sum_l2;
  Kokkos::parallel_for(
      "pdlib_jgs_norm", Kokkos::RangePolicy<ExecSpace>(exec, 0, 1), KOKKOS_LAMBDA(const int) {
        double sum_l2 = 0.0;
        for (int ip = 1; ip <= np; ++ip) {
          const int ip_glob = iplg(ip - 1);
          if (iobp(ip - 1) != 1) continue;
          const double esi = static_cast<double>(si(ip - 1));
          for (int isp = 0; isp < nspec; ++isp) esum(isp) = static_cast<double>(b_jac(isp, ip - 1));
          for (int isp = 0; isp < nspec; ++isp) acloc(isp) = static_cast<double>(va(isp, ip));
          const int isea = mapfs(0, ip_glob - 1);
          for (int isp = 0; isp < nspec; ++isp)  // :6140
            esum(isp) = esum(isp) - static_cast<double>(aspar_diag(isp)) * acloc(isp);
          for (int i = ia_p(ip - 1) + 1; i <= ia_p(ip); ++i) {  // :6141-6144, JP == IP included
            const int jp = ja(i - 1);
            for (int isp = 0; isp < nspec; ++isp)
              esum(isp) = esum(isp) - static_cast<double>(aspar(isp, i - 1) * va(isp, jp));
          }
          if (fsrefraction) {  // :6145-6155
            const double dtsi = dtg * esi;
            for (int isp = 1; isp <= nspec; ++isp) {
              const int pd = prev_dir(isp - 1);
              const int nd = next_dir(isp - 1);
              const double ea_the =
                  -(dtsi * max_sd(static_cast<double>(cad_the(pd - 1, ip - 1)), 0.0));
              const double ec_the = dtsi * min_sd(static_cast<double>(cad_the(nd - 1, ip - 1)), 0.0);
              double e = esum(isp - 1);
              e = e - static_cast<double>(va(pd - 1, ip)) * ea_the;
              e = e - static_cast<double>(va(nd - 1, ip)) * ec_the;
              esum(isp - 1) = e;
            }
          }
          if (fsfreqshift) {  // :6156-6186, no LSIG and no method test in the Fortran
            for (int ik = 0; ik <= nk + 1; ++ik) {
              cg1(ik) = cg(ik, isea);
              wn1(ik) = wn(ik, isea);
            }
            for (int ith = 1; ith <= nth; ++ith) {
              if (iobpd(ith - 1, ip - 1) == 0) continue;
              for (int ik = 2; ik <= nk; ++ik) {
                const int isp = ith + (ik - 1) * nth;
                const int ispm1 = ith + (ik - 2) * nth;
                const double efm1 = static_cast<double>(cg(ik - 1, isea) / cg1(ik));
                const double cp =
                    cp_sig(static_cast<double>(cas_sig(ispm1 - 1, ip - 1)), ispm1, nspec);
                const double ea_sig = -(((cp * esi) / static_cast<double>(dmm(ik - 1))) * efm1);
                esum(isp - 1) = esum(isp - 1) - static_cast<double>(va(ispm1 - 1, ip)) * ea_sig;
              }
              for (int ik = 1; ik <= nk - 1; ++ik) {
                const int isp = ith + (ik - 1) * nth;
                const int ispp1 = ith + ik * nth;
                const double efp1 = static_cast<double>(cg(ik + 1, isea) / cg1(ik));
                const double cm =
                    cm_sig(static_cast<double>(cas_sig(ispp1 - 1, ip - 1)), ispp1, nspec);
                const double ec_sig = ((cm * esi) / static_cast<double>(dmm(ik))) * efp1;
                esum(isp - 1) = esum(isp - 1) - static_cast<double>(va(ispp1 - 1, ip)) * ec_sig;
              }
            }
          }
          double val = 0.0;  // :6187, SUM(eSum*eSum) from 0, then added
          for (int isp = 0; isp < nspec; ++isp) val = val + esum(isp) * esum(isp);
          sum_l2 = sum_l2 + val;
        }
        out() = sum_l2;
      });
  double host = 0;
  Kokkos::deep_copy(exec, host, w.sum_l2);
  exec.fence();
  return host;
}

/// :6213-6229, each IP its own column. With REF1 (:6220-6224) VAOLD(ISP,IP) replaces the
/// masked value where the reflection test holds; VAOLD is (NSPEC,NSEAL), so npa <= NSEAL
/// is assumed, as in the Fortran (:248-279).
template <bool kRef1, class ExecSpace>
void jgs_mask(const ExecSpace& exec, const JgsParams& p,
              const PdlibGeometry<typename ExecSpace::memory_space>& g,
              const JgsArrays<typename ExecSpace::memory_space>& a) {
  using jgs_detail::max_ss;
  const int nspec = p.nspec, nth = p.nth;
  const auto iobdp = g.iobdp_loc;
  const auto iobpd = g.iobpd_loc;
  const auto iobpa = g.iobpa_loc;
  const Real refpars3 = p.refpars3;
  const auto vaold = a.vaold;
  const auto va = a.va;
  Kokkos::parallel_for(
      "pdlib_jgs_mask", Kokkos::RangePolicy<ExecSpace>(exec, 1, p.npa + 1),
      KOKKOS_LAMBDA(const int ip) {
        const Real dp = static_cast<Real>(iobdp(ip - 1));
        for (int isp = 1; isp <= nspec; ++isp) {
          const int ith = 1 + (isp - 1) % nth;
          const Real t = dp * max_ss(va(isp - 1, ip), Real(0));
          va(isp - 1, ip) = static_cast<Real>(static_cast<double>(t) *
                                              static_cast<double>(iobpd(ith - 1, ip - 1)));
          if (kRef1) {
            if (refpars3 < Real(0.5) && iobpd(ith - 1, ip - 1) == 0 && iobpa(ip - 1) == 0)
              va(isp - 1, ip) = vaold(isp - 1, ip - 1);
          }
        }
      });
  exec.fence();
}

namespace jgs_detail {
// :6254-6281 for one JSEA (IP = JSEA): back to energy density, VAOLD first.
template <class G, class M, class A>
KOKKOS_INLINE_FUNCTION int to_energy(const G& g, const M& m, const A& a, int jsea, int nspec,
                                     int nth) {
  const int ip = jsea;
  const int isea = g.mapfs(0, g.iplg(ip - 1) - 1);
  const Real cl = g.clats(isea);
  for (int isp = 1; isp <= nspec; ++isp) {
    const int ik = 1 + (isp - 1) / nth;
    const Real f = m.cg(ik, isea) / cl;
    const Real eva = max_ss(f * a.va(isp - 1, ip), Real(0));
    const Real evo = max_ss(f * a.vaold(isp - 1, jsea - 1), Real(0));
    a.vaold(isp - 1, jsea - 1) = evo;
    a.va(isp - 1, jsea) = eva;
  }
  return isea;
}
}  // namespace jgs_detail

/// :6238-6352 for every JSEA, when FLSOU .and. B_JGS_LIMITER is false: each
/// JSEA writes only its own column.
template <class ExecSpace>
void jgs_to_energy(const ExecSpace& exec, const JgsParams& p,
                   const PdlibGeometry<typename ExecSpace::memory_space>& g,
                   const JgsMesh<typename ExecSpace::memory_space>& m,
                   const JgsArrays<typename ExecSpace::memory_space>& a) {
  const int nspec = p.nspec, nth = p.nth;
  const auto gg = g;
  const auto mm = m;
  const auto aa = a;
  Kokkos::parallel_for(
      "pdlib_jgs_to_energy", Kokkos::RangePolicy<ExecSpace>(exec, 1, p.nseal + 1),
      KOKKOS_LAMBDA(const int jsea) { (void)jgs_detail::to_energy(gg, mm, aa, jsea, nspec, nth); });
  exec.fence();
}

/// :6238-6352 with FLSOU .and. B_JGS_LIMITER, one thread, JSEA in order: the locals carry
/// across JSEA as the Fortran's do (CG1(0), CG1(NK+1) and WN1 stay those of the last node
/// the sweep or the norm loaded). Without ST4 there is no W3SPR4 call: FMEAN, FMEANWS and
/// USTAR keep what the locals held, undefined in the Fortran (⚠), zero here.
template <bool kSt4, class ExecSpace, class Spr4>
void jgs_to_energy_limited(const ExecSpace& exec, const JgsParams& p,
                           const PdlibGeometry<typename ExecSpace::memory_space>& g,
                           const JgsMesh<typename ExecSpace::memory_space>& m,
                           const JgsArrays<typename ExecSpace::memory_space>& a,
                           const JgsWork<typename ExecSpace::memory_space>& w, const Spr4& spr4) {
  using jgs_detail::kDamNum;
  using jgs_detail::kGravE6;
  using jgs_detail::kTpiInv;
  using jgs_detail::max_sd;
  using jgs_detail::max_ss;
  using jgs_detail::min_sd;
  const int nseal = p.nseal, nspec = p.nspec, nth = p.nth, nk = p.nk;
  const double dtg = static_cast<double>(p.dtg);
  const auto gg = g;
  const auto mm = m;
  const auto aa = a;
  const auto cg1 = w.cg1;
  const auto wn1 = w.wn1;
  const auto spec = w.spec;
  const auto dam = w.dam;
  const auto dam2 = w.dam2;
  const auto llws = w.llws;
  const auto loc = w.spr4;
  Kokkos::parallel_for(
      "pdlib_jgs_limiter", Kokkos::RangePolicy<ExecSpace>(exec, 0, 1), KOKKOS_LAMBDA(const int) {
        JgsSpr4Locals& l = loc();
        for (int jsea = 1; jsea <= nseal; ++jsea) {
          const int ip = jsea;
          const int isea = jgs_detail::to_energy(gg, mm, aa, jsea, nspec, nth);
          for (int isp = 1; isp <= nspec; ++isp) cg1(1 + (isp - 1) / nth) = mm.cg(1 + (isp - 1) / nth, isea);
          for (int isp = 0; isp < nspec; ++isp) spec(isp) = aa.vaold(isp, jsea - 1);  // :6302-6305
          if (kSt4) spr4(isea, spec.data(), cg1.data(), wn1.data(), llws.data(), l);  // :6306-6313
          const Real cl = gg.clats(isea);
          for (int isp = 0; isp < nspec; ++isp) dam(isp) = 0;  // :6316-6326
          for (int ik = 1; ik <= nk; ++ik) {
            const Real wnv = mm.wn(ik, isea);
            const Real wn3 = (wnv * wnv) * wnv;
            const Real den = (wn3 * (mm.sig(ik) * Real(2))) * mm.cg(ik, isea);
            dam((ik - 1) * nth) = ((kDamNum / den) * cg1(ik)) / cl;
          }
          for (int ik = 1; ik <= nk; ++ik)
            for (int ith = 2; ith <= nth; ++ith) dam(ith - 1 + (ik - 1) * nth) = dam((ik - 1) * nth);
          for (int isp = 0; isp < nspec; ++isp) dam2(isp) = 0;  // :6328-6339
          const double fm = static_cast<double>(max_ss(l.fmean, l.fmeanws));
          const double us = static_cast<double>(l.ustar);
          for (int ik = 1; ik <= nk; ++ik) {
            const Real jac2 = kTpiInv / mm.sig(ik);
            const double fr = static_cast<double>(mm.sig(ik) * kTpiInv);
            const double fr4 = (fr * fr) * (fr * fr);
            double v = static_cast<double>(kGravE6) / fr4;
            v = v * us;
            v = v * fm;
            v = v * dtg;
            v = v * static_cast<double>(jac2);
            v = v * static_cast<double>(cg1(ik));
            v = v / static_cast<double>(cl);
            dam2((ik - 1) * nth) = static_cast<Real>(v);
          }
          for (int ik = 1; ik <= nk; ++ik)
            for (int ith = 2; ith <= nth; ++ith) dam2(ith - 1 + (ik - 1) * nth) = dam2((ik - 1) * nth);
          for (int ik = 1; ik <= nk; ++ik) {  // :6341-6349
            for (int ith = 1; ith <= nth; ++ith) {
              const int isp = ith + (ik - 1) * nth;
              const Real vo = aa.vaold(isp - 1, jsea - 1);
              double newdac = static_cast<double>(aa.va(isp - 1, ip) - vo);
              const double maxdac = static_cast<double>(max_ss(dam(isp - 1), dam2(isp - 1)));
              newdac = Kokkos::copysign(min_sd(maxdac, Kokkos::fabs(newdac)), newdac);
              aa.va(isp - 1, ip) = static_cast<Real>(max_sd(static_cast<double>(vo) + newdac, 0.0));
            }
          }
        }
      });
  exec.fence();
}

/// :6373-6387 (W3_TRNK), each IP its own column: VA = ANINT(VA/TRVA)*TRVA with
/// TRVA = 10.**(NINT(LOG10(VA))-B_JGS_TRUNK_DIGITS). ⚠ log10f on CUDA is not glibc's.
template <class ExecSpace>
void jgs_truncate(const ExecSpace& exec, const JgsParams& p,
                  const JgsArrays<typename ExecSpace::memory_space>& a) {
  const int nspec = p.nspec;
  const int digits = p.trunk_digits;
  const auto va = a.va;
  Kokkos::parallel_for(
      "pdlib_jgs_trnk", Kokkos::RangePolicy<ExecSpace>(exec, 1, p.npa + 1),
      KOKKOS_LAMBDA(const int ip) {
        const Real tiny = 0x1p-126f;
        for (int isp = 0; isp < nspec; ++isp) {
          const Real v = va(isp, ip);
          if (v > tiny) {
            const int expva = st4::nint(Kokkos::log10(v));
            const Real trva = jgs_detail::powisf2(Real(10), expva - digits);
            if (trva > tiny) va(isp, ip) = Kokkos::round(v / trva) * trva;
          }
        }
      });
  exec.fence();
}

/// The Fortran calls the driver stops at (see the header comment). A Hooks type
/// provides, all on the host:
///   void exchange_va_zero();             :5712, :5792, :6086
///   void exchange_u_jac();               :6088
///   template <bool kRef1> void calcarray_jacobi_vec();  :5767, pdlib_jacobi_vec.hpp's
///                                        REF1 flag (DTG, FACX, FACY, VGX, VGY in JgsParams)
///   void calcarray_jacobi_spectral_1();  :5783
///   void apply_boundary_condition();     :5788  (IMOD in JgsParams)
///   int allreduce_sum_int(int);          :6108
///   void report_iterations(int nbiter, int is_converged, double prop_conv);  :6115
///   double allreduce_norm(double);       :6190  (its result is undefined in the Fortran, see above)
/// Each hook sees JgsArrays as the kernels left them, and leaves them as the
/// Fortran call would.
///
/// Returns 0, or 2 when B_JGS_TERMINATE_NORM .and. FSFREQSHIFT with
/// FreqShiftMethod /= 1 (the Fortran reads the unallocated CAS_SIG).
/// kRef1, kTrnk, kSt4: the switch's W3_REF1, W3_TRNK, W3_ST4 (see the header).
template <bool kRef1, bool kTrnk, bool kSt4, class ExecSpace, class Hooks, class Spr4>
int pdlib_jacobi_gauss_seidel_block(const ExecSpace& exec, const JgsParams& p,
                                    const PdlibGeometry<typename ExecSpace::memory_space>& g,
                                    const JgsMesh<typename ExecSpace::memory_space>& m,
                                    const JgsArrays<typename ExecSpace::memory_space>& a,
                                    const JgsWork<typename ExecSpace::memory_space>& w,
                                    Hooks& hooks, const Spr4& spr4) {
  if (p.terminate_norm && p.fsfreqshift && p.freq_shift_method != 1) return 2;
  jgs_to_action(exec, p, g, m, a);                               // :5694-5708
  hooks.exchange_va_zero();                                      // :5712
  jgs_save_vaold(exec, p, a);                                    // :5713
  hooks.template calcarray_jacobi_vec<kRef1>();                  // :5767
  if (p.fsfreqshift || p.fsrefraction) hooks.calcarray_jacobi_spectral_1();  // :5781-5787
  hooks.apply_boundary_condition();                              // :5788
  hooks.exchange_va_zero();                                      // :5792
  int nbiter = 0;                                                // :5802
  jgs_reset_converged(exec, p, w);                               // :5803-5808
  for (;;) {                                                     // :5810
    int is_converged = jgs_sweep<kRef1>(exec, p, g, m, a, w, nbiter);  // :5812-6069
    if (p.block_gauss_seidel) {                                  // :6085-6090
      hooks.exchange_va_zero();
    } else {
      hooks.exchange_u_jac();
      jgs_copy_u_jac(exec, p, a);
    }
    if (p.terminate_maxiter && nbiter > p.maxiter) break;        // :6095-6102
    if (p.terminate_difference && nbiter % 10 == 0) {           // :6107-6125
      is_converged = hooks.allreduce_sum_int(is_converged);
      const double nx = static_cast<double>(p.nx);
      const double prop_conv = ((nx - static_cast<double>(is_converged)) / nx) * 100.0;
      hooks.report_iterations(nbiter, is_converged, prop_conv);
      if (prop_conv <= p.pmin + jgs_detail::kTiny1) break;
    }
    if (p.terminate_norm) {                                      // :6130-6201
      const double sum_l2 = jgs_norm(exec, p, g, m, a, w);
      const double sum_l2_gl = hooks.allreduce_norm(sum_l2);
      if (sum_l2_gl <= p.norm_thr) break;
    }
    nbiter = nbiter + 1;                                         // :6204
  }
  jgs_mask<kRef1>(exec, p, g, a);                                // :6213-6229
  if (p.flsou && p.limiter) {                                    // :6238-6352
    jgs_to_energy_limited<kSt4>(exec, p, g, m, a, w, spr4);
  } else {
    jgs_to_energy(exec, p, g, m, a);
  }
  if constexpr (kTrnk) jgs_truncate(exec, p, a);                 // :6373-6387
  return 0;
}

}  // namespace ww::pdlib
