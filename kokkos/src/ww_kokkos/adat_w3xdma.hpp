// kokkos/src/ww_kokkos/adat_w3xdma.hpp
// W3XDMA, ported to Kokkos: allocate the X* output arrays of model IMOD, each with
// NXXX = NSEALM*NAPROC points when its output flag is set and one point otherwise,
// fill them with UNDEF, and set AINIT2. Host-side, outside any kernel. Phase 1,
// translated, not regtested.
//
// WW3 heritage: SUBROUTINE W3XDMA, WW3/model/src/w3adatmd.F90:1522-2409 at the WW3/
// pin 8e433f99 (byte-identical to NOAA-EMC/WW3 03adda91); author H. L. Tolman.
// "Version of W3DIMX for extended ouput arrays only" (:1537). The body has no
// switch-dependent code but W3_S STRACE and the W3_T prints, which
// switches/switch_lab_shrd does not compile and which are not ported. Callers at the
// pin: W3INIT (w3initmd.F90:3440, :3442, :4875, under W3_MPI) and DO_OUTPUT_EXCHANGES
// (pdlib_field_vec.F90:1324, the hook w3xdma of pdlib_do_output_exchanges.hpp, whose
// FLGRDALL layout is the outflags argument here).
//
// Checked against gfortran 16.2 -O3 GIMPLE (-fdump-tree-optimized-lineno) of
// w3adatmd.F90 with WW3's Release flags and the switch_lab_shrd defines:
//  - NXXX = NSEALM*NAPROC (:1596) is an integer(kind=4) product of the two module
//    pointers' targets, read after W3SETG(IMOD): NSEALM of the last W3SETA (its
//    unconditional alias, :2754, W3setaTargets::base, which W3XETA does not move, so
//    not Wadats::iadata), NAPROC of the last W3SETO. Widened to kind 8 for each bound.
//  - AINIT2 (:1581, :2375) is WADATS(IMOD)%AINIT2 itself, not the alias, so
//    WadatScalars of model IMOD.
//  - Every ALLOCATE is its own malloc, of max(NXXX*4, 1) bytes for a full array; the
//    ones with STAT= go to EXTCDE(99) on NULL, the eleven without (:1680-1713,
//    :1983, :2276-2302) to _gfortran_os_error_at, which exits with status 1.
//  - XUSERO (:2363-2371): two ALLOCATEs of the same POINTER, no free between them, so
//    the final shape is (NXXX or 1, 2) by OUTFLAGS(10,2) alone; see below.
//  - XQP(1) = UNDEF (:2311) is one 4-byte store to the first element, not a fill.
//  - Every other "= UNDEF" is a scalar loop of 4-byte stores of one value; UNDEF is
//    loaded from the module variable before each fill but XQKK's (:2312), which reuses
//    the load of :2311. Kokkos::deep_copy of a scalar stores the same bits.
//  - memunit = 30000+IAPROC (:1570) reads IAPROC before any check and feeds only
//    print_memcheck (:2387), which writes nothing without W3_MEMCHECK
//    (w3servmd.F90:2502); not ported.
//
// The XUSERO allocation is a defect of WW3 at the pin, kept as written: I = 1
// allocates XUSERO(NXXX or 1, 1), I = 2 allocates XUSERO(NXXX or 1, 2) over it, so
// OUTFLAGS(10,1) only decides the size of an array that is then dropped. With
// OUTFLAGS(10,1) set and OUTFLAGS(10,2) unset, DO_OUTPUT_EXCHANGES's USERO(1:NSEA,1)
// writes (pdlib_field_vec.F90:1723-1728, USERO aliased to XUSERO by W3XETA) run past
// a (1,2) array. Here the first View is
// released when the second is assigned; the Fortran leaks it. No value is read from it.
//
// Not ported, by construction of a C++ library call: the WRITE(NDSE) messages and
// EXTCDE (returned as the code EXTCDE would exit with, W3xdmaStatus); W3SETG(IMOD) at
// :1591 and W3SETG(JGRID) at :2385 (the caller passes IMOD's grid in W3xdmaGrid).
#pragma once

#include <array>
#include <cstddef>
#include <exception>
#include <vector>

#include <Kokkos_Core.hpp>

#include "adat_w3dima.hpp"
#include "adat_w3naux.hpp"
#include "adat_w3seta.hpp"
#include "adat_w3xdma_wadat.hpp"
#include "adat_wadat.hpp"
#include "real.hpp"

namespace ww::adat {

/// Exit codes of W3XDMA, returned instead of stopping the program.
enum class W3xdmaStatus : int {
  ok = 0,
  grids_not_initialized = 1,  ///< NGRIDS == -1, :1571-1574
  illegal_model = 2,          ///< IMOD < 1 or IMOD > NADATA, :1576-1579
  already_allocated = 3,      ///< WADATS(IMOD)%AINIT2, :1581-1584
  alloc_failed = 99,          ///< CHECK_ALLOC_STATUS, w3macros.h:49-51
  /// Not an EXTCDE code: an ALLOCATE without STAT= failed, and gfortran's
  /// _gfortran_os_error_at prints "Error allocating %lu bytes" and exits with status 1
  /// (libgfortran exit_error(1)). w3xdma_exit_status() gives that 1.
  alloc_runtime_error = -4,
  /// Not a WW3 code: no W3SETA yet (W3setaTargets::base 0), so :1596 would read NSEALM
  /// through an unassociated pointer, undefined behaviour in the Fortran.
  nsealm_unassociated = -1,
  /// Not a WW3 code: x.models or scalars has no entry imod-1. In the Fortran the X
  /// components and AINIT2 live inside WADATS(IMOD), so this cannot happen there.
  x_not_sized = -2,
  /// Not a WW3 code: OUTFLAGS(8,5) set and NXXX <= 0, so :2311 would store XQP(1) into
  /// a zero-extent array (a 4-byte store into gfortran's 1-byte malloc).
  xqp_empty = -3,
};

/// The process exit status the Fortran ends with for a W3XDMA status (0 for ok). The
/// negative codes other than -4 stand for undefined behaviour, have no Fortran exit
/// status, and come back unchanged.
constexpr int w3xdma_exit_status(W3xdmaStatus s) {
  return s == W3xdmaStatus::alloc_runtime_error ? 1 : static_cast<int>(s);
}

/// @brief The module state of other modules W3XDMA reads, for grid IMOD.
/// @details W3GDATMD of grid IMOD (what W3SETG(IMOD) at :1591 selects), and the
///          W3ODATMD pointers of the last W3SETO (W3XDMA does not select them).
struct W3xdmaGrid {
  int ngrids = -1;                           ///< W3GDATMD NGRIDS
  int nk = 0;                                ///< NK
  std::array<std::array<int, 3>, 5> e3df{};  ///< E3DF(i,j) -> e3df[j-1][i-1]
  std::array<int, 3> p2msf{};                ///< P2MSF(i) -> p2msf[i-1]
  int naproc = 1;                            ///< W3ODATMD NAPROC (w3odatmd.F90:687 default)
  int noswll = 0;                            ///< W3ODATMD NOSWLL
  Real undef = -999.9f;                      ///< CONSTANTS UNDEF (constants.F90:85)
};

namespace xdma_detail {
// W3ODATMD PARAMETERs NOGRP and NOEXTR (w3odatmd.F90:330, :335).
inline constexpr int kNogrp = 10;
inline constexpr int kNoextr = 2;
}  // namespace xdma_detail

/// @brief W3XDMA(IMOD, NDSE, NDST, OUTFLAGS).
/// @param exec     execution space instance the fills run on; fenced before the ok
///                 return only (on an error code the fills may still be in flight).
/// @param reg      WADATS: NADATA and the NSEALM of model t.base (in).
/// @param scalars  W3NAUX's WadatScalars, parallel to reg.wadats; scalars[imod-1].ainit2
///                 is WADATS(IMOD)%AINIT2 (read :1581, set :2375).
/// @param t        W3SETA's alias targets; t.base is the model NSEALM points at (in).
/// @param x        the X parts of WADATS (in/out).
/// @param imod     model number, 1-based as in the Fortran.
/// @param g        the state of grid IMOD and of the other modules W3XDMA reads.
/// @param outflags OUTFLAGS(NOGRP,NGRPP), LOGICAL(4) as 0/1, column-major: OUTFLAGS(j,k)
///                 is outflags[(j-1)+(k-1)*NOGRP], 200 values.
/// @return W3xdmaStatus::ok, or the code the Fortran stops with. On a code other than
///         ok, x is left as far as the Fortran had got and must not be used further.
/// @author ww3-gpu; heritage H. L. Tolman (W3ADATMD)
/// @date 2026-10-16
template <class ExecSpace>
W3xdmaStatus w3xdma(const ExecSpace& exec, const Wadats<ExecSpace>& reg,
                    std::vector<WadatScalars>& scalars, const W3setaTargets& t,
                    WadatsX<ExecSpace>& x, int imod, const W3xdmaGrid& g,
                    const int* outflags) {
  using X = WadatX<ExecSpace>;
  using R1 = typename X::R1;
  using R2 = typename X::R2;
  using detail::alloc;
  using detail::fext;
  using St = W3xdmaStatus;

  if (g.ngrids == -1) return St::grids_not_initialized;          // :1571
  if (imod < 1 || imod > reg.nadata) return St::illegal_model;    // :1576
  const auto m = static_cast<std::size_t>(imod - 1);
  if (m >= x.models.size() || m >= scalars.size()) return St::x_not_sized;
  X& w = x.models[m];
  if (scalars[m].ainit2) return St::already_allocated;  // :1581

  // :1596, through the NSEALM alias of the last W3SETA, in default INTEGER.
  if (t.base < 1 || t.base > static_cast<int>(reg.wadats.size()))
    return St::nsealm_unassociated;
  const int nsealm = reg.wadats[static_cast<std::size_t>(t.base - 1)].nsealm;
  const int nxxx = nsealm * g.naproc;

  auto of = [outflags](int j, int k) {
    return outflags[(j - 1) + (k - 1) * xdma_detail::kNogrp] != 0;
  };
  // One ALLOCATE statement. With STAT=, a failure is CHECK_ALLOC_STATUS (99); without,
  // the gfortran run-time error.
  auto run = [](bool has_stat, auto&& body) -> St {
    try {
      body();
      return St::ok;
    } catch (const std::exception&) {
      return has_stat ? St::alloc_failed : St::alloc_runtime_error;
    }
  };
  // IF (cond) ALLOCATE(V(NXXX)) ELSE ALLOCATE(V(1)); stat_full and stat_one say which
  // branch has STAT=.
  auto a1 = [&](R1& v, const char* label, bool cond, bool stat_full, bool stat_one) {
    return run(cond ? stat_full : stat_one,
               [&] { v = alloc<R1>(label, cond ? fext(1, nxxx) : std::size_t{1}); });
  };
  // IF (cond) ALLOCATE(V(NXXX,lb:ub)) ELSE ALLOCATE(V(1,olb:oub)), all with STAT=.
  auto a2 = [&](R2& v, int* lb_out, const char* label, bool cond, int lb, int ub, int olb,
                int oub) {
    const St s = run(true, [&] {
      v = cond ? alloc<R2>(label, fext(1, nxxx), fext(lb, ub))
               : alloc<R2>(label, std::size_t{1}, fext(olb, oub));
    });
    if (s == St::ok && lb_out) *lb_out = cond ? lb : olb;
    return s;
  };
  auto fill = [&exec, &g](const auto& v) { Kokkos::deep_copy(exec, v, g.undef); };
  St s = St::ok;

  // Group 2, :1598-1730; (2,11)-(2,16) have no STAT=.
  if ((s = a1(w.xhs, "ww::adat::XHS", of(2, 1), true, true)) != St::ok) return s;
  if ((s = a1(w.xwlm, "ww::adat::XWLM", of(2, 2), true, true)) != St::ok) return s;
  if ((s = a1(w.xt02, "ww::adat::XT02", of(2, 3), true, true)) != St::ok) return s;
  if ((s = a1(w.xt0m1, "ww::adat::XT0M1", of(2, 4), true, true)) != St::ok) return s;
  if ((s = a1(w.xt01, "ww::adat::XT01", of(2, 5), true, true)) != St::ok) return s;
  if ((s = a1(w.xfp0, "ww::adat::XFP0", of(2, 6) || of(2, 18), true, true)) != St::ok)
    return s;  // :1638, TP shares FP0
  if ((s = a1(w.xthm, "ww::adat::XTHM", of(2, 7), true, true)) != St::ok) return s;
  if ((s = a1(w.xths, "ww::adat::XTHS", of(2, 8), true, true)) != St::ok) return s;
  if ((s = a1(w.xthp0, "ww::adat::XTHP0", of(2, 9), true, true)) != St::ok) return s;
  if ((s = a1(w.xhsig, "ww::adat::XHSIG", of(2, 10), true, true)) != St::ok) return s;
  if ((s = a1(w.xstmaxe, "ww::adat::XSTMAXE", of(2, 11), false, false)) != St::ok) return s;
  if ((s = a1(w.xstmaxd, "ww::adat::XSTMAXD", of(2, 12), false, false)) != St::ok) return s;
  if ((s = a1(w.xhmaxe, "ww::adat::XHMAXE", of(2, 13), false, false)) != St::ok) return s;
  if ((s = a1(w.xhcmaxe, "ww::adat::XHCMAXE", of(2, 14), false, false)) != St::ok) return s;
  if ((s = a1(w.xhmaxd, "ww::adat::XHMAXD", of(2, 15), false, false)) != St::ok) return s;
  if ((s = a1(w.xhcmaxd, "ww::adat::XHCMAXD", of(2, 16), false, false)) != St::ok) return s;
  if ((s = a1(w.xwbt, "ww::adat::XWBT", of(2, 17), true, true)) != St::ok) return s;
  if ((s = a1(w.xwnmean, "ww::adat::XWNMEAN", of(2, 19), true, true)) != St::ok) return s;
  for (const R1* v : {&w.xhs, &w.xwlm, &w.xt02, &w.xt0m1, &w.xt01, &w.xfp0, &w.xthm,
                      &w.xths, &w.xthp0, &w.xhsig, &w.xstmaxe, &w.xstmaxd, &w.xhmaxe,
                      &w.xhmaxd, &w.xhcmaxe, &w.xhcmaxd, &w.xwbt, &w.xwnmean})
    fill(*v);  // :1732-1749, in this order (XHMAXD before XHCMAXE)

  // Group 3, :1751-1789; full second bounds E3DF(2,k):E3DF(3,k), else (1,1).
  {
    R2* const v[5] = {&w.xef, &w.xth1m, &w.xsth1m, &w.xth2m, &w.xsth2m};
    int* const lb[5] = {&w.xef_lb, &w.xth1m_lb, &w.xsth1m_lb, &w.xth2m_lb, &w.xsth2m_lb};
    const char* const name[5] = {"ww::adat::XEF", "ww::adat::XTH1M", "ww::adat::XSTH1M",
                                 "ww::adat::XTH2M", "ww::adat::XSTH2M"};
    for (int k = 0; k < 5; ++k) {
      const auto& e = g.e3df[static_cast<std::size_t>(k)];
      if ((s = a2(*v[k], lb[k], name[k], of(3, k + 1), e[1], e[2], 1, 1)) != St::ok) return s;
    }
    for (int k = 0; k < 5; ++k) fill(*v[k]);  // :1791-1795
  }

  // Group 4, :1797-1931; full second bounds 0:NOSWLL, else (1,1).
  {
    const int nsw = g.noswll;
    if ((s = a2(w.xphs, &w.xphs_lb, "ww::adat::XPHS", of(4, 1), 0, nsw, 1, 1)) != St::ok)
      return s;
    if ((s = a2(w.xptp, &w.xptp_lb, "ww::adat::XPTP", of(4, 2), 0, nsw, 1, 1)) != St::ok)
      return s;
    if ((s = a2(w.xplp, &w.xplp_lb, "ww::adat::XPLP", of(4, 3), 0, nsw, 1, 1)) != St::ok)
      return s;
    if ((s = a2(w.xpdir, &w.xpdir_lb, "ww::adat::XPDIR", of(4, 4), 0, nsw, 1, 1)) != St::ok)
      return s;
    if ((s = a2(w.xpsi, &w.xpsi_lb, "ww::adat::XPSI", of(4, 5), 0, nsw, 1, 1)) != St::ok)
      return s;
    if ((s = a2(w.xpws, &w.xpws_lb, "ww::adat::XPWS", of(4, 6), 0, nsw, 1, 1)) != St::ok)
      return s;
    if ((s = a2(w.xpthp0, &w.xpthp0_lb, "ww::adat::XPTHP0", of(4, 7), 0, nsw, 1, 1)) !=
        St::ok)
      return s;
    if ((s = a2(w.xpqp, &w.xpqp_lb, "ww::adat::XPQP", of(4, 8), 0, nsw, 1, 1)) != St::ok)
      return s;
    if ((s = a2(w.xppe, &w.xppe_lb, "ww::adat::XPPE", of(4, 9), 0, nsw, 1, 1)) != St::ok)
      return s;
    if ((s = a2(w.xpgw, &w.xpgw_lb, "ww::adat::XPGW", of(4, 10), 0, nsw, 1, 1)) != St::ok)
      return s;
    if ((s = a2(w.xpsw, &w.xpsw_lb, "ww::adat::XPSW", of(4, 11), 0, nsw, 1, 1)) != St::ok)
      return s;
    if ((s = a2(w.xptm1, &w.xptm1_lb, "ww::adat::XPTM1", of(4, 12), 0, nsw, 1, 1)) != St::ok)
      return s;
    if ((s = a2(w.xpt1, &w.xpt1_lb, "ww::adat::XPT1", of(4, 13), 0, nsw, 1, 1)) != St::ok)
      return s;
    if ((s = a2(w.xpt2, &w.xpt2_lb, "ww::adat::XPT2", of(4, 14), 0, nsw, 1, 1)) != St::ok)
      return s;
    if ((s = a2(w.xpep, &w.xpep_lb, "ww::adat::XPEP", of(4, 15), 0, nsw, 1, 1)) != St::ok)
      return s;
  }
  if ((s = a1(w.xpwst, "ww::adat::XPWST", of(4, 16), true, true)) != St::ok) return s;
  if ((s = a1(w.xpnr, "ww::adat::XPNR", of(4, 17), true, true)) != St::ok) return s;
  fill(w.xphs);  // :1933-1949, in this order (XPWST, XPNR after XPWS)
  fill(w.xptp);
  fill(w.xplp);
  fill(w.xpdir);
  fill(w.xpsi);
  fill(w.xpws);
  fill(w.xpwst);
  fill(w.xpnr);
  fill(w.xpthp0);
  fill(w.xpqp);
  fill(w.xppe);
  fill(w.xpgw);
  fill(w.xpsw);
  fill(w.xptm1);
  fill(w.xpt1);
  fill(w.xpt2);
  fill(w.xpep);

  // Group 5, :1951-2014. OUTFLAGS(5,1) is not read. XTAUWIY(1) at :1983 has no STAT=;
  // the CHECK_ALLOC_STATUS after it (:1984) tests the ISTAT = 0 of :1981 and cannot fire.
  if ((s = a1(w.xcharn, "ww::adat::XCHARN", of(5, 2), true, true)) != St::ok) return s;
  if ((s = a1(w.xcge, "ww::adat::XCGE", of(5, 3), true, true)) != St::ok) return s;
  if ((s = a1(w.xphiaw, "ww::adat::XPHIAW", of(5, 4), true, true)) != St::ok) return s;
  if ((s = a1(w.xtauwix, "ww::adat::XTAUWIX", of(5, 5), true, true)) != St::ok) return s;
  if ((s = a1(w.xtauwiy, "ww::adat::XTAUWIY", of(5, 5), true, false)) != St::ok) return s;
  if ((s = a1(w.xtauwnx, "ww::adat::XTAUWNX", of(5, 6), true, true)) != St::ok) return s;
  if ((s = a1(w.xtauwny, "ww::adat::XTAUWNY", of(5, 6), true, true)) != St::ok) return s;
  if ((s = a2(w.xwhitecap, nullptr, "ww::adat::XWHITECAP",
              of(5, 7) || of(5, 8) || of(5, 9) || of(5, 10), 1, 4, 1, 4)) != St::ok)
    return s;  // :1999-2006
  if ((s = a1(w.xtws, "ww::adat::XTWS", of(5, 11), true, true)) != St::ok) return s;
  for (const R1* v : {&w.xcharn, &w.xtws, &w.xcge, &w.xphiaw, &w.xtauwix, &w.xtauwiy,
                      &w.xtauwnx, &w.xtauwny})
    fill(*v);  // :2016-2023, in this order (XTWS second)
  fill(w.xwhitecap);  // :2024

  // Group 6, :2026-2156. 2*NK in default INTEGER, as the Fortran.
  if ((s = a1(w.xsxx, "ww::adat::XSXX", of(6, 1), true, true)) != St::ok) return s;
  if ((s = a1(w.xsyy, "ww::adat::XSYY", of(6, 1), true, true)) != St::ok) return s;
  if ((s = a1(w.xsxy, "ww::adat::XSXY", of(6, 1), true, true)) != St::ok) return s;
  if ((s = a1(w.xtauox, "ww::adat::XTAUOX", of(6, 2), true, true)) != St::ok) return s;
  if ((s = a1(w.xtauoy, "ww::adat::XTAUOY", of(6, 2), true, true)) != St::ok) return s;
  if ((s = a1(w.xbhd, "ww::adat::XBHD", of(6, 3), true, true)) != St::ok) return s;
  if ((s = a1(w.xphioc, "ww::adat::XPHIOC", of(6, 4), true, true)) != St::ok) return s;
  if ((s = a1(w.xtusx, "ww::adat::XTUSX", of(6, 5), true, true)) != St::ok) return s;
  if ((s = a1(w.xtusy, "ww::adat::XTUSY", of(6, 5), true, true)) != St::ok) return s;
  if ((s = a1(w.xussx, "ww::adat::XUSSX", of(6, 6), true, true)) != St::ok) return s;
  if ((s = a1(w.xussy, "ww::adat::XUSSY", of(6, 6), true, true)) != St::ok) return s;
  if ((s = a1(w.xprms, "ww::adat::XPRMS", of(6, 7), true, true)) != St::ok) return s;
  if ((s = a1(w.xtpms, "ww::adat::XTPMS", of(6, 7), true, true)) != St::ok) return s;
  const int nk2 = 2 * g.nk;
  if ((s = a2(w.xus3d, nullptr, "ww::adat::XUS3D", of(6, 8), 1, nk2, 1, 1)) != St::ok)
    return s;  // :2106-2112
  if ((s = a2(w.xp2sms, &w.xp2sms_lb, "ww::adat::XP2SMS", of(6, 9), g.p2msf[1], g.p2msf[2],
              1, 1)) != St::ok)
    return s;  // :2114-2120
  if ((s = a2(w.xtauice, nullptr, "ww::adat::XTAUICE", of(6, 10), 1, 2, 1, 2)) != St::ok)
    return s;  // :2122-2128
  if ((s = a1(w.xphice, "ww::adat::XPHICE", of(6, 11), true, true)) != St::ok) return s;
  if ((s = a2(w.xussp, nullptr, "ww::adat::XUSSP", of(6, 12), 1, nk2, 1, 1)) != St::ok)
    return s;  // :2138-2144
  if ((s = a1(w.xtauocx, "ww::adat::XTAUOCX", of(6, 13), true, true)) != St::ok) return s;
  if ((s = a1(w.xtauocy, "ww::adat::XTAUOCY", of(6, 13), true, true)) != St::ok) return s;
  fill(w.xsxx);  // :2158-2177, in this order
  fill(w.xsyy);
  fill(w.xsxy);
  fill(w.xtauox);
  fill(w.xtauoy);
  fill(w.xbhd);
  fill(w.xphioc);
  fill(w.xtusx);
  fill(w.xtusy);
  fill(w.xussx);
  fill(w.xussy);
  fill(w.xprms);
  fill(w.xtpms);
  fill(w.xus3d);
  fill(w.xp2sms);
  fill(w.xphice);
  fill(w.xtauice);
  fill(w.xussp);
  fill(w.xtauocx);
  fill(w.xtauocy);

  // Group 7, :2179-2225
  if ((s = a1(w.xaba, "ww::adat::XABA", of(7, 1), true, true)) != St::ok) return s;
  if ((s = a1(w.xabd, "ww::adat::XABD", of(7, 1), true, true)) != St::ok) return s;
  if ((s = a1(w.xuba, "ww::adat::XUBA", of(7, 2), true, true)) != St::ok) return s;
  if ((s = a1(w.xubd, "ww::adat::XUBD", of(7, 2), true, true)) != St::ok) return s;
  if ((s = a2(w.xbedforms, nullptr, "ww::adat::XBEDFORMS", of(7, 3), 1, 3, 1, 3)) != St::ok)
    return s;  // :2203-2209
  if ((s = a1(w.xphibbl, "ww::adat::XPHIBBL", of(7, 4), true, true)) != St::ok) return s;
  if ((s = a2(w.xtaubbl, nullptr, "ww::adat::XTAUBBL", of(7, 5), 1, 2, 1, 2)) != St::ok)
    return s;  // :2219-2225
  fill(w.xaba);  // :2227-2233
  fill(w.xabd);
  fill(w.xuba);
  fill(w.xubd);
  fill(w.xbedforms);
  fill(w.xphibbl);
  fill(w.xtaubbl);

  // Group 8, :2235-2303; (8,5)-(8,9) have no STAT=.
  if ((s = a1(w.xmssx, "ww::adat::XMSSX", of(8, 1), true, true)) != St::ok) return s;
  if ((s = a1(w.xmssy, "ww::adat::XMSSY", of(8, 1), true, true)) != St::ok) return s;
  if ((s = a1(w.xmscx, "ww::adat::XMSCX", of(8, 2), true, true)) != St::ok) return s;
  if ((s = a1(w.xmscy, "ww::adat::XMSCY", of(8, 2), true, true)) != St::ok) return s;
  if ((s = a1(w.xmssd, "ww::adat::XMSSD", of(8, 3), true, true)) != St::ok) return s;
  if ((s = a1(w.xmscd, "ww::adat::XMSCD", of(8, 4), true, true)) != St::ok) return s;
  if ((s = a1(w.xqp, "ww::adat::XQP", of(8, 5), false, false)) != St::ok) return s;
  if ((s = a1(w.xqkk, "ww::adat::XQKK", of(8, 6), false, false)) != St::ok) return s;
  if ((s = a1(w.xskew, "ww::adat::XSKEW", of(8, 7), false, false)) != St::ok) return s;
  if ((s = a1(w.xembia1, "ww::adat::XEMBIA1", of(8, 8), false, false)) != St::ok) return s;
  if ((s = a1(w.xembia2, "ww::adat::XEMBIA2", of(8, 9), false, false)) != St::ok) return s;
  fill(w.xmssx);  // :2305-2310, in this order (XMSSD before XMSCX)
  fill(w.xmssy);
  fill(w.xmssd);
  fill(w.xmscx);
  fill(w.xmscy);
  fill(w.xmscd);
  // :2311, XQP(1) only: the rest of a full XQP stays undefined, as in the Fortran.
  if (w.xqp.extent(0) == 0) return St::xqp_empty;
  Kokkos::deep_copy(exec, Kokkos::subview(w.xqp, 0), g.undef);
  fill(w.xqkk);  // :2312-2315
  fill(w.xskew);
  fill(w.xembia1);
  fill(w.xembia2);

  // Group 9, :2317-2355
  if ((s = a1(w.xdtdyn, "ww::adat::XDTDYN", of(9, 1), true, true)) != St::ok) return s;
  if ((s = a1(w.xfcut, "ww::adat::XFCUT", of(9, 2), true, true)) != St::ok) return s;
  if ((s = a1(w.xcflxymax, "ww::adat::XCFLXYMAX", of(9, 3), true, true)) != St::ok) return s;
  if ((s = a1(w.xcflthmax, "ww::adat::XCFLTHMAX", of(9, 4), true, true)) != St::ok) return s;
  if ((s = a1(w.xcflkmax, "ww::adat::XCFLKMAX", of(9, 5), true, true)) != St::ok) return s;
  fill(w.xdtdyn);  // :2357-2361
  fill(w.xfcut);
  fill(w.xcflxymax);
  fill(w.xcflthmax);
  fill(w.xcflkmax);

  // Group 10, :2363-2371: the second extent is the loop index I, and each pass replaces
  // XUSERO (WW3's defect, kept; see the header comment).
  for (int i = 1; i <= xdma_detail::kNoextr; ++i)
    if ((s = a2(w.xusero, nullptr, "ww::adat::XUSERO", of(10, i), 1, i, 1, i)) != St::ok)
      return s;
  fill(w.xusero);  // :2373

  scalars[m].ainit2 = true;  // :2375
  exec.fence("ww::adat::w3xdma fills");
  return St::ok;
}

}  // namespace ww::adat
