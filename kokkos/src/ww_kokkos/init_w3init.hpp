// kokkos/src/ww_kokkos/init_w3init.hpp
// W3INIT, ported: set up one wave model grid IMOD before its first W3WAVE call. It selects the
// grid's data structures, sets the process counts, opens the log file, reads mod_def through
// W3IOGR, allocates the W3WDATMD, W3ADATMD and W3IDATMD arrays, reads the restart file through
// W3IORS, merges the restart MAPSTA with the grid's, sets the output times, computes the depth
// DW and, per sea point, the wavenumber WN and group velocity CG of every frequency, and writes
// the log header. Phase 1, translated, not wired into WW3, not regtested.
//
// WW3 heritage: SUBROUTINE W3INIT, WW3-noaa/model/src/w3initmd.F90:163-1683 at NOAA-EMC/WW3
// 03adda91; author H. L. Tolman. The h0ffmann/WW3 fork at the WW3/ pin adds a W3_KOKKOS block
// after the W3IOGR call (`git -C WW3 diff 03adda91 HEAD -- model/src/w3initmd.F90`, 100 lines,
// all under #ifdef W3_KOKKOS); switch_lab_shrd has no KOKKOS, so that block is not compiled and
// not ported. Ported as switches/switch_lab_shrd compiles it (gfortran -E with the defines of
// WW3's flags.make): SHRD, no MPI, no DIST, no PDLIB. Not compiled and not ported: W3_MPI (the
// communicators :568-574, :625-640, the W3IOPP call with MPI_COMM_WAVE :1260, and W3MPII,
// W3MPIO, W3MPIP at :1560-1562, which are therefore not hooks here), W3_DIST (the NSEA/NSPEC checks :807-818 and the IAPPRO sweeps
// :874-934, :956-965), W3_PDLIB, W3_SETUP, W3_UOST, W3_T and W3_T1 test output, W3_S,
// W3_TIMINGS, W3_DEBUGCOH, W3_DEBUGSTP, W3_DEBUGINIT.
//
// Dead in the -O3 object and not ported (v, GIMPLE and objdump of the object below):
//  - :690-696, the test file: TSTOUT is a .FALSE. PARAMETER (constants.F90:57), so INQUIRE and
//    OPEN of MDS(3) are folded away; TFILE and IFT (:672, :682) feed only that block.
//  - :860-867, :872-873, :966, NTTOT, NTTARG, NTTMAX and NT: read only under W3_DIST and W3_T.
//    The object still computes the NTLOC loop (vectorised) and mallocs and frees NT, but no
//    store reaches module state; nothing observable is lost. A failed malloc of NT (no STAT,
//    gfortran exit 1) is the one effect not reproduced ⚠.
//  - :586-619, the IOSTYP > 1 branch, is kept although SHRD makes it unreachable (IOSTYP = 1 at
//    :565 is folded into the test in the object): it costs nothing and keeps the source's
//    shape.
//
// Arithmetic, checked against the -O3 GIMPLE (-fdump-tree-optimized-lineno) and objdump of
// w3initmd.F90 built by gfortran 16.2 with WW3's CMake Release flags and the lab defines in the
// pinned ww3 shell; .text, .rodata*, .data and .bss byte-identical to the CMake object:
//  - :622 FRACOS = 100. * REAL(NTPROC-NAPROC) / REAL(NTPROC): cvtsi2ss of the difference,
//    mulss by 100., divss by cvtsi2ss NTPROC; the test is FRACOS > 15. (comiss).
//  - :1070, :1081 DTOUT = REAL(ODAT(..)): cvtsi2ss.
//  - :1151, :1205 MAX(0., DTOUT(J)) is `pxor xmm0; maxss (DTOUT),xmm0`: 0 > D ? 0 : D, so a NaN
//    D stays D. FLOUT(J) .AND. DTOUT(J) .GT. 0.5 tests the LOGICAL word against 0, then comiss
//    with 0.5; when both hold FLOUT(J) is not stored (its word is kept), else 0 is stored.
//  - :1165 DTTST.LT.0. for J /= 4 and DTTST.LE.0. for J = 4 (the object unswitches the loop on
//    J == 4); :1180 .LT.0., :1189 .GT.0., the same at :1216-1240 for J = 8.
//  - :1317, :1346 DW = MAX(0., WLVeff-ZB): subss, then maxss with a zero register as source:
//    D > 0 ? D : 0, a NaN D gives 0. :1321, :1347 recompute WLVeff-ZB (ZB is reloaded after the
//    DW store) and test .LE.0. (NaN false).
//  - :1396 MAX(DMIN, DW(IS)) is `maxss (DW),xmm0` with xmm0 = DMIN: DMIN > DW ? DMIN : DW.
//    :1410 MAX(DMIN, -ZB(IS)) is xorps (sign flip), then `maxss (DMIN),xmm0`: -ZB > DMIN ? -ZB :
//    DMIN. :1419 KDCHCK = WNbat*DEPTHbat, one mulss; :1420 .GE. KDMAX with KDMAX the SAVE
//    local 4. (its initialiser, :518; nothing in W3INIT writes it).
//  - :1013 SIGN(MAPTST, MAPSTA) on INTEGER is the xor/shift sequence of |A| with B's sign
//    (B >= 0 gives |A|); MOD and / on INTEGER (:1285, :1286, :1368) truncate toward zero.
//  - :1521, :1523 FACTOR*PTLOC: one mulss each; FACTOR is 1. or 1.E-3 (:786-790).
//  - :1096, :1108 FLOUT(1) = FLOUT(1) .OR. FLOGRD(J,K): when FLOUT(1) is 0 the word of FLOGRD
//    is stored as it is, else 1; reproduced (lor_raw).
//  - WAVNU1 is a call into w3dispmd (not inlined); its port is prop_wavnu1.hpp. No FMA, no
//    other libm call in W3INIT's own code.
//
// Calls. The routines with a port are called: W3SETA (adat_w3seta.hpp, twice: :540 and the
// tail W3DIMA's header asks for), W3IOGR (iogr_w3iogr.hpp), W3FLGRDUPDT (iogo_w3flgrdupdt.hpp),
// SET_UP_NSEAL_NSEALM and INIT_GET_ISEA (parall_*.hpp, the SHRD branch), W3DIMA
// (adat_w3dima.hpp) and WAVNU1 (prop_wavnu1.hpp). Every other call is a hook in InitHooks, at
// its place and with the Fortran's arguments; an empty hook on the path ends with
// kExitMissingHook, never a skipped call. Messages the Fortran writes before EXTCDE come back
// in InitStatus; the other WRITEs go through hooks, in the Fortran's order.
//
// Memory contract (phase 1). Grid arrays W3IOGR fills (MAPSTA, MAPST2, MAPSF, ZB, SIG) are the
// host vectors of iogr::IogrState. The W3ADATMD arrays are adat::Wadat Views in ExecSpace's
// memory space, selected through the W3SETA alias targets as the Fortran's pointers are; VA
// and WLV are InitWdat Views in the same space. The depth loops :1300-1350 run on the host in
// the Fortran's order, staging DW and WLV through host mirrors; the VA zeroing, the fills and
// the wavenumber loop :1394-1439 are kernels. Every hook receives the whole InitState and
// runs between fences, so it may read and write any of it. Never Unified Memory.
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <new>
#include <optional>
#include <string>
#include <vector>

#include <Kokkos_Core.hpp>

#include "adat_w3dima.hpp"
#include "adat_w3seta.hpp"
#include "adat_wadat.hpp"
#include "iogo_w3flgrdupdt.hpp"
#include "iogr_state.hpp"
#include "iogr_w3iogr.hpp"
#include "parall_init_get_isea.hpp"
#include "parall_set_up_nseal_nsealm.hpp"
#include "prop_wavnu1.hpp"
#include "real.hpp"

namespace ww::init {

using iogr::FChar;
using iogr::FLogical;

// ---- Constants ---------------------------------------------------------------------------

inline constexpr int kNogrp = 10;  ///< NOGRP, w3odatmd.F90:330, PARAMETER
inline constexpr int kNgrpp = 20;  ///< NGRPP, w3odatmd.F90:331, PARAMETER
inline constexpr Real kCritos = static_cast<Real>(15.);    ///< CRITOS, w3initmd.F90:129
inline constexpr Real kKdmax = static_cast<Real>(4.);      ///< KDMAX, :518 (SAVE, never written)
inline constexpr Real kFactorXY = static_cast<Real>(1.e-3);  ///< FACTOR when .NOT.FLAGLL, :789
inline const char* const kWwver = "7.14      ";  ///< WWVER, CHARACTER(10) PARAMETER, :130

/// Codes outside the Fortran's EXTCDE codes.
inline constexpr int kExitMissingHook = iogr::kExitMissingHook;  ///< -101: a hook the path needs is empty
inline constexpr int kExitShape = iogr::kExitShape;  ///< -102: an array lacks its Fortran extent
inline constexpr int kExitUnassociated = -104;  ///< a W3ADATMD alias W3INIT uses has no target
inline constexpr int kExitAllocate = 1;  ///< ALLOCATE without STAT= failed: gfortran exits 1

// ---- Module state W3INIT reads and writes that no other port holds -----------------------

/// W3ODATMD, the fields of OUTPTS(IMOD) W3SETO selects and the module variables W3INIT uses
/// (w3odatmd.F90:328-346, TYPE OUTPUT :442-461, OTYPE2 :364-388). LOGICAL as FLogical words.
/// Index maps: TONEXT(I,J), TOLAST(I,J) -> [(J-1)*2 + I-1]; FLOGRD(I,J), FLOGR2(I,J),
/// IDOUT(I,J) -> [(J-1)*kNogrp + I-1]; PTLOC(I,IP) -> ptloc[(IP-1)*2 + I-1].
struct InitOdat {
  std::array<int, 15> nds{};  ///< NDS(15)
  int ndso = 6, ndse = 6, ndst = 6, screen = 6;
  int ntproc = 0, naproc = 0, iaproc = 0, naplog = 1, naperr = 1;
  int napfld = 0, nappnt = 0, naptrk = 0, naprst = 0, napbpt = 0, napprt = 0;
  int iostyp = 1;          ///< module variable, :328
  FLogical unipts = 0, upproc = 0;  ///< module variables, :346
  int notype = 0;          ///< module variable, :334; 6 to 8 at the pin's callers
  std::array<int, 2> tofrst{};
  std::array<int, 16> tonext{}, tolast{};
  std::array<Real, 8> dtout{};
  std::array<FLogical, 8> flout{};
  std::array<FLogical, kNogrp * kNgrpp> flogrd{}, flogr2{};
  std::array<FLogical, kNogrp> flogd{}, flog2{};
  int ix0 = 0, ixn = 0, ixs = 0, iy0 = 0, iyn = 0, iys = 0;
  FLogical flform = 0;
  int nopts = 0;                  ///< NOPTS, set by W3IOPP
  std::vector<Real> ptloc;        ///< PTLOC(2,NOPTS), set by W3IOPP
  std::vector<FChar<40>> ptnme;   ///< PTNME(NOPTS), set by W3IOPP
  std::array<FChar<20>, kNogrp * kNgrpp> idout{};  ///< IDOUT, set by W3NOUT
};

/// W3IDATMD of input model IMOD as W3SETI(IMOD) selects it (w3idatmd.F90:962-984).
struct InitIdat {
  FLogical fllev = 0, flcur = 0, flwind = 0, flice = 0, fltaua = 0, flrhoa = 0;
  FLogical flmdn = 0, flmth = 0, flmvs = 0;
  FLogical flic1 = 0, flic2 = 0, flic3 = 0, flic4 = 0, flic5 = 0;
};

/// W3GDATMD of grid IMOD that iogr::IogrState does not carry.
struct InitGdat {
  int ngrids = -1;  ///< NGRIDS, as W3NMOD left it (W3DIMA checks it)
  int nseal = 0;    ///< NSEAL, written at :804
  int rstype = -1;  ///< RSTYPE, GRIDS(IMOD)%RSTYPE (w3gdatmd.F90:646), written by W3IORS
};

/// CONSTANTS variables W3INIT reads or writes (constants.F90:85, :102).
struct InitConstants {
  Real undef = static_cast<Real>(-999.9);
  FLogical lpdlib = 0;  ///< written .FALSE. at :642
};

/// W3WDATMD of model IMOD (w3wdatmd.F90:138-163 TYPE WDATA, :172-185 aliases): the dates W3INIT logs and the two arrays it
/// touches, allocated by W3DIMW (:491, :560). VA(NSPEC,0:NSEALM): va(ISPEC-1, JSEA);
/// WLV(NSEA): wlv(ISEA-1).
template <class ExecSpace>
struct InitWdat {
  using Mem = typename ExecSpace::memory_space;
  std::array<int, 2> time{}, tlev{}, tice{}, trho{};
  Kokkos::View<Real**, Kokkos::LayoutLeft, Mem> va;
  Kokkos::View<Real*, Kokkos::LayoutLeft, Mem> wlv;
};

/// The module state of grid IMOD, as the Fortran's USE association reaches it after the
/// W3SETx calls of :529-543. Every member refers to the caller's object: a multi-grid caller
/// owns one per grid (and one WADATS for all), as WW3 does.
template <class ExecSpace>
struct InitState {
  iogr::IogrState& g;          ///< GRIDS(IMOD), SGRDS(IMOD), MPARS(IMOD), W3ODATMD parts
  iogr::IogrSaved& saved;      ///< W3DISPMD tables, one per process (W3IOGR fills them)
  InitGdat& gx;
  adat::Wadats<ExecSpace>& adat;
  adat::WadatsNl1<ExecSpace>& nl1;
  std::vector<adat::WadatScalars>& ascal;  ///< FLCOLD, FLIWND of each model
  adat::W3setaTargets& at;     ///< W3SETA's alias targets
  InitWdat<ExecSpace>& w;
  InitIdat& in;
  InitOdat& o;
  InitConstants& c;
};

/// W3INIT's dummy arguments (:163-164, :460-474). LOGICAL arrays are FLogical words in
/// Fortran order; flgrd and flgr2 are (NOGRP,NGRPP), flgd and flg2 (NOGRP), all INOUT.
/// IsMulti and MPI_COMM_IN are read only under PDLIB and MPI and are not here.
struct InitArgs {
  int imod = 1;
  std::string fext;            ///< FEXT*(*), as passed (trailing blanks count for W3IOGR)
  std::array<int, 15> mds{};
  std::array<int, 2> mtrace{};
  std::array<int, 40> odat{};
  FLogical* flgrd = nullptr;
  FLogical* flgr2 = nullptr;
  FLogical* flgd = nullptr;
  FLogical* flg2 = nullptr;
  int npt = 0;
  Real* xpt = nullptr;         ///< XPT(NPT), INOUT
  Real* ypt = nullptr;         ///< YPT(NPT), INOUT
  const FChar<40>* pnames = nullptr;  ///< PNAMES(NPT)
  std::array<int, 6> iprt{};
  FLogical* prtfrm = nullptr;  ///< INOUT, read once (:1131)
  const FLogical* flagstidein = nullptr;  ///< FLAGSTIDEIN(4), OPTIONAL: null when absent
};

// ---- Status and hooks --------------------------------------------------------------------

/// What W3INIT ends with. exit_code 0 and stop false: it returned. stop true: the Fortran
/// executed STOP (:650, :655), _gfortran_stop_string(0, 0, 0): exit status 0, nothing on stdout,
/// and on stderr "Note: The following floating-point exceptions are signalling: ..." when an
/// IEEE flag other than inexact is raised (a gfortran 16.2 probe, 2026-10-16); the caller does it. Otherwise exit_code is the code EXTCDE (or
/// gfortran) would exit with, callee the routine that ended, and the fields below the message
/// the caller prints.
struct InitStatus {
  int exit_code = 0;
  bool stop = false;
  std::string callee;     ///< "W3INIT", "W3IOGR", "W3SETA", "W3DIMA", or the hook's name
  int fmt = 0;            ///< 1000: EXTOPN's FORMAT (w3servmd.F90:1044-1046), for the LOG file
  int iostat = 0;         ///< IERR handed to EXTOPN
  iogr::IogrStatus iogr;  ///< when callee is "W3IOGR"
};

/// A routine W3INIT calls and this file does not port. Returns 0, or the EXTCDE code it ends
/// with.
template <class ExecSpace>
using InitHook = std::function<int(InitState<ExecSpace>&)>;

/// The calls of W3INIT that stay outside this file, in its order. A path that reaches an
/// empty hook ends with kExitMissingHook rather than skip the call; memcheck may be empty
/// (print_memcheck writes only under W3_MEMCHECK, w3servmd.F90:2501-2516, not in the switch).
template <class ExecSpace>
struct InitHooks {
  using S = InitState<ExecSpace>;
  /// W3SETO, W3SETG, W3SETW, W3SETI (IMOD, MDS(2), MDS(3)), :529, :534, :537, :543.
  std::function<int(S&, int imod, int ndse, int ndst)> w3seto, w3setg, w3setw, w3seti;
  /// print_memcheck(memunit, tag), the tags verbatim.
  std::function<void(S&, int memunit, const char* tag)> memcheck;
  /// WRITE (NDSE,8002) FRACOS, :623.
  std::function<void(S&, Real fracos)> write_8002;
  /// WRITE (NDSE,*) text (list-directed), :647-648, :652-653.
  std::function<void(S&, const char* text)> write_ndse_list;
  /// CALL FLUSH(NDSE), :649, :654.
  std::function<void(S&)> flush_ndse;
  /// OPEN (unit, FILE=file, IOSTAT=IERR), :686; returns IERR.
  std::function<int(S&, int unit, const std::string& file)> open_log;
  /// ITRACE(MTRACE(1), MTRACE(2)), :708.
  std::function<void(S&, int ndstrc, int ntrace)> itrace;
  /// WWDATE(STDATE), WWTIME(STTIME), WRITE (NDSO,900) WWVER, STDATE, STTIME, :715-717.
  std::function<void(S&, const char* wwver)> log_900;
  /// W3IOGR's own hooks (iogr_w3iogr.hpp), passed to its port at :735.
  iogr::IogrHooks iogr;
  /// SPATIAL_GRID, NVECTRI, COORDMAX, AREA_SI(1), :737-743, GTYPE = UNGTYPE only.
  InitHook<ExecSpace> spatial_grid, nvectri, coordmax;
  std::function<int(S&, int imod)> area_si;
  /// WRITE (NDSE,1008) once per bit of W3FLGRDUPDT's mask (iogo_w3flgrdupdt.hpp), :780.
  std::function<void(S&, int warn)> write_flgrdupdt;
  /// WRITE (NDSO, label) for the formats without data items (920, 975, 978, 979, 984, 985,
  /// 987) and WRITE (NDSO, label) a for those with one A item (930, 970-974, 976, 981-983,
  /// 988-990, 9970-9977); a holds the item's full CHARACTER length, blanks included.
  std::function<void(S&, int label, const std::string& a)> write_log;
  /// WRITE (NDSO,977) NOPTS, :1510.
  std::function<void(S&, int nopts)> write_977;
  /// WRITE (NDSO, label) IP, X, Y, PTNME(IP), label 980 or 986, :1521, :1523.
  std::function<void(S&, int label, int ip, Real x, Real y, const std::string& name)> write_point;
  /// W3DIMW(IMOD, NDSE, NDST[, F_ONLY]), :840, :843: allocates s.w.va and s.w.wlv.
  std::function<int(S&, int imod, int ndse, int ndst, std::optional<bool> f_only)> w3dimw;
  /// W3DIMI(IMOD, NDSE, NDST, FLAGSTIDEIN), :852; flagstidein null when absent.
  std::function<int(S&, int imod, int ndse, int ndst, const FLogical* flagstidein)> w3dimi;
  /// W3IORS('READ', NDS(6), SIG(NK), IMOD), :978: NDSR and DUMFPI by reference, as the
  /// Fortran passes them (DUMFPI is INTENT(INOUT), w3iorsmd.F90:354). Sets s.gx.rstype.
  std::function<int(S&, int& ndsr, Real& dumfpi, int imod)> w3iors_read;
  /// DSEC21(T1, T2) and TICK21(T, DTIME) (W3TIMEMD).
  std::function<Real(const int* t1, const int* t2)> dsec21;
  std::function<void(int* t, Real dtime)> tick21;
  /// W3DMO5(IMOD, NDSE, NDST, 4), :1120.
  std::function<int(S&, int imod, int ndse, int ndst, int iblock)> w3dmo5;
  /// W3IOPP(NPT, XPT, YPT, PNAMES, IMOD, 1), :1262: sets s.o.nopts, s.o.ptloc, s.o.ptnme.
  std::function<int(S&, int npt, Real* xpt, Real* ypt, const FChar<40>* pnames, int imod,
                    int comm)>
      w3iopp;
  /// STME21(TIME, DTME21): the CHARACTER(23) date string, :1529-1541.
  std::function<std::string(const int* time)> stme21;
};

namespace detail {

/// L1 .OR. L2 on LOGICAL(4) words as the object stores it at :1096, :1108: 1 when L1 is not
/// 0, else L2's word unchanged.
inline FLogical lor_raw(FLogical l1, FLogical l2) { return l1 != 0 ? 1 : l2; }

/// Fortran SIGN(A,B) for INTEGER: |A| when B >= 0, -|A| otherwise (:1013).
inline std::int32_t isign(std::int32_t a, std::int32_t b) {
  const std::int32_t m = a < 0 ? -a : a;
  return b >= 0 ? m : -m;
}

/// LEN_TRIM of a std::string: trailing blanks only.
inline std::size_t len_trim(const std::string& s) {
  std::size_t n = s.size();
  while (n > 0 && s[n - 1] == ' ') --n;
  return n;
}

template <std::size_t N>
std::string full(const FChar<N>& c) {
  return std::string(c.c.data(), N);
}

}  // namespace detail

// ---- Kernels -----------------------------------------------------------------------------

/// The WN, CG loop of :1394-1439 for IS = 0..NSEA. Each IS writes only WN(:,IS), CG(:,IS)
/// from its own inputs, with no reduction and no atomic, so any execution space and any order
/// give the bits of the Fortran's serial loop. zb is ZB(NSEA) as zb(ISEA-1); sig is
/// SIG(0:NK+1) as sig(IK); dw, wn, cg keep their Fortran lower bound 0.
template <class ExecSpace>
void init_wavenumbers(
    const ExecSpace& space,
    const Kokkos::View<const Real*, Kokkos::LayoutLeft, typename ExecSpace::memory_space>& dw,
    const Kokkos::View<const Real*, Kokkos::LayoutLeft, typename ExecSpace::memory_space>& zb,
    const Kokkos::View<const Real*, Kokkos::LayoutLeft, typename ExecSpace::memory_space>& sig,
    const Kokkos::View<Real**, Kokkos::LayoutLeft, typename ExecSpace::memory_space>& wn,
    const Kokkos::View<Real**, Kokkos::LayoutLeft, typename ExecSpace::memory_space>& cg,
    const prop::DispTable tab, Real dmin, int nsea, int nk) {
  Kokkos::parallel_for(
      "ww::init::wavenumbers",
      Kokkos::RangePolicy<ExecSpace, Kokkos::IndexType<int>>(space, 0, nsea + 1),
      KOKKOS_LAMBDA(const int is) {
        Real depth;
        if (is > 0) {
          const Real d = dw(is);
          depth = (dmin > d) ? dmin : d;  // :1396, maxss (DW),DMIN
        } else {
          depth = dmin;  // :1398
        }
        Real depthbat;
        if (is > 0) {
          const Real mz = -zb(is - 1);           // :1410, xorps
          depthbat = (mz > dmin) ? mz : dmin;    // maxss (DMIN),-ZB
        } else {
          depthbat = dmin;  // :1412
        }
        Real wnbat, cgbat;
        prop::wavnu1(tab, sig(1), depthbat, wnbat, cgbat);  // :1417
        const Real kdchck = wnbat * depthbat;               // :1419
        if (kdchck >= kKdmax) depth = depthbat;             // :1420-1423
        for (int ik = 0; ik <= nk + 1; ++ik) {              // :1425-1438
          prop::wavnu1(tab, sig(ik), depth, wn(ik, is), cg(ik, is));
        }
      });
  space.fence("ww::init::wavenumbers");
}

/// VA(:,JSEA) = 0. for the dry points of :1347-1349; dry(JSEA-1) is 1 for them.
template <class ExecSpace>
void init_zero_dry(
    const ExecSpace& space,
    const Kokkos::View<Real**, Kokkos::LayoutLeft, typename ExecSpace::memory_space>& va,
    const Kokkos::View<const int*, Kokkos::LayoutLeft, typename ExecSpace::memory_space>& dry,
    int nspec, int nseal) {
  Kokkos::parallel_for(
      "ww::init::zero_dry",
      Kokkos::MDRangePolicy<ExecSpace, Kokkos::Rank<2>, Kokkos::IndexType<int>>(
          space, {0, 1}, {nspec, nseal + 1}),
      KOKKOS_LAMBDA(const int isp, const int jsea) {
        if (dry(jsea - 1) != 0) va(isp, jsea) = Real(0);
      });
  space.fence("ww::init::zero_dry");
}

// ---- The routine -------------------------------------------------------------------------

/// W3INIT(IMOD, IsMulti, FEXT, MDS, MTRACE, ODAT, FLGRD, FLGR2, FLGD, FLG2, NPT, XPT, YPT,
/// PNAMES, IPRT, PRTFRM, MPI_COMM_IN, FLAGSTIDEIN), shared memory (W3_SHRD) build.
/// @param exec execution space of the W3ADATMD and W3WDATMD Views; fenced before every return.
/// @param s    the module state of grid IMOD (InitState).
/// @param a    the dummy arguments.
/// @param h    the hooks of the calls not ported here.
/// @return InitStatus: returned, STOP, or the exit code and message the Fortran ends with.
/// @author ww3-gpu; heritage H. L. Tolman (W3INITMD)
/// @date 2026-10-16
template <class ExecSpace>
InitStatus w3init(const ExecSpace& exec, InitState<ExecSpace>& s, const InitArgs& a,
                  const InitHooks<ExecSpace>& h) {
  using Mem = typename ExecSpace::memory_space;
  using R1 = Kokkos::View<Real*, Kokkos::LayoutLeft, Mem>;
  using CR1 = Kokkos::View<const Real*, Kokkos::LayoutLeft, Mem>;
  using HostR1 = Kokkos::View<const Real*, Kokkos::LayoutLeft, Kokkos::HostSpace,
                              Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
  InitStatus st;
  st.callee = "W3INIT";
  iogr::IogrState& g = s.g;
  InitOdat& o = s.o;
  const int imod = a.imod;

  auto missing = [&](const char* name) {
    exec.fence("ww::init::w3init");
    st.exit_code = kExitMissingHook;
    st.callee = name;
    return st;
  };
  auto ended = [&](int code, const char* name) {
    exec.fence("ww::init::w3init");
    st.exit_code = code;
    st.callee = name;
    return st;
  };
  auto memcheck = [&](int memunit, const char* tag) {
    if (!h.memcheck) return;
    exec.fence("ww::init::w3init");  // a hook may read any of s (header)
    h.memcheck(s, memunit, tag);
  };
  auto set4 = [&](const auto& hook) -> int {
    return hook ? hook(s, imod, a.mds[1], a.mds[2]) : kExitMissingHook;
  };

  // The dummy arrays the Fortran dereferences unconditionally (:780, :1092-1105, :1131).
  if (a.flgrd == nullptr || a.flgr2 == nullptr || a.flgd == nullptr || a.flg2 == nullptr ||
      a.prtfrm == nullptr)
    return ended(kExitShape, "W3INIT");

  // 1.a  Point to proper data structures, :529-550.
  if (const int e = set4(h.w3seto)) return ended(e, "W3SETO");
  int memunit = 10000 + o.iaproc;  // :531
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 1a");
  if (const int e = set4(h.w3setg)) return ended(e, "W3SETG");
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 1b");
  if (const int e = set4(h.w3setw)) return ended(e, "W3SETW");
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 1c");
  {
    adat::W3setaInputs si;  // :540, INPUTS(IMOD)%INFLAGS1 are the W3IDATMD flags of IMOD
    si.inflags1_2 = s.in.flcur != 0;
    si.inflags1_3 = s.in.flwind != 0;
    si.inflags1_5 = s.in.fltaua != 0;
    si.inflags1_6 = s.in.flrhoa != 0;
    si.gtype = g.gtype;
    const auto e = adat::w3seta(s.adat, s.nl1, s.at, imod, si);
    if (e != adat::W3setaStatus::ok) return ended(static_cast<int>(e), "W3SETA");
  }
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 1d");
  if (const int e = set4(h.w3seti)) return ended(e, "W3SETI");
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 1e");

  // 1.b  Number of processors and processor number, W3_SHRD, :562-565.
  o.ntproc = 1;
  o.naproc = 1;
  o.iaproc = 1;
  o.iostyp = 1;
  memunit = 10000 + o.iaproc;  // :575
  if (o.iostyp <= 1) {  // :577-585
    o.napfld = std::max(1, o.naproc - 1);
    o.nappnt = std::max(1, o.naproc - 2);
    o.naptrk = std::max(1, o.naproc - 5);
    o.naprst = o.naproc;
    o.napbpt = std::max(1, o.naproc - 3);
    o.napprt = std::max(1, o.naproc - 4);
  } else {  // :586-619, unreachable under SHRD (header)
    o.nappnt = o.naproc;
    if (o.unipts != 0 && o.upproc != 0) o.naproc = std::max(1, o.ntproc - 1);
    o.napfld = o.naproc;
    o.naprst = o.naproc;
    o.napbpt = o.naproc;
    o.naptrk = o.naproc;
    o.napprt = o.naproc;
    if (o.iostyp == 2) {
      o.naproc = std::max(1, o.naproc - 1);
    } else if (o.iostyp == 3) {
      if (a.odat[2] > 0 || a.odat[32] > 0) {
        o.napfld = o.naproc;
        o.naproc = std::max(1, o.naproc - 1);
      }
      if (a.odat[12] > 0) {
        o.naptrk = o.naproc;
        o.naproc = std::max(1, o.naproc - 1);
      }
      if (a.odat[27] > 0) {
        o.napprt = o.naproc;
        o.naproc = std::max(1, o.naproc - 1);
      }
      if (a.odat[7] > 0) o.nappnt = o.naproc;
      if (a.odat[17] > 0) o.naprst = o.naproc;
      if (a.odat[22] > 0) o.napbpt = o.naproc;
      if (a.odat[7] > 0 || a.odat[17] > 0 || a.odat[22] > 0) o.naproc = std::max(1, o.naproc - 1);
    }
  }

  // :622-623
  const Real fracos = static_cast<Real>(o.ntproc - o.naproc) * static_cast<Real>(100.) /
                      static_cast<Real>(o.ntproc);
  if (fracos > kCritos && o.iaproc == o.naperr) {
    if (!h.write_8002) return missing("write_8002");
    h.write_8002(s, fracos);
  }

  // :642-656
  s.c.lpdlib = 0;
  if (g.fstotalimp != 0 && s.c.lpdlib == 0) {
    if (!h.write_ndse_list || !h.flush_ndse) return missing("write_ndse_list");
    h.write_ndse_list(s, "IMPTOTAL is selected");
    h.write_ndse_list(s, "But PDLIB is not");
    h.flush_ndse(s);
    exec.fence("ww::init::w3init");
    st.stop = true;
    return st;
  } else if (g.fstotalexp != 0 && s.c.lpdlib == 0) {
    if (!h.write_ndse_list || !h.flush_ndse) return missing("write_ndse_list");
    h.write_ndse_list(s, "EXPTOTAL is selected");
    h.write_ndse_list(s, "But PDLIB is not");
    h.flush_ndse(s);
    exec.fence("ww::init::w3init");
    st.stop = true;
    return st;
  }

  // 1.c  Open files, :668-704. LFILE is CHARACTER(30): 'log.'//FEXT(:IE) truncated or padded.
  const std::size_t ie = detail::len_trim(a.fext);
  const FChar<30> lfile{"log." + a.fext.substr(0, ie)};
  const std::size_t ifl = lfile.len_trim();
  const std::size_t jpre = g.fnmpre.len_trim();
  if (o.iaproc == o.naplog) {  // :685, OUTPTS(IMOD)%IAPROC and %NAPLOG are o's fields
    if (!h.open_log) return missing("open_log");
    const int ierr = h.open_log(s, a.mds[0], std::string(g.fnmpre.c.data(), jpre) +
                                                 std::string(lfile.c.data(), ifl));
    if (ierr != 0) {  // :687, EXTOPN(NDSE, IERR, 'W3INIT', 'LOG', 1) -> EXTCDE(1)
      st.fmt = 1000;
      st.iostat = ierr;
      return ended(1, "W3INIT");
    }
  }
  o.nds = a.mds;  // :700-704
  o.ndso = o.nds[0];
  o.ndse = o.nds[1];
  o.ndst = o.nds[2];
  o.screen = o.nds[3];

  if (!h.itrace) return missing("itrace");
  h.itrace(s, a.mtrace[0], a.mtrace[1]);  // :708
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 2");
  if (o.iaproc == o.naplog) {  // :714-718
    if (!h.log_900) return missing("log_900");
    h.log_900(s, kWwver);
  }
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 2a");

  // 2.a  Model definition file, :735-747.
  st.iogr = iogr::w3iogr("READ", g, s.saved, h.iogr, imod, a.fext);
  if (st.iogr.exit_code != 0) return ended(st.iogr.exit_code, "W3IOGR");
  if (g.gtype == iogr::kUngType) {
    if (!h.spatial_grid || !h.nvectri || !h.coordmax || !h.area_si) return missing("spatial_grid");
    if (const int e = h.spatial_grid(s)) return ended(e, "SPATIAL_GRID");
    if (const int e = h.nvectri(s)) return ended(e, "NVECTRI");
    if (const int e = h.coordmax(s)) return ended(e, "COORDMAX");
    if (const int e = h.area_si(s, 1)) return ended(e, "AREA_SI");
  }
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 2b");
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 2c");
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 2cc");
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 2d");

  // :780
  {
    const int warn = iogo::w3flgrdupdt(a.flgrd, a.flgr2, a.flgd, a.flg2, g.e3df.data(),
                                       g.p2msf.data(), g.us3df.data(), g.usspf.data());
    if (warn != 0) {
      if (!h.write_flgrdupdt) return missing("write_flgrdupdt");
      h.write_flgrdupdt(s, warn);
    }
  }
  const Real factor = (g.flagll != 0) ? static_cast<Real>(1.) : kFactorXY;  // :786-790
  if (o.iaproc == o.naplog) {  // :791
    if (!h.write_log) return missing("write_log");
    h.write_log(s, 920, std::string());
  }

  // 2.b  Save MAPSTA, :795-796.
  const std::size_t nxy = static_cast<std::size_t>(g.nx) * static_cast<std::size_t>(g.ny);
  if (g.nx < 0 || g.ny < 0 || g.mapsta.size() != nxy || g.mapst2.size() != nxy)
    return ended(kExitShape, "W3INIT");
  std::vector<std::int32_t> maptst;
  try {
    maptst = g.mapsta;
  } catch (const std::bad_alloc&) {
    return ended(kExitAllocate, "W3INIT");
  }
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 2e");

  // 2.c  MPP preparation, :803-805. NSEALM goes through the alias W3SETA :540 pointed.
  {
    int nseal_out = 0, nsealm_out = 0;
    parall::set_up_nseal_nsealm(nseal_out, nsealm_out, false, false, g.nsea, s.c.lpdlib != 0,
                                g.gtype, o.iaproc, o.naproc, 0, 0);
    s.gx.nseal = nseal_out;
    adat::Wadat<ExecSpace>* base = adat::selected(s.adat, s.at.base);
    if (base == nullptr) return ended(kExitUnassociated, "W3INIT");
    base->nsealm = nsealm_out;
  }
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 2f");
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 2g");

  // 2.c.2 Allocate arrays, :839-852.
  if (!h.w3dimw) return missing("w3dimw");
  if (o.iaproc <= o.naproc) {
    if (const int e = h.w3dimw(s, imod, o.ndse, o.ndst, std::nullopt)) return ended(e, "W3DIMW");
    memcheck(memunit, "memcheck_____: WW3_INIT SECTION 2h");
  } else {
    if (const int e = h.w3dimw(s, imod, o.ndse, o.ndst, false)) return ended(e, "W3DIMW");
    memcheck(memunit, "memcheck_____: WW3_INIT SECTION 2i");
  }
  {
    adat::W3dimaGrid dg;  // :849, the state of grid IMOD and the modules W3DIMA reads
    dg.ngrids = s.gx.ngrids;
    dg.nsea = g.nsea;
    dg.nseal = s.gx.nseal;
    dg.nx = g.nx;
    dg.ny = g.ny;
    dg.nk = g.nk;
    dg.nth = g.nth;
    dg.nspec = g.nspec;
    dg.gtype = g.gtype;
    for (int j = 0; j < 5; ++j)
      for (int i = 0; i < 3; ++i)
        dg.e3df[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)] =
            g.e3df[static_cast<std::size_t>(j * 3 + i)];
    dg.p2msf = g.p2msf;
    dg.us3df = g.us3df;
    dg.usspf = g.usspf;
    dg.noswll = g.noswll;
    dg.undef = s.c.undef;
    dg.flcur = s.in.flcur != 0;
    dg.flwind = s.in.flwind != 0;
    dg.fltaua = s.in.fltaua != 0;
    dg.flrhoa = s.in.flrhoa != 0;
    dg.lpdlib = s.c.lpdlib != 0;
    const auto e = adat::w3dima(exec, s.adat, s.at, imod, dg);
    if (e != adat::W3dimaStatus::ok) return ended(static_cast<int>(e), "W3DIMA");
    // The W3SETA(IMOD) of w3adatmd.F90:1467, which W3DIMA's port leaves to its caller.
    adat::W3setaInputs si;
    si.inflags1_2 = s.in.flcur != 0;
    si.inflags1_3 = s.in.flwind != 0;
    si.inflags1_5 = s.in.fltaua != 0;
    si.inflags1_6 = s.in.flrhoa != 0;
    si.gtype = g.gtype;
    const auto e2 = adat::w3seta(s.adat, s.nl1, s.at, imod, si);
    if (e2 != adat::W3setaStatus::ok) return ended(static_cast<int>(e2), "W3SETA");
  }
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 2j");
  if (!h.w3dimi) return missing("w3dimi");
  if (const int e = h.w3dimi(s, imod, o.ndse, o.ndst, a.flagstidein)) return ended(e, "W3DIMI");
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 3");

  // The W3ADATMD aliases from here on: DW, UA .. AS, WN of the AINIT group; CG, IAPPRO of the
  // FL_ALL group; FLCOLD, FLIWND of the unconditional one (adat_w3seta.hpp).
  adat::Wadat<ExecSpace>* wa = adat::selected(s.adat, s.at.ainit);
  adat::Wadat<ExecSpace>* wf = adat::selected(s.adat, s.at.fl_all);
  adat::WadatScalars* sc = adat::selected_scalars(s.ascal, s.at.base);
  if (wa == nullptr || wf == nullptr || sc == nullptr) return ended(kExitUnassociated, "W3INIT");

  // 2.c.4 IAPPRO = 1, :871.
  Kokkos::deep_copy(exec, wf->iappro, 1);

  // 3.a  Read restart file, :971-982.
  Kokkos::deep_copy(exec, s.w.va, Real(0));
  if (!h.w3iors_read) return missing("w3iors_read");
  if (g.nk < 1 || g.sig.size() < static_cast<std::size_t>(g.nk) + 2)
    return ended(kExitShape, "W3INIT");
  exec.fence("ww::init::w3init");
  if (const int e = h.w3iors_read(s, o.nds[5], g.sig[static_cast<std::size_t>(g.nk)], imod))
    return ended(e, "W3IORS");
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 3a");

  sc->flcold = s.gx.rstype <= 1 || s.gx.rstype == 4;  // :987
  if (o.iaproc == o.naplog) {  // :988-998, A item of length 23, 18, 18, 13
    if (!h.write_log) return missing("write_log");
    if (s.gx.rstype == 0) {
      h.write_log(s, 930, "cold start (idealized).");
    } else if (s.gx.rstype == 1) {
      h.write_log(s, 930, "cold start (wind).");
    } else if (s.gx.rstype == 4) {
      h.write_log(s, 930, "cold start (calm).");
    } else {
      h.write_log(s, 930, "full restart.");
    }
  }

  // 3.b  Compare MAPSTA from grid and restart, :1009-1016.
  for (int ix = 1; ix <= g.nx; ++ix) {
    for (int iy = 1; iy <= g.ny; ++iy) {
      const std::size_t k = static_cast<std::size_t>(ix - 1) * static_cast<std::size_t>(g.ny) +
                            static_cast<std::size_t>(iy - 1);
      const std::int32_t ms = g.mapsta[k];
      const std::int32_t mt = maptst[k];
      if ((ms < 0 ? -ms : ms) == 2 || (mt < 0 ? -mt : mt) == 2) g.mapsta[k] = detail::isign(mt, ms);
    }
  }
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 3b");

  // 3.c, 3.d  :1033-1048.
  sc->fliwnd = s.gx.rstype == 1;
  if (s.gx.rstype == 4) Kokkos::deep_copy(exec, s.w.va, Real(0));
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 4");

  // 3.e  :1053.
  if (s.in.flcur == 0) g.flck = 0;

  // 4.   Output times, :1066-1246.
  if (o.notype > 8) return ended(kExitShape, "W3INIT");  // TONEXT(2,8): the Fortran overruns
  auto ton = [&](int i, int j) -> int& { return o.tonext[static_cast<std::size_t>((j - 1) * 2 + i - 1)]; };
  auto tol = [&](int i, int j) -> int& { return o.tolast[static_cast<std::size_t>((j - 1) * 2 + i - 1)]; };
  auto od = [&](int i) { return a.odat[static_cast<std::size_t>(i - 1)]; };
  auto dto = [&](int j) -> Real& { return o.dtout[static_cast<std::size_t>(j - 1)]; };
  auto flo = [&](int j) -> FLogical& { return o.flout[static_cast<std::size_t>(j - 1)]; };
  for (int j = 1; j <= o.notype; ++j) {  // :1066-1073
    const int j0 = (j - 1) * 5;
    ton(1, j) = od(j0 + 1);
    ton(2, j) = od(j0 + 2);
    dto(j) = static_cast<Real>(od(j0 + 3));
    tol(1, j) = od(j0 + 4);
    tol(2, j) = od(j0 + 5);
  }
  {
    const int j = 8;  // :1076-1087
    const int j0 = (j - 1) * 5;
    if (od(j0 + 1) != 0) {
      ton(1, j) = od(j0 + 1);
      ton(2, j) = od(j0 + 2);
      dto(j) = static_cast<Real>(od(j0 + 3));
      tol(1, j) = od(j0 + 4);
      tol(2, j) = od(j0 + 5);
      flo(8) = 1;
    } else {
      flo(8) = 0;
    }
  }
  flo(1) = 0;  // :1091-1098
  for (int i = 0; i < kNogrp * kNgrpp; ++i) o.flogrd[static_cast<std::size_t>(i)] = a.flgrd[i];
  for (int i = 0; i < kNogrp; ++i) o.flogd[static_cast<std::size_t>(i)] = a.flgd[i];
  for (int j = 1; j <= kNogrp; ++j)
    for (int k = 1; k <= kNgrpp; ++k)
      flo(1) = detail::lor_raw(flo(1), o.flogrd[static_cast<std::size_t>((k - 1) * kNogrp + j - 1)]);
  flo(7) = 0;  // :1103-1110
  for (int i = 0; i < kNogrp * kNgrpp; ++i) o.flogr2[static_cast<std::size_t>(i)] = a.flgr2[i];
  for (int i = 0; i < kNogrp; ++i) o.flog2[static_cast<std::size_t>(i)] = a.flg2[i];
  for (int j = 1; j <= kNogrp; ++j)
    for (int k = 1; k <= kNgrpp; ++k)
      flo(7) = detail::lor_raw(flo(7), o.flogr2[static_cast<std::size_t>((k - 1) * kNogrp + j - 1)]);
  flo(2) = a.npt > 0;  // :1112
  flo(3) = 1;          // :1114
  flo(4) = 1;          // :1116
  flo(5) = g.flbpo;    // :1118, the word as it is
  if (g.flbpo != 0) {  // :1119-1123
    if (!h.w3dmo5) return missing("w3dmo5");
    exec.fence("ww::init::w3init");
    if (const int e = h.w3dmo5(s, imod, o.ndse, o.ndst, 4)) return ended(e, "W3DMO5");
  } else {
    dto(5) = static_cast<Real>(0.);
  }
  o.ix0 = std::max(1, a.iprt[0]);  // :1125-1132
  o.ixn = std::min(g.nx, a.iprt[1]);
  o.ixs = std::max(1, a.iprt[2]);
  o.iy0 = std::max(1, a.iprt[3]);
  o.iyn = std::min(g.ny, a.iprt[4]);
  o.iys = std::max(1, a.iprt[5]);
  o.flform = *a.prtfrm;
  flo(6) = o.ix0 <= o.ixn && o.iy0 <= o.iyn;
  o.tofrst[0] = -1;  // :1136-1137
  o.tofrst[1] = 0;

  if (!h.dsec21 || !h.tick21) return missing("dsec21");
  // :1147-1197 for J = 1..NOTYPE, and :1201-1246 for J = 8: the same statements.
  auto output_type = [&](int j) {
    dto(j) = (static_cast<Real>(0.) > dto(j)) ? static_cast<Real>(0.) : dto(j);  // maxss
    if (!(flo(j) != 0 && dto(j) > static_cast<Real>(0.5))) flo(j) = 0;
    if (flo(j) == 0) return;
    std::array<int, 2> tout{ton(1, j), ton(2, j)};
    const std::array<int, 2> tlst{tol(1, j), tol(2, j)};
    for (;;) {
      const Real dttst = h.dsec21(s.w.time.data(), tout.data());
      if ((j != 4 && dttst < static_cast<Real>(0.)) || (j == 4 && dttst <= static_cast<Real>(0.))) {
        h.tick21(tout.data(), dto(j));
      } else {
        break;
      }
    }
    ton(1, j) = tout[0];
    ton(2, j) = tout[1];
    {
      const Real dttst = h.dsec21(tout.data(), tlst.data());
      if (dttst < static_cast<Real>(0.)) flo(j) = 0;
    }
    if (flo(j) != 0) {
      if (o.tofrst[0] == -1) {
        o.tofrst = tout;
      } else {
        const Real dttst = h.dsec21(tout.data(), o.tofrst.data());
        if (dttst > static_cast<Real>(0.)) o.tofrst = tout;
      }
    }
  };
  for (int j = 1; j <= o.notype; ++j) output_type(j);
  output_type(8);
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 5");

  // 4.d  :1262.
  if (flo(2) != 0) {
    if (!h.w3iopp) return missing("w3iopp");
    exec.fence("ww::init::w3init");
    if (const int e = h.w3iopp(s, a.npt, a.xpt, a.ypt, a.pnames, imod, 1)) return ended(e, "W3IOPP");
  }

  // 5.a  Calculate depth, :1285-1370, on the host in the Fortran's order.
  const int nsea = g.nsea;
  const int nseal = s.gx.nseal;
  if (nsea < 0 || g.zb.size() < static_cast<std::size_t>(nsea) ||
      g.mapsf.size() < 3 * static_cast<std::size_t>(nsea) ||
      wa->dw.extent(0) < static_cast<std::size_t>(nsea) + 1 ||
      s.w.wlv.extent(0) < static_cast<std::size_t>(nsea) ||
      s.w.va.extent(0) < static_cast<std::size_t>(g.nspec) ||
      s.w.va.extent(1) < static_cast<std::size_t>(nseal) + 1)
    return ended(kExitShape, "W3INIT");
  for (std::size_t k = 0; k < nxy; ++k) maptst[k] = (g.mapst2[k] / 2) % 2;  // :1285
  for (std::size_t k = 0; k < nxy; ++k) g.mapst2[k] = g.mapst2[k] - 2 * maptst[k];  // :1286
  exec.fence("ww::init::w3init");
  auto dw_h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), wa->dw);
  auto wlv_h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), s.w.wlv);
  for (int isea = 1; isea <= nsea; ++isea) {  // :1300-1326
    const std::size_t is = static_cast<std::size_t>(isea - 1);
    const int ix = g.mapsf[is];
    const int iy = g.mapsf[static_cast<std::size_t>(nsea) + is];
    const Real wlveff = wlv_h(is);
    const Real d = wlveff - g.zb[is];
    dw_h(isea) = (d > static_cast<Real>(0.)) ? d : static_cast<Real>(0.);  // maxss 0,D
    if (wlveff - g.zb[is] <= static_cast<Real>(0.)) {
      const std::size_t k = static_cast<std::size_t>(ix - 1) * static_cast<std::size_t>(g.ny) +
                            static_cast<std::size_t>(iy - 1);
      maptst[k] = 1;
      g.mapsta[k] = -(g.mapsta[k] < 0 ? -g.mapsta[k] : g.mapsta[k]);
    }
  }
  Kokkos::View<int*, Kokkos::LayoutLeft, Kokkos::HostSpace> dry_h("ww::init::dry",
                                                                  static_cast<std::size_t>(nseal));
  for (int jsea = 1; jsea <= nseal; ++jsea) {  // :1334-1350
    int isea = 0;
    parall::init_get_isea(isea, jsea, false, false, s.c.lpdlib != 0, g.gtype, o.iaproc,
                          o.naproc, nullptr);
    const std::size_t is = static_cast<std::size_t>(isea - 1);
    const Real wlveff = wlv_h(is);
    const Real d = wlveff - g.zb[is];
    dw_h(isea) = (d > static_cast<Real>(0.)) ? d : static_cast<Real>(0.);
    if (wlveff - g.zb[is] <= static_cast<Real>(0.)) dry_h(jsea - 1) = 1;
  }
  for (std::size_t k = 0; k < nxy; ++k) g.mapst2[k] = g.mapst2[k] + 2 * maptst[k];  // :1368
  Kokkos::deep_copy(exec, wa->dw, dw_h);
  {
    auto dry = Kokkos::create_mirror_view_and_copy(Mem(), dry_h);
    init_zero_dry<ExecSpace>(exec, s.w.va, dry, g.nspec, nseal);
  }
  maptst = std::vector<std::int32_t>();  // :1370
  memcheck(memunit, "memcheck_____: WW3_INIT SECTION 6");

  // 5.b  Wavenumbers and group velocities, :1394-1439.
  if (wa->wn.extent(0) < static_cast<std::size_t>(g.nk) + 2 ||
      wa->wn.extent(1) < static_cast<std::size_t>(nsea) + 1 ||
      wf->cg.extent(0) < static_cast<std::size_t>(g.nk) + 2 ||
      wf->cg.extent(1) < static_cast<std::size_t>(nsea) + 1)
    return ended(kExitShape, "W3INIT");
  {
    auto copy_in = [](const char* label, const Real* p, std::size_t n) {
      R1 d(Kokkos::view_alloc(Kokkos::WithoutInitializing, std::string(label)), n);
      Kokkos::deep_copy(d, HostR1(p, n));
      return CR1(d);
    };
    const CR1 zb = copy_in("ww::init::ZB", g.zb.data(), static_cast<std::size_t>(nsea));
    const CR1 sig = copy_in("ww::init::SIG", g.sig.data(), static_cast<std::size_t>(g.nk) + 2);
    const CR1 ewn1 = copy_in("ww::init::EWN1", s.saved.ewn1.data(), s.saved.ewn1.size());
    const CR1 ecg1 = copy_in("ww::init::ECG1", s.saved.ecg1.data(), s.saved.ecg1.size());
    const prop::DispTable tab{ewn1.data(), ecg1.data(), s.saved.dsie, s.saved.n1max};
    init_wavenumbers<ExecSpace>(exec, wa->dw, zb, sig, wa->wn,
                                wf->cg, tab, g.dmin, nsea, g.nk);
  }

  // 6.   Initialize arrays, :1445-1453.
  Kokkos::deep_copy(exec, wa->ua, Real(0));
  Kokkos::deep_copy(exec, wa->ud, Real(0));
  Kokkos::deep_copy(exec, wa->u10, Real(0));
  Kokkos::deep_copy(exec, wa->u10d, Real(0));
  Kokkos::deep_copy(exec, wa->as, s.c.undef);
  Kokkos::deep_copy(exec, Kokkos::subview(wa->as, 0), Real(0));
  Kokkos::deep_copy(exec, Kokkos::subview(wa->dw, 0), Real(0));
  exec.fence("ww::init::w3init");

  // 7.   Write info to log file, :1457-1546.
  if (o.iaproc == o.naplog) {
    if (!h.write_log || !h.write_977 || !h.write_point || !h.stme21) return missing("write_log");
    const std::string pre = "Prescribed", no = "No";
    auto pair = [&](FLogical fl, int label) {
      if (fl != 0) h.write_log(s, label, pre);
      if (fl == 0) h.write_log(s, label, no);
    };
    h.write_log(s, 970, detail::full(g.gname));
    pair(s.in.fllev, 971);
    pair(s.in.flcur, 972);
    pair(s.in.flwind, 973);
    pair(s.in.flice, 974);
    pair(s.in.fltaua, 988);
    pair(s.in.flrhoa, 989);
    pair(s.in.flmdn, 9972);
    pair(s.in.flmth, 9971);
    pair(s.in.flmvs, 9970);
    pair(s.in.flic1, 9973);
    pair(s.in.flic2, 9974);
    pair(s.in.flic3, 9975);
    pair(s.in.flic4, 9976);
    pair(s.in.flic5, 9977);
    if (flo(1) != 0) {  // :1491-1498
      h.write_log(s, 975, std::string());
      for (int j = 1; j <= kNogrp; ++j)
        for (int k = 1; k <= kNgrpp; ++k) {
          const std::size_t i = static_cast<std::size_t>((k - 1) * kNogrp + j - 1);
          if (o.flogrd[i] != 0) h.write_log(s, 976, detail::full(o.idout[i]));
        }
    }
    if (flo(7) != 0) {  // :1500-1507
      h.write_log(s, 987, std::string());
      for (int j = 1; j <= kNogrp; ++j)
        for (int k = 1; k <= kNgrpp; ++k) {
          const std::size_t i = static_cast<std::size_t>((k - 1) * kNogrp + j - 1);
          if (o.flogr2[i] != 0) h.write_log(s, 976, detail::full(o.idout[i]));
        }
    }
    if (flo(2) != 0) {  // :1509-1527
      h.write_977(s, o.nopts);
      if (o.nopts == 0) {
        h.write_log(s, 978, std::string());
      } else {
        h.write_log(s, g.flagll != 0 ? 979 : 985, std::string());
        if (o.ptloc.size() < 2 * static_cast<std::size_t>(o.nopts) ||
            o.ptnme.size() < static_cast<std::size_t>(o.nopts))
          return ended(kExitShape, "W3INIT");
        for (int ip = 1; ip <= o.nopts; ++ip) {
          const std::size_t p = static_cast<std::size_t>(ip - 1);
          const Real x = factor * o.ptloc[2 * p];
          const Real y = factor * o.ptloc[2 * p + 1];
          h.write_point(s, g.flagll != 0 ? 980 : 986, ip, x, y, detail::full(o.ptnme[p]));
        }
      }
    }
    h.write_log(s, 981, h.stme21(s.w.time.data()));  // :1529-1544
    if (s.in.fllev != 0) h.write_log(s, 982, h.stme21(s.w.tlev.data()));
    if (s.in.flice != 0) h.write_log(s, 983, h.stme21(s.w.tice.data()));
    if (s.in.flrhoa != 0) h.write_log(s, 990, h.stme21(s.w.trho.data()));
    h.write_log(s, 984, std::string());
  }

  if (o.nopts == 0) flo(2) = 0;  // :1548
  memcheck(memunit,
           "memcheck_____: WW3_INIT SECTION 7 - After allocation of group velocities");
  exec.fence("ww::init::w3init");
  return st;
}

}  // namespace ww::init
