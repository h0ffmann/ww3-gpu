// kokkos/src/ww_kokkos/adat_w3dima.hpp
// W3DIMA, ported to Kokkos: size and allocate the W3ADATMD arrays of model IMOD
// (all of them, or with D_ONLY only the data arrays), and give the ones the Fortran
// initialises their first value. Host-side, outside any kernel. Phase 1,
// translated, not regtested.
//
// WW3 heritage: SUBROUTINE W3DIMA, WW3/model/src/w3adatmd.F90:847-1509 at the WW3/
// pin 8e433f99 (byte-identical to NOAA-EMC/WW3 03adda91); author H. L. Tolman.
// Ported as switches/switch_lab_shrd compiles it: W3_PR3 :1438-1448 is in; W3_IC3
// :1315-1320 and :1328-1331, W3_SMC :1404-1409, W3_PR1 :1414-1420, W3_PR2
// :1422-1431, W3_S STRACE and the W3_T prints are not compiled and not ported.
// LPDLIB (:1374) is a run-time LOGICAL (constants.F90:102), so both branches are.
//
// Checked against gfortran 16.2 -O3 GIMPLE (-fdump-tree-optimized-lineno) of
// w3adatmd.F90 with WW3's Release flags and the switch_lab_shrd defines:
//  - FL_ALL is stored through the module pointer FL_ALL at :961, BEFORE the checks
//    and before W3SETA(IMOD) at :994 retargets it (W3SETA :2760), so it lands in the
//    model selected by the previous W3SETA, W3setaTargets::base. :1323 reloads the
//    pointer and reads WADATS(IMOD)%FL_ALL. Both go through t.base here.
//  - NSEALM at :1038-1039 is read and written through the pointer W3SETA :2754 set,
//    i.e. WADATS(IMOD)%NSEALM, here wadats[t.base-1] with t.base == imod.
//  - Every fill (= 0. and = UNDEF) is a scalar loop of 4-byte stores of one value,
//    no arithmetic; UNDEF is loaded from the module variable before each fill.
//    Kokkos::deep_copy of a scalar stores the same bits.
//  - NXXX = NSEALM*NAPROC (:998) survives only as a DEBUG bind: dead, not ported.
//  - print_memcheck (w3servmd.F90:2501) writes only under W3_MEMCHECK, not compiled.
//
// Not ported, by construction of a C++ library call: the WRITE(NDSE) messages and
// EXTCDE (returned as the code EXTCDE would exit with, W3dimaStatus); W3SETG(IMOD)
// at :986 and W3SETG(JGRID) at :1485 (the caller passes IMOD's grid in
// W3dimaGrid).
//
// W3SETA(IMOD) at :994 and :1467, partly. The port cannot call w3seta: adat_w3seta.hpp
// includes this header (through adat_w3dmnl.hpp), and W3SETA's conditions read
// WADATS(IMOD)%NLINIT (WadatsNl1) and INPUTS(IMOD)%INFLAGS1, which W3DIMA does not
// take. So each call does W3SETA's unconditional part, IADATA = IMOD (:2746) and
// the target of ITIME .. FL_ALL, NMX0 .. NCENT, NFR .. NLINIT (:2751-2809,
// t.base = imod). After an ok return the caller must call
// w3seta(reg, nl1, t, imod, in) with in.gtype the GTYPE of grid IMOD (W3SETG(IMOD)
// at :986 is still in force at :1467) and INPUTS(IMOD)%INFLAGS1; that one call
// leaves every alias group as the two Fortran calls do: at :994 AINIT of IMOD is
// false (:976), so of the conditional groups only NLINIT (:3038) can move there,
// W3DIMA does not change NLINIT, and :1467 tests it again. Between :994 and :1467
// the Fortran reads no alias of a conditional group, only NSEALM and FL_ALL.
#pragma once

#include <array>
#include <cstddef>
#include <exception>
#include <optional>
#include <string>

#include <Kokkos_Core.hpp>

#include "adat_wadat.hpp"
#include "real.hpp"

namespace ww::adat {

/// EXTCDE exit codes of W3DIMA, returned instead of stopping the program.
enum class W3dimaStatus : int {
  ok = 0,
  grids_not_initialized = 1,  ///< NGRIDS == -1, :966-969
  illegal_model = 2,          ///< IMOD < 1 or IMOD > NADATA, :971-974
  already_allocated = 3,      ///< WADATS(IMOD)%AINIT, :976-979
  alloc_failed = 99,          ///< CHECK_ALLOC_STATUS, w3macros.h:49-51
  /// Not a WW3 code: :961 would store through the FL_ALL pointer before any W3SETA
  /// has associated it (W3setaTargets::base still 0), undefined behaviour in the
  /// Fortran. No caller at the pin does this (W3INIT calls W3SETA at
  /// w3initmd.F90:540, W3IOGO at w3iogomd.F90:2605, ww3_gint at ww3_gint.F90:805,
  /// before W3DIMA).
  fl_all_unassociated = -1,
};

/// @brief The module state of other modules W3DIMA reads, for grid IMOD.
/// @details W3GDATMD of grid IMOD (what W3SETG(IMOD) at :986 selects), W3ODATMD,
///          W3IDATMD and CONSTANTS. FLCUR..FLRHOA are the W3IDATMD pointers as the
///          caller left them (the input grid of the last W3SETI); W3DIMA does not
///          select them.
struct W3dimaGrid {
  int ngrids = -1;                     ///< W3GDATMD NGRIDS
  int nsea = 0, nseal = 0;             ///< NSEA, NSEAL
  int nx = 0, ny = 0;                  ///< NX, NY
  int nk = 0, nth = 0, nspec = 0;      ///< NK, NTH, NSPEC
  int gtype = 0, ungtype = 3;          ///< GTYPE, UNGTYPE (w3gdatmd.F90:627)
  std::array<std::array<int, 3>, 5> e3df{};  ///< E3DF(i,j) -> e3df[j-1][i-1]
  std::array<int, 3> p2msf{};          ///< P2MSF(i) -> p2msf[i-1]
  std::array<int, 3> us3df{};          ///< US3DF(i) -> us3df[i-1]
  std::array<int, 2> usspf{};          ///< USSPF(i) -> usspf[i-1]
  int noswll = 0;                      ///< W3ODATMD NOSWLL, of the last W3SETO (not selected here)
  int noextr = 2;                      ///< W3ODATMD NOEXTR, PARAMETER (w3odatmd.F90:335)
  Real undef = -999.9f;                ///< CONSTANTS UNDEF (constants.F90:85)
  bool flcur = false, flwind = false, fltaua = false, flrhoa = false;  ///< W3IDATMD
  bool lpdlib = false;                 ///< CONSTANTS LPDLIB (constants.F90:102)
};

namespace detail {

/// Extent of the Fortran bound pair lb:ub; negative means zero-size, as ALLOCATE.
/// 64-bit like gfortran's (integer(kind=8)) bound arithmetic.
inline std::size_t fext(long lb, long ub) {
  return ub >= lb ? static_cast<std::size_t>(ub - lb + 1) : std::size_t{0};
}

/// ALLOCATE without the fill: a Fortran pointer ALLOCATE leaves the contents
/// undefined, so the View is created WithoutInitializing too.
template <class V, class... N>
V alloc(const char* label, N... n) {
  return V(Kokkos::view_alloc(Kokkos::WithoutInitializing, std::string(label)), n...);
}

}  // namespace detail

/// @brief W3DIMA(IMOD, NDSE, NDST, D_ONLY), shared memory (W3_SHRD) build.
/// @param exec   execution space instance the fills run on; fenced before the ok return
///               only (on an error code the fills may still be in flight).
/// @param reg    WADATS, NADATA and IADATA (in/out: IADATA = IMOD once past the checks).
/// @param t      W3SETA's alias targets (in/out): t.base is the FL_ALL target of :961
///               (in) and is IMOD once past the checks; the other groups are not
///               touched, see the header for the w3seta call the caller owes.
/// @param imod   model number, 1-based as in the Fortran.
/// @param g      the state of grid IMOD and of the other modules W3DIMA reads.
/// @param d_only the OPTIONAL D_ONLY; absent means FL_ALL = .TRUE. (:960-964).
/// @return W3dimaStatus::ok, or the code EXTCDE would have exited with. On a code
///         other than ok the Fortran stops the program; here reg is left as far as
///         the Fortran had got and must not be used further.
/// @author ww3-gpu; heritage H. L. Tolman (W3ADATMD)
/// @date 2026-10-16
template <class ExecSpace>
W3dimaStatus w3dima(const ExecSpace& exec, Wadats<ExecSpace>& reg, W3setaTargets& t,
                    int imod, const W3dimaGrid& g,
                    std::optional<bool> d_only = std::nullopt) {
  using W = Wadat<ExecSpace>;
  using R1 = typename W::R1;
  using R2 = typename W::R2;
  using R3 = typename W::R3;
  using I1 = typename W::I1;
  using I2 = typename W::I2;
  using detail::alloc;
  using detail::fext;

  // :960-964, through the FL_ALL alias of the model the previous W3SETA selected.
  if (t.base < 1 || t.base > static_cast<int>(reg.wadats.size()))
    return W3dimaStatus::fl_all_unassociated;
  reg.wadats[static_cast<std::size_t>(t.base - 1)].fl_all =
      d_only.has_value() ? !*d_only : true;

  if (g.ngrids == -1) return W3dimaStatus::grids_not_initialized;           // :966
  if (imod < 1 || imod > reg.nadata) return W3dimaStatus::illegal_model;    // :971
  W& w = reg.wadats[static_cast<std::size_t>(imod - 1)];
  if (w.ainit) return W3dimaStatus::already_allocated;                     // :976

  // W3SETA(IMOD) :994, its unconditional part (header): :2746, :2751-2809.
  reg.iadata = imod;
  t.base = imod;
  // The target of the NSEALM and FL_ALL aliases from here on; the same struct as w.
  W& alias = reg.wadats[static_cast<std::size_t>(t.base - 1)];

  // One ALLOCATE(..., STAT=ISTAT) statement; CHECK_ALLOC_STATUS on failure.
  auto stmt = [](auto&& body) -> bool {
    try {
      body();
      return true;
    } catch (const std::exception&) {
      return false;
    }
  };
  auto fill = [&exec](const auto& v, Real x) { Kokkos::deep_copy(exec, v, x); };
  constexpr auto fail = W3dimaStatus::alloc_failed;

  const long nsea = g.nsea;
  const long nk = g.nk;
  const std::size_t sea0 = fext(0, nsea);  // 0:NSEA
  const std::size_t k01 = fext(0, nk + 1);  // 0:NK+1

  // 1) Forcing fields
  if (!stmt([&] { w.dw = alloc<R1>("ww::adat::DW", sea0); })) return fail;  // :1004
  fill(w.dw, Real(0));                                                      // :1006
  if (!stmt([&] {                                                           // :1008
        w.cx = alloc<R1>("ww::adat::CX", sea0);
        w.cy = alloc<R1>("ww::adat::CY", sea0);
      }))
    return fail;
  fill(w.cx, Real(0));  // :1011
  fill(w.cy, Real(0));  // :1012
  if (!stmt([&] {       // :1014-1016, not initialised
        w.ua = alloc<R1>("ww::adat::UA", sea0);
        w.ud = alloc<R1>("ww::adat::UD", sea0);
        w.u10 = alloc<R1>("ww::adat::U10", fext(1, nsea));
        w.u10d = alloc<R1>("ww::adat::U10D", fext(1, nsea));
        w.as = alloc<R1>("ww::adat::AS", sea0);
      }))
    return fail;
  if (!stmt([&] {  // :1019-1020
        w.taua = alloc<R1>("ww::adat::TAUA", sea0);
        w.tauadir = alloc<R1>("ww::adat::TAUADIR", sea0);
      }))
    return fail;
  fill(w.taua, Real(0));     // :1022
  fill(w.tauadir, Real(0));  // :1023

  // 2) Standard mean wave parameters
  if (alias.nsealm == 0) alias.nsealm = g.nsea;  // :1038-1040
  const std::size_t nl = fext(1, alias.nsealm);
  if (!stmt([&] {  // :1041-1053
        w.hs = alloc<R1>("ww::adat::HS", nl);
        w.wlm = alloc<R1>("ww::adat::WLM", nl);
        w.t02 = alloc<R1>("ww::adat::T02", nl);
        w.t0m1 = alloc<R1>("ww::adat::T0M1", nl);
        w.t01 = alloc<R1>("ww::adat::T01", nl);
        w.fp0 = alloc<R1>("ww::adat::FP0", nl);
        w.thm = alloc<R1>("ww::adat::THM", nl);
        w.ths = alloc<R1>("ww::adat::THS", nl);
        w.thp0 = alloc<R1>("ww::adat::THP0", nl);
        w.hsig = alloc<R1>("ww::adat::HSIG", nl);
        w.stmaxe = alloc<R1>("ww::adat::STMAXE", nl);
        w.stmaxd = alloc<R1>("ww::adat::STMAXD", nl);
        w.hmaxe = alloc<R1>("ww::adat::HMAXE", nl);
        w.hmaxd = alloc<R1>("ww::adat::HMAXD", nl);
        w.hcmaxe = alloc<R1>("ww::adat::HCMAXE", nl);
        w.hcmaxd = alloc<R1>("ww::adat::HCMAXD", nl);
        w.qp = alloc<R1>("ww::adat::QP", nl);
        w.wbt = alloc<R1>("ww::adat::WBT", nl);
        w.wnmean = alloc<R1>("ww::adat::WNMEAN", nl);
      }))
    return fail;
  for (const R1* v : {&w.hs, &w.wlm, &w.t02, &w.t0m1, &w.t01, &w.fp0, &w.thm, &w.ths,
                      &w.thp0, &w.hsig, &w.stmaxe, &w.stmaxd, &w.hmaxe, &w.hmaxd,
                      &w.hcmaxe, &w.hcmaxd, &w.qp, &w.wbt, &w.wnmean})
    fill(*v, g.undef);  // :1056-1074, in this order

  // 3) Frequency-dependent parameters, :1083-1107 allocate, :1109-1113 fill.
  //    E3DF(1,i) > 0 selects array i; its second bounds are E3DF(2,i):E3DF(3,i).
  R2* const e3[5] = {&w.ef, &w.th1m, &w.sth1m, &w.th2m, &w.sth2m};
  int* const e3lb[5] = {&w.ef_lb, &w.th1m_lb, &w.sth1m_lb, &w.th2m_lb, &w.sth2m_lb};
  const char* const e3name[5] = {"ww::adat::EF", "ww::adat::TH1M", "ww::adat::STH1M",
                                 "ww::adat::TH2M", "ww::adat::STH2M"};
  for (int i = 0; i < 5; ++i) {
    const auto& e = g.e3df[static_cast<std::size_t>(i)];
    if (e[0] > 0) {
      if (!stmt([&] { *e3[i] = alloc<R2>(e3name[i], nl, fext(e[1], e[2])); })) return fail;
      *e3lb[i] = e[1];
    }
  }
  for (int i = 0; i < 5; ++i)
    if (g.e3df[static_cast<std::size_t>(i)][0] > 0) fill(*e3[i], g.undef);

  // 4) Spectral partitions, second dimension 0:NOSWLL
  const std::size_t np = fext(0, g.noswll);
  if (!stmt([&] {  // :1119-1137
        w.phs = alloc<R2>("ww::adat::PHS", nl, np);
        w.ptp = alloc<R2>("ww::adat::PTP", nl, np);
        w.plp = alloc<R2>("ww::adat::PLP", nl, np);
        w.pdir = alloc<R2>("ww::adat::PDIR", nl, np);
        w.psi = alloc<R2>("ww::adat::PSI", nl, np);
        w.pws = alloc<R2>("ww::adat::PWS", nl, np);
        w.pwst = alloc<R1>("ww::adat::PWST", nl);
        w.pnr = alloc<R1>("ww::adat::PNR", nl);
        w.pthp0 = alloc<R2>("ww::adat::PTHP0", nl, np);
        w.pqp = alloc<R2>("ww::adat::PQP", nl, np);
        w.ppe = alloc<R2>("ww::adat::PPE", nl, np);
        w.pgw = alloc<R2>("ww::adat::PGW", nl, np);
        w.psw = alloc<R2>("ww::adat::PSW", nl, np);
        w.ptm1 = alloc<R2>("ww::adat::PTM1", nl, np);
        w.pt1 = alloc<R2>("ww::adat::PT1", nl, np);
        w.pt2 = alloc<R2>("ww::adat::PT2", nl, np);
        w.pep = alloc<R2>("ww::adat::PEP", nl, np);
      }))
    return fail;
  fill(w.phs, g.undef);   // :1139
  fill(w.ptp, g.undef);
  fill(w.plp, g.undef);
  fill(w.pdir, g.undef);
  fill(w.psi, g.undef);
  fill(w.pws, g.undef);
  fill(w.pwst, g.undef);
  fill(w.pnr, g.undef);
  fill(w.pthp0, g.undef);
  fill(w.pqp, g.undef);
  fill(w.ppe, g.undef);
  fill(w.pgw, g.undef);
  fill(w.psw, g.undef);
  fill(w.ptm1, g.undef);
  fill(w.pt1, g.undef);
  fill(w.pt2, g.undef);
  fill(w.pep, g.undef);   // :1155

  // 5) Atmosphere-waves layer
  if (!stmt([&] {  // :1161-1171
        w.charn = alloc<R1>("ww::adat::CHARN", nl);
        w.tws = alloc<R1>("ww::adat::TWS", nl);
        w.cge = alloc<R1>("ww::adat::CGE", nl);
        w.phiaw = alloc<R1>("ww::adat::PHIAW", nl);
        w.tauwix = alloc<R1>("ww::adat::TAUWIX", nl);
        w.tauwiy = alloc<R1>("ww::adat::TAUWIY", nl);
        w.tauwnx = alloc<R1>("ww::adat::TAUWNX", nl);
        w.tauwny = alloc<R1>("ww::adat::TAUWNY", nl);
        w.whitecap = alloc<R2>("ww::adat::WHITECAP", nl, std::size_t{4});
      }))
    return fail;
  for (const R1* v : {&w.charn, &w.tws, &w.cge, &w.phiaw, &w.tauwix, &w.tauwiy, &w.tauwnx,
                      &w.tauwny})
    fill(*v, g.undef);      // :1173-1180
  fill(w.whitecap, g.undef);  // :1181

  // 6) Wave-ocean layer
  if (!stmt([&] {  // :1187-1205
        w.sxx = alloc<R1>("ww::adat::SXX", nl);
        w.syy = alloc<R1>("ww::adat::SYY", nl);
        w.sxy = alloc<R1>("ww::adat::SXY", nl);
        w.tauox = alloc<R1>("ww::adat::TAUOX", nl);
        w.tauoy = alloc<R1>("ww::adat::TAUOY", nl);
        w.bhd = alloc<R1>("ww::adat::BHD", nl);
        w.phioc = alloc<R1>("ww::adat::PHIOC", nl);
        w.tusx = alloc<R1>("ww::adat::TUSX", nl);
        w.tusy = alloc<R1>("ww::adat::TUSY", nl);
        w.ussx = alloc<R1>("ww::adat::USSX", nl);
        w.ussy = alloc<R1>("ww::adat::USSY", nl);
        w.tauocx = alloc<R1>("ww::adat::TAUOCX", nl);
        w.tauocy = alloc<R1>("ww::adat::TAUOCY", nl);
        w.prms = alloc<R1>("ww::adat::PRMS", nl);
        w.tpms = alloc<R1>("ww::adat::TPMS", nl);
        w.phice = alloc<R1>("ww::adat::PHICE", nl);
        w.tauice = alloc<R2>("ww::adat::TAUICE", nl, std::size_t{2});
      }))
    return fail;
  // NK*2 in default INTEGER, as the Fortran; US3DF(2:3) is not used (:1214).
  const std::size_t nk2 = fext(1, g.nk * 2);
  if (g.p2msf[0] > 0) {  // :1210-1213
    if (!stmt([&] { w.p2sms = alloc<R2>("ww::adat::P2SMS", nl, fext(g.p2msf[1], g.p2msf[2])); }))
      return fail;
    w.p2sms_lb = g.p2msf[1];
  }
  if (g.us3df[0] > 0)  // :1214-1217
    if (!stmt([&] { w.us3d = alloc<R2>("ww::adat::US3D", nl, nk2); })) return fail;
  if (g.usspf[0] > 0)  // :1218-1221
    if (!stmt([&] { w.ussp = alloc<R2>("ww::adat::USSP", nl, nk2); })) return fail;
  for (const R1* v : {&w.sxx, &w.syy, &w.sxy, &w.tauox, &w.tauoy, &w.bhd, &w.phioc, &w.tusx,
                      &w.tusy, &w.ussx, &w.ussy, &w.tauocx, &w.tauocy, &w.prms, &w.tpms,
                      &w.phice})
    fill(*v, g.undef);  // :1223-1238
  fill(w.tauice, g.undef);                          // :1239
  if (g.p2msf[0] > 0) fill(w.p2sms, g.undef);       // :1240
  if (g.us3df[0] > 0) fill(w.us3d, g.undef);        // :1241
  if (g.usspf[0] > 0) fill(w.ussp, g.undef);        // :1242

  // 7) Wave-bottom layer
  if (!stmt([&] {  // :1248-1252
        w.aba = alloc<R1>("ww::adat::ABA", nl);
        w.abd = alloc<R1>("ww::adat::ABD", nl);
        w.uba = alloc<R1>("ww::adat::UBA", nl);
        w.ubd = alloc<R1>("ww::adat::UBD", nl);
        w.bedforms = alloc<R2>("ww::adat::BEDFORMS", nl, std::size_t{3});
        w.phibbl = alloc<R1>("ww::adat::PHIBBL", nl);
        w.taubbl = alloc<R2>("ww::adat::TAUBBL", nl, std::size_t{2});
      }))
    return fail;
  fill(w.aba, g.undef);       // :1255
  fill(w.abd, g.undef);
  fill(w.uba, g.undef);
  fill(w.ubd, g.undef);
  fill(w.bedforms, g.undef);
  fill(w.phibbl, g.undef);
  fill(w.taubbl, g.undef);    // :1261

  // 8) Spectrum parameters
  if (!stmt([&] {  // :1267-1272
        w.mssx = alloc<R1>("ww::adat::MSSX", nl);
        w.mssy = alloc<R1>("ww::adat::MSSY", nl);
        w.mscx = alloc<R1>("ww::adat::MSCX", nl);
        w.mscy = alloc<R1>("ww::adat::MSCY", nl);
        w.mssd = alloc<R1>("ww::adat::MSSD", nl);
        w.mscd = alloc<R1>("ww::adat::MSCD", nl);
        w.qkk = alloc<R1>("ww::adat::QKK", nl);
        w.skew = alloc<R1>("ww::adat::SKEW", nl);
        w.embia1 = alloc<R1>("ww::adat::EMBIA1", nl);
        w.embia2 = alloc<R1>("ww::adat::EMBIA2", nl);
      }))
    return fail;
  for (const R1* v : {&w.mssx, &w.mssy, &w.mssd, &w.mscx, &w.mscy, &w.mscd, &w.qkk, &w.skew,
                      &w.embia1, &w.embia2})
    fill(*v, g.undef);  // :1275-1284, in this order (MSSD before MSCX)

  // 9) Numerical diagnostics
  if (!stmt([&] {  // :1290-1294
        w.dtdyn = alloc<R1>("ww::adat::DTDYN", nl);
        w.fcut = alloc<R1>("ww::adat::FCUT", nl);
        w.cflxymax = alloc<R1>("ww::adat::CFLXYMAX", nl);
        w.cflthmax = alloc<R1>("ww::adat::CFLTHMAX", nl);
        w.cflkmax = alloc<R1>("ww::adat::CFLKMAX", nl);
      }))
    return fail;
  for (const R1* v : {&w.dtdyn, &w.fcut, &w.cflxymax, &w.cflthmax, &w.cflkmax})
    fill(*v, g.undef);  // :1297-1301

  // 10) User defined, and the wavenumbers
  if (!stmt([&] { w.usero = alloc<R2>("ww::adat::USERO", nl, fext(1, g.noextr)); }))  // :1307
    return fail;
  fill(w.usero, g.undef);  // :1310
  if (!stmt([&] { w.wn = alloc<R2>("ww::adat::WN", k01, sea0); })) return fail;  // :1312

  // :1323, read through the alias W3SETA retargeted: WADATS(IMOD)%FL_ALL.
  if (alias.fl_all) {
    if (!stmt([&] { w.cg = alloc<R2>("ww::adat::CG", k01, sea0); })) return fail;  // :1325
    const std::size_t s1 = fext(1, nsea);
    if (g.flcur)  // :1334-1341
      if (!stmt([&] {
            w.ca0 = alloc<R1>("ww::adat::CA0", s1);
            w.cai = alloc<R1>("ww::adat::CAI", s1);
            w.cd0 = alloc<R1>("ww::adat::CD0", s1);
            w.cdi = alloc<R1>("ww::adat::CDI", s1);
          }))
        return fail;
    if (g.flwind)  // :1343-1352
      if (!stmt([&] {
            w.ua0 = alloc<R1>("ww::adat::UA0", s1);
            w.uai = alloc<R1>("ww::adat::UAI", s1);
            w.ud0 = alloc<R1>("ww::adat::UD0", s1);
            w.udi = alloc<R1>("ww::adat::UDI", s1);
            w.as0 = alloc<R1>("ww::adat::AS0", s1);
            w.asi = alloc<R1>("ww::adat::ASI", s1);
          }))
        return fail;
    if (g.fltaua)  // :1354-1361
      if (!stmt([&] {
            w.ma0 = alloc<R1>("ww::adat::MA0", s1);
            w.mai = alloc<R1>("ww::adat::MAI", s1);
            w.md0 = alloc<R1>("ww::adat::MD0", s1);
            w.mdi = alloc<R1>("ww::adat::MDI", s1);
          }))
        return fail;
    if (g.flrhoa)  // :1363-1368
      if (!stmt([&] {
            w.ra0 = alloc<R1>("ww::adat::RA0", s1);
            w.rai = alloc<R1>("ww::adat::RAI", s1);
          }))
        return fail;
    // NY*NX in default INTEGER, as the Fortran, then widened for the bound.
    const std::size_t nxy = fext(1, g.ny * g.nx);
    if (!stmt([&] {  // :1370-1371, second dimension -1:1
          w.atrnx = alloc<R2>("ww::adat::ATRNX", nxy, std::size_t{3});
          w.atrny = alloc<R2>("ww::adat::ATRNY", nxy, std::size_t{3});
        }))
      return fail;
    // :1374-1393. Rectilinear (NY,NX) storage, or under LPDLIB one row of NSEAL.
    const std::size_t d1 = g.lpdlib ? std::size_t{1} : fext(1, g.ny);
    const std::size_t d2 = g.lpdlib ? fext(1, g.nseal) : fext(1, g.nx);
    if (!stmt([&] {
          w.dddx = alloc<R2>("ww::adat::DDDX", d1, d2);
          w.dddy = alloc<R2>("ww::adat::DDDY", d1, d2);
          w.dcdx = alloc<R3>("ww::adat::DCDX", k01, d1, d2);
          w.dcdy = alloc<R3>("ww::adat::DCDY", k01, d1, d2);
          w.dcxdx = alloc<R2>("ww::adat::DCXDX", d1, d2);
          w.dcydx = alloc<R2>("ww::adat::DCYDX", d1, d2);
          w.dcxdy = alloc<R2>("ww::adat::DCXDY", d1, d2);
          w.dcydy = alloc<R2>("ww::adat::DCYDY", d1, d2);
        }))
      return fail;
    fill(w.dddx, Real(0));   // :1395
    fill(w.dddy, Real(0));
    fill(w.dcdx, Real(0));
    fill(w.dcdy, Real(0));
    fill(w.dcxdx, Real(0));
    fill(w.dcydx, Real(0));
    fill(w.dcxdy, Real(0));
    fill(w.dcydy, Real(0));  // :1402
    if (!stmt([&] { w.alpha = alloc<R2>("ww::adat::ALPHA", fext(1, nk), fext(1, g.nseal)); }))
      return fail;  // :1411
    if (g.gtype == g.ungtype)  // :1433-1436
      if (!stmt([&] { w.iter = alloc<I2>("ww::adat::ITER", fext(1, nk), fext(1, g.nth)); }))
        return fail;
    // W3_PR3, :1439-1447. (NK+2)*NTH and NSPEC+NTH in default INTEGER.
    if (!stmt([&] {
          w.mapx2 = alloc<I1>("ww::adat::MAPX2", nxy);
          w.mapy2 = alloc<I1>("ww::adat::MAPY2", nxy);
          w.mapaxy = alloc<I1>("ww::adat::MAPAXY", nxy);
          w.mapcxy = alloc<I1>("ww::adat::MAPCXY", fext(1, nsea));
          w.mapth2 = alloc<I1>("ww::adat::MAPTH2", fext(1, (g.nk + 2) * g.nth));
          w.mapwn2 = alloc<I1>("ww::adat::MAPWN2", fext(1, g.nspec + g.nth));
          w.maptrn = alloc<I1>("ww::adat::MAPTRN", nxy);
        }))
      return fail;
    Kokkos::deep_copy(exec, w.mapth2, 0);  // :1447
    if (!stmt([&] {  // :1450-1451
          w.iappro = alloc<I1>("ww::adat::IAPPRO", fext(1, g.nspec));
          w.sppnt = alloc<R3>("ww::adat::SPPNT", fext(1, g.nth), fext(1, nk), std::size_t{4});
        }))
      return fail;
  }

  w.ainit = true;  // :1456
  // W3SETA(IMOD) at :1467, its unconditional part; the caller's w3seta does the rest
  // (header). W3SETG(JGRID) at :1485 is the caller's.
  reg.iadata = imod;
  t.base = imod;
  exec.fence("ww::adat::w3dima fills");
  return W3dimaStatus::ok;
}

}  // namespace ww::adat
