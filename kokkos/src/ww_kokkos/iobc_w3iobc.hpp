// kokkos/src/ww_kokkos/iobc_w3iobc.hpp
// W3IOBC, ported: writes the boundary output files nest1.<ext> .. nest9.<ext>, reads the
// boundary input file nest.<ext> one time record per call, and dumps the input spectra for
// ww3_multi ('DUMP'). On a WRITE it gathers the output spectra from VA as frequency spectra,
// VA*SIG2/CG, the only floating-point array statement of the routine. Phase 1, translated, not
// wired into WW3, not regtested.
//
// WW3 heritage: SUBROUTINE W3IOBC, WW3-noaa/model/src/w3iobcmd.F90:100-852 at NOAA-EMC/WW3
// 03adda91 (identical in the h0ffmann/WW3 fork at the WW3/ pin: `git diff 03adda91 HEAD --
// model/src/w3iobcmd.F90` is empty), author H. L. Tolman. Only the paths switches/switch_lab_shrd
// compiles are ported (read from `gfortran -E` with the defines of WW3's flags.make): W3_SHRD
// gathers ABPOS from VA (:666-671). Not compiled and not ported: W3_DIST (:676-681), W3_RTD
// (:471-508 and :683-693, the rotated-pole remap and W3ACTURN), W3_SMC (W3SMCGMP, :512-518),
// W3_T, W3_T0, W3_T1 test output, W3_S STRACE.
//
// W3IOBC does not interpolate: the time interpolation of ABPI0/ABPIN is W3UBPT (w3updtmd.F90),
// the spatial weights IPBPI/RDBPI come from ww3_grid, and a change of spectral grid is W3CSPC,
// a hook here.
//
// Arithmetic, checked against the -O3 GIMPLE and assembly of w3iobcmd.F90 (gfortran 16.2.0,
// WW3's Release flags, switch_lab_shrd; .text and .rodata byte-identical to the CMake object):
// - ABPOS(IS,ISOUT) = VA(IS,ISEA) * SIG2(IS) / CG(1+(IS-1)/NTH,ISEA) (:668-669) is one mulss
//   and one divss per element, (VA*SIG2)/CG, not vectorised; (IS-1)/NTH is a signed int
//   division. abpos_shrd() is that statement; contraction is off in the ww_kokkos target.
// - SPCONV (:458-461) is short-circuit in the source order: NKI/=NK, NTHI/=NTH, then
//   ABS(XFRI/XFR-1.) > 0.01 and ABS(FR1I/FR1-1.) > 0.01 (divss, subss of 1.0, andps, comiss with
//   0.01f = 9.99999977648258209228515625e-3), then ABS(TH1I-TH(1)) > DTH*0.01 (subss, andps,
//   mulss, comiss). No operand reaches a store other than the LOGICAL, so the order cannot
//   change a bit; a NaN makes a test false in both.
// - DCIN=0.1 in the W3GFPT call (:524) is the REAL(4) 0.100000001490116119384765625.
// Everything else is copies and integer index arithmetic.
//
// File format: gfortran sequential unformatted, CONVERT=file_endian, 'big_endian' from WW3's
// CMake (the -O3 GIMPLE sets open_parm.convert = "big_endian" for the OPENs at :334, :352,
// :363), the records of iogr_unformatted.hpp. The record layout is the table of
// kokkos/port_report/W3IOBC_KOKKOS.md.
//
// Memory contract (phase 1). VA, CG, SIG2, ISBPO and ABPOS are Views in ExecSpace's memory
// space (IobcViews); the gather is a kernel on it, and ABPOS goes through a host mirror to the
// file. Every other array W3IOBC touches (the point tables, ABPI0, ABPIN) is host state in
// IobcState, as the routines that read them (W3UBPT, W3IOGR) are not ported on the device yet.
#pragma once

#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include <Kokkos_Core.hpp>

#include "iogr_state.hpp"
#include "iogr_unformatted.hpp"
#include "iogr_w3iogr.hpp"

namespace ww::iobc {

using ww::iogr::FChar;
using ww::iogr::FLogical;

// ---- Element function of the SHRD gather (host and device) ------------------------------

/// ABPOS(IS,ISOUT) = VA(IS,ISEA) * SIG2(IS) / CG(1+(IS-1)/NTH,ISEA), w3iobcmd.F90:668-669:
/// mulss, then divss (GIMPLE _811 = _800 * _810; _843 = _811 / _830).
KOKKOS_INLINE_FUNCTION float abpos_shrd(float va, float sig2, float cg) {
  const float p = va * sig2;
  return p / cg;
}

/// The Views of the gather, in ExecSpace's memory space, LayoutLeft, extents in Fortran order;
/// index (i-l) for Fortran index i of a dimension with lower bound l.
template <class ExecSpace>
struct IobcViews {
  using M = typename ExecSpace::memory_space;
  Kokkos::View<const float**, Kokkos::LayoutLeft, M> va;  // VA(NSPEC,0:NSEALM): va(is-1, isea)
  Kokkos::View<const float**, Kokkos::LayoutLeft, M> cg;  // CG(0:NK+1,0:NSEA): cg(ik, isea)
  Kokkos::View<const float*, M> sig2;                     // SIG2(NSPEC): sig2(is-1)
  Kokkos::View<const std::int32_t*, M> isbpo;             // ISBPO(4*NBO(NFBPO)): isbpo(i-1)
  Kokkos::View<float**, Kokkos::LayoutLeft, M> abpos;     // ABPOS(NSPEC,0:NBO2(NFBPO))
};

/// DO ISOUT=first,last: ISEA = ISBPO(ISOUT); DO IS=1,NSPEC: ABPOS(IS,ISOUT) = ... (:660-671).
/// Each element depends on its own inputs only, with no reduction, so any execution space and
/// any order give the bits of the Fortran loop.
template <class ExecSpace>
void iobc_gather_shrd(const ExecSpace& space, const IobcViews<ExecSpace>& v, int first,
                      int last, int nspec, int nth) {
  if (last < first) return;
  const auto va = v.va;
  const auto cg = v.cg;
  const auto sig2 = v.sig2;
  const auto isbpo = v.isbpo;
  const auto abpos = v.abpos;
  Kokkos::parallel_for(
      "ww::iobc::gather_shrd", Kokkos::RangePolicy<ExecSpace>(space, first, last + 1),
      KOKKOS_LAMBDA(const int isout) {
        const int isea = isbpo(isout - 1);
        for (int is = 1; is <= nspec; ++is)
          abpos(is - 1, isout) = abpos_shrd(va(is - 1, isea), sig2(is - 1),
                                            cg(1 + (is - 1) / nth, isea));
      });
  space.fence("ww::iobc::gather_shrd");
}

// ---- Constants of the file ---------------------------------------------------------------

inline const FChar<10>& verbptbc() {  // VERBPTBC, w3iobcmd.F90:78
  static const FChar<10> v{std::string("2018-03-01")};
  return v;
}
inline const FChar<32>& idstrbc() {  // IDSTRBC, w3iobcmd.F90:79-80 (the literal at :80)
  static const FChar<32> v{std::string("WAVEWATCH III BOUNDARY DATA FILE")};
  return v;
}

/// Codes outside the Fortran's EXTCDE codes (the W3IOGR ones are reused: kExitGfortranRuntime 2,
/// kExitMissingHook, kExitShape, kExitNfbpoBounds).
inline constexpr int kExitNullImod = -104;         // FORMAT 1001 writes the absent IMOD (:337)
inline constexpr int kExitUnitNotConnected = -105;  // a transfer on a unit W3IOBC never opened
/// LIBERROR_OPTION_CONFLICT: a sequential READ after the end of the file (data_transfer_init,
/// io/transfer.c:3357-3364, gcc 16.2.0; warn_std is set without -std, and a standalone program
/// built with WW3's Release flags gets 5001).
inline constexpr int kIostatOptionConflict = 5001;

// ---- Units -------------------------------------------------------------------------------

/// The Fortran units W3IOBC opens and keeps connected between calls: NDSB for 'READ' and
/// 'DUMP', NDSL(1..NFBPO) for 'WRITE'. A unit records which transfers its OPEN allowed.
class IobcUnits {
 public:
  struct Unit {
    std::unique_ptr<std::FILE, ww::iogr::detail::FileCloser> f;
    bool can_read = false;
    bool can_write = false;
    // endfile == AFTER_ENDFILE: a READ hit the end of the file (hit_eof, io/transfer.c). The
    // next READ then fails with LIBERROR_OPTION_CONFLICT, IOSTAT 5001, without moving.
    bool after_endfile = false;
  };
  /// The unit's stream, or nullptr when it is not connected.
  Unit* get(int unit) {
    const auto it = u_.find(unit);
    return it == u_.end() ? nullptr : &it->second;
  }
  /// OPEN(unit, FILE=path, FORM='UNFORMATTED', CONVERT=file_endian, IOSTAT=IERR[, STATUS='OLD']),
  /// :334-335 (status_old), :352-353 and :363-364. Returns IERR. A connected unit is closed
  /// first, as an OPEN of a connected unit to another file does. The open(2) calls are those of
  /// libgfortran's regular_file2 (io/unix.c, gcc 16.2.0) for an OPEN without ACTION: O_RDWR
  /// (with O_CREAT unless STATUS='OLD'); on EACCES, EPERM or EROFS, O_RDONLY without O_CREAT;
  /// on EACCES, EPERM or ENOENT after that, O_WRONLY. A directory fails the O_RDWR with EISDIR
  /// in both (an fopen "rb" would have opened it). A file that opens only the other way fails
  /// at its first transfer, not here. Without STATUS the first WRITE truncates the file after
  /// its record (st_write_done); the ftruncate at the OPEN ends the file the same.
  /// ⚠ IERR is errno as generate_error reads it (runtime/error.c), after gf_strerror built the
  /// message: a standalone gfortran 16.2 program got IOSTAT 2 for an EISDIR failure (the
  /// message-catalogue lookup left ENOENT), so the value EXTOPN prints may differ; the branch
  /// taken (IERR /= 0) does not.
  int open(int unit, const std::string& path, bool status_old) {
    u_.erase(unit);
    const int cr = status_old ? 0 : O_CREAT;
    constexpr mode_t mode = 0666;
    Unit nu;
    int fd = ::open(path.c_str(), O_RDWR | cr | O_CLOEXEC, mode);
    if (fd >= 0) {
      nu.can_read = nu.can_write = true;
    } else if (errno == EACCES || errno == EPERM || errno == EROFS) {
      fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC, mode);
      if (fd >= 0) {
        nu.can_read = true;
      } else if (errno == EACCES || errno == EPERM || errno == ENOENT) {
        fd = ::open(path.c_str(), O_WRONLY | cr | O_CLOEXEC, mode);
        if (fd >= 0) nu.can_write = true;
      }
    }
    if (fd < 0) return errno;  // LIBERROR_OS sets IOSTAT to errno (runtime/error.c)
    if (!status_old && nu.can_write && ::ftruncate(fd, 0) != 0) {
      const int err = errno;
      ::close(fd);
      return err;
    }
    nu.f.reset(::fdopen(fd, nu.can_read && nu.can_write ? "r+b" : nu.can_read ? "rb" : "wb"));
    if (!nu.f) {
      const int err = errno;
      ::close(fd);
      return err;
    }
    u_.emplace(unit, std::move(nu));
    return 0;
  }
  void close(int unit) { u_.erase(unit); }

 private:
  std::map<int, Unit> u_;
};

/// BACKSPACE(unit) without IOSTAT on an unformatted sequential unit, as libgfortran's
/// st_backspace and unformatted_backspace do it (io/file_pos.c, gcc 16.2.0, read 2026-10-16): at
/// the start of the file nothing; otherwise read the 4-byte marker before the position, seek
/// back over the subrecord and both markers, and repeat while the marker was negative (a
/// continued subrecord). Returns false where libgfortran raises LIBERROR_OS, which without
/// IOSTAT is a run-time error (exit status 2).
inline bool backspace(std::FILE* f) {
  if (std::ftell(f) == 0) return true;
  bool continued = false;
  do {
    if (std::fseek(f, -4, SEEK_CUR) != 0) return false;
    unsigned char mk[4];
    if (std::fread(mk, 1, 4, f) != 4) return false;
    std::int32_t m32 = 0;
    ww::iogr::detail::get_be(mk, &m32, 4);
    long m = m32;
    continued = m < 0;
    if (continued) m = -m;
    if (std::fseek(f, -m - 8, SEEK_CUR) != 0) return false;
  } while (continued);
  return true;
}

// ---- State -------------------------------------------------------------------------------

/// The module variables W3IOBC reads or writes after W3SETO, W3SETG, W3SETW and W3SETA have
/// selected grid IGRD (w3iobcmd.F90:309-312); the caller owns one IobcState per grid. Kinds as in
/// iogr_state.hpp: REAL float, INTEGER int32_t, LOGICAL its int32_t storage. Arrays are
/// std::vector in Fortran element order; each comment gives the Fortran shape with its lower
/// bounds, element (i,j) of (l1:u1,l2:u2) at [(i-l1) + (u1-l1+1)*(j-l2)].
struct IobcState {
  // W3GDATMD (w3iobcmd.F90:219-223)
  std::int32_t nk = 0, nth = 0, nspec = 0, nsea = 0;
  std::int32_t nx = 0, ny = 0;  // extents of MAPSTA and MAPFS; W3IOBC itself does not use NX, NY
  std::int32_t gtype = 0;
  std::vector<std::int32_t> mapsta;  // MAPSTA(NY,NX)
  std::vector<std::int32_t> mapfs;   // MAPFS(NY,NX)
  std::vector<std::int32_t> mapsf;   // MAPSF(NSEA,3)
  float xfr = 0.f, fr1 = 0.f, dth = 0.f, fachfe = 0.f, dxymax = 0.f;
  std::vector<float> th;  // TH(NTH)
  FChar<13> filext;       // FILEXT, CHARACTER(LEN=13)
  // W3ODATMD (w3iobcmd.F90:236-242); NDSE and NDST are handed to the hooks only
  std::int32_t ndse = 0, ndst = 0, iaproc = 1, naperr = 1, napbpt = 1;
  FChar<80> fnmpre{std::string("./")};  // FNMPRE, w3odatmd.F90:337
  std::int32_t nbi = 0, nbi2 = 0, nfbpo = 0, nki = 0, nthi = 0;
  std::array<std::int32_t, 10> nbo{}, nbo2{};  // NBO(0:9), NBO2(0:9)
  std::array<std::int32_t, 9> ndsl{};          // NDSL(9)
  float xfri = 0.f, fr1i = 0.f, th1i = 0.f;
  std::vector<std::int32_t> ipbpi;  // IPBPI(NBI,4)
  std::vector<std::int32_t> isbpi;  // ISBPI(NBI)
  std::vector<float> xbpi, ybpi;    // XBPI(NBI), YBPI(NBI)
  std::vector<float> rdbpi;         // RDBPI(NBI,4)
  std::vector<std::int32_t> ipbpo;  // IPBPO(NBO(NFBPO),4)
  std::vector<float> xbpo, ybpo;    // XBPO(NBO(NFBPO)), YBPO(NBO(NFBPO))
  std::vector<float> rdbpo;         // RDBPO(NBO(NFBPO),4)
  std::vector<float> abpi0, abpin;  // ABPI0(NSPEC,0:NBI2), ABPIN(NSPEC,0:NBI2), NBI2 of W3DMO5(3)
  FLogical flbpi = 0, filer = 1, filew = 1, filed = 1, spconv = 0;  // w3odatmd.F90:734-736
};

/// The shapes W3DMO5 gives (w3odatmd.F90:1446-1499): iblock 1 IPBPI, ISBPI, XBPI, YBPI, RDBPI
/// with NBI; iblock 3 ABPI0, ABPIN with NSPEC and NBI2. Zero filled, where the Fortran leaves
/// them undefined. BBPI0 and BBPIN of iblock 3 are not W3IOBC's state; nothing else of W3DMO5.
inline void iobc_shape_w3dmo5(IobcState& s, int iblock) {
  const auto nbi = static_cast<std::size_t>(s.nbi > 0 ? s.nbi : 0);
  if (iblock == 1) {
    s.ipbpi.assign(4 * nbi, 0);
    s.isbpi.assign(nbi, 0);
    s.xbpi.assign(nbi, 0.f);
    s.ybpi.assign(nbi, 0.f);
    s.rdbpi.assign(4 * nbi, 0.f);
  } else if (iblock == 3) {
    const auto n = static_cast<std::size_t>(s.nspec > 0 ? s.nspec : 0) *
                   static_cast<std::size_t>(s.nbi2 + 1 > 0 ? s.nbi2 + 1 : 0);
    s.abpi0.assign(n, 0.f);
    s.abpin.assign(n, 0.f);
  }
}

// ---- Status and hooks --------------------------------------------------------------------

/// What W3IOBC ends with. exit_code 0: it returned (IOTST says how). Otherwise the code EXTCDE
/// would exit with. The messages are the ones the Fortran writes to NDSE when IAPROC equals
/// NAPERR (FORMATs at w3iobcmd.F90:785-811); fmt and the lists stay empty when it does not.
/// The caller prints them; a kernel never does.
struct IobcStatus {
  struct Inactive {  // FORMAT 909, :527
    std::int32_t ix, iy, mapsta;
  };
  int exit_code = 0;
  int fmt = 0;         // 900, 901, 902, 910, 1010; 0 for EXTOPN, EXTIOF and EXTCDE(20)
  int iostat = 0;      // IERR handed to EXTOPN or EXTIOF
  std::string found, expected;  // FORMATs 900 (INXOUT), 901, 902
  std::string filen;            // FILEN, the NAMEF of EXTOPN (trimmed by it)
  std::string filename;         // FNMPRE(:J)//FILEN(:5+I) or (:6+I) of the last OPEN
  std::int32_t point = 0;       // FORMAT 910: I, XBPI(I), YBPI(I)
  float xbpi = 0.f, ybpi = 0.f;
  bool warn1001 = false;              // FORMAT 1001, :337, with IMOD
  std::vector<Inactive> fmt909;       // one per boundary point on an inactive cell, :525-529
  std::vector<std::array<std::int32_t, 2>> fmt911;  // IX, IY of points not updated, :549-560
};

/// The calls of W3IOBC this file does not port, in its order. A path that reaches an empty
/// hook ends with kExitMissingHook rather than skip the call. Each returns 0 or the EXTCDE code
/// it ends with, except w3gfpt.
struct IobcHooks {
  /// W3DMO5(IGRD, NDSE, NDST, iblock): iblock 1 at :463 (NBI), 3 at :590 (NBI2).
  std::function<int(IobcState&, int igrd, int iblock)> w3dmo5;
  /// W3NESTUG(DXYMAX, FLOK), :511, GTYPE = UNGTYPE: sets ISBPI and may set FLOK.
  std::function<int(IobcState&, float distmin, FLogical& flok)> w3nestug;
  /// W3GFPT(GSU, XBPI(I), YBPI(I), IX, IY, DCIN=0.1), :524, through W3GFPT_R4
  /// (w3gsrumd.F90:2038-2074); returns INGRID. GSU is the caller's.
  std::function<FLogical(const IobcState&, float xt, float yt, std::int32_t& ix,
                         std::int32_t& iy, float dcin)>
      w3gfpt;
  /// W3CSPC(TMPSPC, NKI, NTHI, XFRI, FR1I, TH1I, ABPIN(:,1:NBI2), NK, NTH, XFR, FR1, TH(1),
  /// NBI2, NDST, NDSE, FACHFE), :738-740. sp2 is ABPIN's column 1, NSPEC*NBI2 contiguous values;
  /// the Fortran packs and unpacks that section around the call, so it enters with ABPIN's values.
  std::function<int(const float* sp1, int nfr1, int nth1, float xf1, float fr1, float th1,
                    float* sp2, int nfr2, int nth2, float xf2, float fr2, float th2, int nsp,
                    int ndst, int ndse, float fachfe)>
      w3cspc;
};

/// Hooks with W3DMO5 replaced by the shape helper. W3NESTUG, W3GFPT and W3CSPC are the caller's.
inline IobcHooks iobc_shape_hooks() {
  IobcHooks h;
  h.w3dmo5 = [](IobcState& s, int, int iblock) {
    iobc_shape_w3dmo5(s, iblock);
    return 0;
  };
  return h;
}

// ---- W3IOBC ------------------------------------------------------------------------------

/// W3IOBC(INXOUT, NDSB, TIME1, TIME2, IOTST, IMOD), w3iobcmd.F90:100-852, host driver.
/// inxout is 'READ', 'WRITE' or 'DUMP'; ndsb the unit number; s the state of grid IGRD = imod
/// (1 when absent, :303-307), already selected as W3SETO..W3SETA select it; units keeps the
/// connected files between calls; v the Views of the gather, used on 'WRITE' only. time1 and
/// time2 may be the same array, as W3WAVE (w3wavemd.F90:2715) and WMINIO (wminiomd.F90:1121) pass
/// them for 'WRITE' and 'DUMP', which never write TIME2.
/// IOTST: 1 file not found, 0 fields read, -1 past the end of the file (:159-162).
template <class ExecSpace>
IobcStatus w3iobc(const ExecSpace& space, const std::string& inxout, int ndsb,
                  std::array<std::int32_t, 2>& time1, std::array<std::int32_t, 2>& time2,
                  int& iotst, IobcState& s, IobcUnits& units, const IobcViews<ExecSpace>& v,
                  const IobcHooks& hooks, std::optional<int> imod = std::nullopt) {
  using ww::iogr::RecordReader;
  using ww::iogr::RecordWriter;
  using ww::iogr::detail::fstr_eq;
  IobcStatus st;
  const bool msg = s.iaproc == s.naperr;

  iotst = 0;                              // :299
  const int igrd = imod ? *imod : 1;      // :303-307

  if (!fstr_eq(inxout, "READ") && !fstr_eq(inxout, "WRITE") &&
      !fstr_eq(inxout, "DUMP")) {  // :314-318
    if (msg) {
      st.fmt = 900;
      st.found = inxout;
    }
    st.exit_code = 1;
    return st;
  }
  const bool rd = fstr_eq(inxout, "READ");
  const bool wr = fstr_eq(inxout, "WRITE");
  const bool dp = fstr_eq(inxout, "DUMP");
  if (s.nfbpo > 9) {  // NBO(0:9), NDSL(9): W3IOGR stops at NFBPO > 9 (w3iogrmd.F90:682-686)
    st.exit_code = ww::iogr::kExitNfbpoBounds;
    return st;
  }

  const std::size_t ilen = s.filext.len_trim();  // I = LEN_TRIM(FILEXT), :326
  const std::string pre = s.fnmpre.trimmed();    // FNMPRE(:J), J = LEN_TRIM(FNMPRE), :327
  const std::string ext = s.filext.trimmed();
  const auto nspec = static_cast<std::size_t>(s.nspec > 0 ? s.nspec : 0);

  // Transfers. A WRITE has no IOSTAT: a failure is a gfortran run-time error, exit status 2.
  auto unit_of = [&](int u, IobcUnits::Unit*& p) {
    p = units.get(u);
    if (p == nullptr) st.exit_code = kExitUnitNotConnected;
    return p != nullptr;
  };
  auto write_rec = [&](int u, const auto& fill) {
    IobcUnits::Unit* p = nullptr;
    if (!unit_of(u, p)) return false;
    if (!p->can_write) {
      st.exit_code = ww::iogr::kExitGfortranRuntime;
      return false;
    }
    RecordWriter w(p->f.get());
    fill(w);
    if (!w.end()) {
      st.exit_code = ww::iogr::kExitGfortranRuntime;
      return false;
    }
    return true;
  };
  // READ (u, IOSTAT=IERR) list: sets IERR; false, with exit_code set, for an unconnected unit.
  // A READ that reached the end of the file leaves the unit AFTER_ENDFILE, and the next READ
  // fails with IOSTAT 5001 (a standalone gfortran 16.2 program, 2026-10-16).
  auto read_rec = [&](int u, const auto& fill, int& ierr) {
    IobcUnits::Unit* p = nullptr;
    if (!unit_of(u, p)) return false;
    if (!p->can_read) {
      ierr = ww::iogr::kIostatBadAction;
      return true;
    }
    if (p->after_endfile) {
      ierr = kIostatOptionConflict;
      return true;
    }
    RecordReader r(p->f.get());
    r.begin();
    fill(r);
    r.finish();
    ierr = r.iostat();
    if (ierr == ww::iogr::kIostatEnd) p->after_endfile = true;
    return true;
  };
  // READ (NDSB,IOSTAT=IERR) TIME2, NBI2 (:564, :624): two list items, TIME2 one
  // _gfortran_transfer_array of 8 bytes, NBI2 one _gfortran_transfer_integer. libgfortran's
  // unformatted_read (io/transfer.c:1096-1231) reads an item's bytes straight into it and then
  // byte-swaps the whole item, error or not; an item after the failing one is not touched. So a
  // record cut inside TIME2 leaves TIME2(2) the byte swap of its old value with the bytes read
  // over its first bytes (a standalone gfortran 16.2 program, 2026-10-16).
  auto time_record = [&](RecordReader& r) {
    auto item = [&r](std::int32_t* p, std::size_t n) {
      if (r.iostat() != 0) return;
      r.chars(reinterpret_cast<char*>(p), 4 * n);
      for (std::size_t k = 0; k < n; ++k) {
        unsigned char b[4];
        std::memcpy(b, p + k, 4);
        ww::iogr::detail::get_be(b, p + k, 4);
      }
    };
    item(time2.data(), 2);
    item(&s.nbi2, 1);
  };
  // EXTIOF(NDSE, IERR, 'W3IOBC', '', 41) under IF (IERR.GT.0): EXTCDE(42), w3servmd.F90:910-923.
  auto extiof41 = [&](int ierr) {
    st.iostat = ierr;
    st.exit_code = 42;
  };
  auto missing = [&](const auto& h) {
    if (!h) st.exit_code = ww::iogr::kExitMissingHook;
    return !h;
  };
  auto spec_fits = [&](std::int32_t ncol) {  // ABPI0/ABPIN(NSPEC,0:ncol) within W3DMO5's extent
    const auto need = nspec * static_cast<std::size_t>(ncol + 1 > 0 ? ncol + 1 : 0);
    if (s.abpi0.size() < need || s.abpin.size() < need) {
      st.exit_code = ww::iogr::kExitShape;
      return false;
    }
    return true;
  };
  auto col = [&](std::vector<float>& a, std::int32_t ip) {  // &A(1,IP) of A(NSPEC,0:NBI2)
    return a.data() + nspec * static_cast<std::size_t>(ip);
  };

  // ---- open file (:329-366) --------------------------------------------------------------
  if (rd && s.filer != 0) {
    const std::string filen = "nest." + ext;  // WRITE (FILEN,'(A5,A)'), 5+I <= 18 always
    st.filename = pre + filen;
    if (units.open(ndsb, st.filename, true) != 0) {  // :334-341
      if (msg) {
        if (!imod) {  // the GIMPLE transfers *imod with no PRESENT test
          st.exit_code = kExitNullImod;
          return st;
        }
        st.warn1001 = true;
      }
      iotst = 1;
      s.flbpi = 0;
      return st;
    }
  }
  if (wr && s.filew != 0) {
    for (int ifile = 1; ifile <= s.nfbpo; ++ifile) {  // :344-356
      s.ndsl[static_cast<std::size_t>(ifile - 1)] = static_cast<std::int32_t>(
          static_cast<std::uint32_t>(ndsb) + static_cast<std::uint32_t>(ifile) - 1u);
      // WRITE (FILEN,'(A4,I1,A1,A)'): 6+I characters into CHARACTER(LEN=18). With I = 13 the
      // internal record overflows, a run-time error with exit status 2 (a standalone gfortran
      // 16.2 program, 2026-10-16).
      if (6 + ilen > 18) {
        st.exit_code = ww::iogr::kExitGfortranRuntime;
        return st;
      }
      const std::string filen = "nest" + std::to_string(ifile) + "." + ext;
      st.filename = pre + filen;
      const int ierr = units.open(s.ndsl[static_cast<std::size_t>(ifile - 1)], st.filename, false);
      if (ierr != 0) {  // EXTOPN(NDSE, IERR, 'W3IOBC', '', 40, NAMEF=FILEN), w3servmd.F90:1042
        st.iostat = ierr;
        st.filen = filen;
        st.exit_code = 40;
        return st;
      }
    }
  }
  if (dp && s.filed != 0) {  // :358-366
    const std::string filen = "nest." + ext;
    st.filename = pre + filen;
    const int ierr = units.open(ndsb, st.filename, false);
    if (ierr != 0) {
      st.iostat = ierr;
      st.filen = filen;
      st.exit_code = 40;
      return st;
    }
  }

  // ---- test info, new files only: writing (:372-406), dumping (:410-430) ----------------
  if (wr && s.filew != 0 && s.iaproc == s.napbpt) {
    const auto nbot = static_cast<std::size_t>(  // NBO(NFBPO), the extent W3DMO5(2) gave
        s.nfbpo > 0 ? s.nbo[static_cast<std::size_t>(s.nfbpo)] : 0);
    if (s.th.empty() || s.xbpo.size() < nbot || s.ybpo.size() < nbot ||
        s.ipbpo.size() < 4 * nbot || s.rdbpo.size() < 4 * nbot) {
      st.exit_code = ww::iogr::kExitShape;
      return st;
    }
    for (int ifile = 1; ifile <= s.nfbpo; ++ifile) {
      const auto f = static_cast<std::size_t>(ifile);
      const std::int32_t lo = s.nbo[f - 1] + 1, hi = s.nbo[f];
      const int u = s.ndsl[f - 1];
      if (!write_rec(u, [&](RecordWriter& w) {  // :375-377
            w.chars(idstrbc().c.data(), 32);
            w.chars(verbptbc().c.data(), 10);
            w.scalar(s.nk);
            w.scalar(s.nth);
            w.scalar(s.xfr);
            w.scalar(s.fr1);
            w.scalar(s.th[0]);
            const std::int32_t n = s.nbo[f] - s.nbo[f - 1];
            w.scalar(n);
          }))
        return st;
      if (!write_rec(u, [&](RecordWriter& w) {  // :389-393
            for (std::int32_t i = lo; i <= hi; ++i)
              w.scalar(s.xbpo[static_cast<std::size_t>(i - 1)]);
            for (std::int32_t i = lo; i <= hi; ++i)
              w.scalar(s.ybpo[static_cast<std::size_t>(i - 1)]);
            for (std::size_t j = 0; j < 4; ++j)
              for (std::int32_t i = lo; i <= hi; ++i)
                w.scalar(s.ipbpo[static_cast<std::size_t>(i - 1) + nbot * j]);
            for (std::size_t j = 0; j < 4; ++j)
              for (std::int32_t i = lo; i <= hi; ++i)
                w.scalar(s.rdbpo[static_cast<std::size_t>(i - 1) + nbot * j]);
          }))
        return st;
    }
  }
  if (dp && s.filed != 0 && s.iaproc == s.napbpt) {
    const auto nbi = static_cast<std::size_t>(s.nbi > 0 ? s.nbi : 0);
    if (s.th.empty() || s.xbpi.size() < nbi || s.ybpi.size() < nbi || s.ipbpi.size() < 4 * nbi ||
        s.rdbpi.size() < 4 * nbi) {
      st.exit_code = ww::iogr::kExitShape;
      return st;
    }
    if (!write_rec(ndsb, [&](RecordWriter& w) {  // :412
          w.chars(idstrbc().c.data(), 32);
          w.chars(verbptbc().c.data(), 10);
          w.scalar(s.nk);
          w.scalar(s.nth);
          w.scalar(s.xfr);
          w.scalar(s.fr1);
          w.scalar(s.th[0]);
          w.scalar(s.nbi);
        }))
      return st;
    if (!write_rec(ndsb, [&](RecordWriter& w) {  // :418-420, (NBI,4) arrays in storage order
          w.array(s.xbpi.data(), nbi);
          w.array(s.ybpi.data(), nbi);
          w.array(s.ipbpi.data(), 4 * nbi);
          w.array(s.rdbpi.data(), 4 * nbi);
        }))
      return st;
  }

  // ---- reading the header, the points and the first time (:434-592) --------------------
  if (rd && s.filer != 0) {
    FChar<32> idtst;  // uninitialised in the Fortran; read on IERR = 0 (an IERR < 0 goes on)
    FChar<10> vertst;
    int ierr = 0;
    if (!read_rec(ndsb, [&](RecordReader& r) {  // :437-438
          r.chars(idtst.c.data(), 32);
          r.chars(vertst.c.data(), 10);
          r.scalar(s.nki);
          r.scalar(s.nthi);
          r.scalar(s.xfri);
          r.scalar(s.fr1i);
          r.scalar(s.th1i);
          r.scalar(s.nbi);
        }, ierr))
      return st;
    if (ierr > 0) return extiof41(ierr), st;  // :439; IERR < 0 is not tested
    if (!(idtst == idstrbc())) {  // :445-449
      if (msg) {
        st.fmt = 901;
        st.found = std::string(idtst.c.data(), 32);
        st.expected = std::string(idstrbc().c.data(), 32);
      }
      st.exit_code = 10;
      return st;
    }
    if (!(vertst == verbptbc())) {  // :450-454
      if (msg) {
        st.fmt = 902;
        st.found = std::string(vertst.c.data(), 10);
        st.expected = std::string(verbptbc().c.data(), 10);
      }
      st.exit_code = 11;
      return st;
    }
    if (s.th.empty()) {
      st.exit_code = ww::iogr::kExitShape;
      return st;
    }
    // :458-461, short-circuit in this order (the GIMPLE's branches)
    s.spconv = (s.nki != s.nk || s.nthi != s.nth || std::fabs(s.xfri / s.xfr - 1.0f) > 0.01f ||
                std::fabs(s.fr1i / s.fr1 - 1.0f) > 0.01f ||
                std::fabs(s.th1i - s.th[0]) > s.dth * 0.01f)
                   ? 1
                   : 0;
    if (missing(hooks.w3dmo5)) return st;
    if (const int c = hooks.w3dmo5(s, igrd, 1); c != 0) {  // :463
      st.exit_code = c;
      return st;
    }
    const auto nbi = static_cast<std::size_t>(s.nbi > 0 ? s.nbi : 0);
    if (s.xbpi.size() < nbi || s.ybpi.size() < nbi || s.ipbpi.size() < 4 * nbi ||
        s.rdbpi.size() < 4 * nbi || s.isbpi.size() < nbi) {
      st.exit_code = ww::iogr::kExitShape;
      return st;
    }
    if (!read_rec(ndsb, [&](RecordReader& r) {  // :465-468
          r.array(s.xbpi.data(), nbi);
          r.array(s.ybpi.data(), nbi);
          r.array(s.ipbpi.data(), 4 * nbi);
          r.array(s.rdbpi.data(), 4 * nbi);
        }, ierr))
      return st;
    if (ierr > 0) return extiof41(ierr), st;  // :469

    FLogical flok = 1;  // :509
    if (s.gtype == ww::iogr::kUngType) {
      if (missing(hooks.w3nestug)) return st;
      if (const int c = hooks.w3nestug(s, s.dxymax, flok); c != 0) {  // :511
        st.exit_code = c;
        return st;
      }
    } else {  // RLGTYPE, CLGTYPE and, without W3_SMC, SMCTYPE (:519-537)
      if (s.nbi > 0 && missing(hooks.w3gfpt)) return st;
      const auto ny = static_cast<std::size_t>(s.ny);
      for (std::int32_t i = 1; i <= s.nbi; ++i) {
        const auto ii = static_cast<std::size_t>(i - 1);
        std::int32_t ix = 0, iy = 0;
        const FLogical ingrid = hooks.w3gfpt(s, s.xbpi[ii], s.ybpi[ii], ix, iy, 0.1f);
        if (ingrid != 0) {
          if (ix < 1 || ix > s.nx || iy < 1 || iy > s.ny ||
              s.mapsta.size() < ny * static_cast<std::size_t>(s.nx) ||
              s.mapfs.size() < ny * static_cast<std::size_t>(s.nx)) {
            st.exit_code = ww::iogr::kExitShape;
            return st;
          }
          const std::size_t k =
              static_cast<std::size_t>(iy - 1) + ny * static_cast<std::size_t>(ix - 1);
          const std::int32_t m = s.mapsta[k] < 0 ? -s.mapsta[k] : s.mapsta[k];
          if (m != 2) {  // :525-529
            if (msg) st.fmt909.push_back({ix, iy, m});
            flok = 0;
          }
          s.isbpi[ii] = s.mapfs[k];  // :535
        } else {  // :531-534
          if (msg) {
            st.fmt = 910;
            st.point = i;
            st.xbpi = s.xbpi[ii];
            st.ybpi = s.ybpi[ii];
          }
          st.exit_code = 12;
          return st;
        }
      }
    }
    if (flok == 0) {  // :547
      st.exit_code = 20;
      return st;
    }
    // :549-560: a warning for each boundary cell no input point maps to. FLOK is not read after
    // the loop, so without the message (IAPROC /= NAPERR) the loop has no effect.
    if (msg) {
      const auto ns = static_cast<std::size_t>(s.nsea > 0 ? s.nsea : 0);
      const auto ny = static_cast<std::size_t>(s.ny);
      if (s.mapsf.size() < 2 * ns || s.mapsta.size() < ny * static_cast<std::size_t>(s.nx)) {
        st.exit_code = ww::iogr::kExitShape;
        return st;
      }
      for (std::size_t is = 0; is < ns; ++is) {
        const std::int32_t ix = s.mapsf[is], iy = s.mapsf[is + ns];
        if (ix < 1 || ix > s.nx || iy < 1 || iy > s.ny) {
          st.exit_code = ww::iogr::kExitShape;
          return st;
        }
        const std::int32_t m0 =
            s.mapsta[static_cast<std::size_t>(iy - 1) + ny * static_cast<std::size_t>(ix - 1)];
        if ((m0 < 0 ? -m0 : m0) == 2) {
          bool found = false;
          for (std::size_t i = 0; i < nbi; ++i)
            if (static_cast<std::int32_t>(is + 1) == s.isbpi[i]) found = true;
          if (!found) st.fmt911.push_back({ix, iy});
        }
      }
    }
    // :564-590: read the first time, back up over it, allocate ABPI0/N with its NBI2
    if (!read_rec(ndsb, time_record, ierr)) return st;
    if (ierr != 0) {  // FILER is set on this path: FORMAT 1010 and EXTCDE(43), :565-569
      if (msg) st.fmt = 1010;
      st.exit_code = 43;
      return st;
    }
    // :586. The READ above succeeded, so the unit is not AFTER_ENDFILE (st_backspace would
    // only reset that, io/file_pos.c:246-251).
    if (!ww::iobc::backspace(units.get(ndsb)->f.get())) {
      st.exit_code = ww::iogr::kExitGfortranRuntime;
      return st;
    }
    if (const int c = hooks.w3dmo5(s, igrd, 3); c != 0) {  // :590
      st.exit_code = c;
      return st;
    }
  }

  // ---- save previous spectra on read (:596-602) ------------------------------------------
  if (rd && s.filer == 0) {
    time1 = time2;
    if (!spec_fits(s.nbi2)) return st;
    for (std::int32_t ip = 1; ip <= s.nbi2; ++ip)
      for (std::size_t isp = 0; isp < nspec; ++isp) col(s.abpi0, ip)[isp] = col(s.abpin, ip)[isp];
  }

  // ---- time (:604-649) -------------------------------------------------------------------
  if (wr) {
    for (int ifile = 1; ifile <= s.nfbpo; ++ifile) {  // :607-609
      const auto f = static_cast<std::size_t>(ifile);
      const std::int32_t npts = s.nbo2[f] - s.nbo2[f - 1];
      if (!write_rec(s.ndsl[f - 1], [&](RecordWriter& w) {
            w.array(time1.data(), 2);
            w.scalar(npts);
          }))
        return st;
    }
  }
  if (dp) {  // :616-621
    if (!write_rec(ndsb, [&](RecordWriter& w) {
          w.array(time1.data(), 2);
          w.scalar(s.nbi2);
        }))
      return st;
  }
  if (rd) {  // :623-649
    int ierr = 0;
    if (!read_rec(ndsb, time_record, ierr)) return st;
    if (ierr != 0) {
      if (s.filer != 0) {  // :626-629
        if (msg) st.fmt = 1010;
        st.exit_code = 43;
        return st;
      }
      time1[0] = time2[0];  // :634-640, ABPI0(:,0:NBI2) = ABPIN
      time1[1] = time2[1];
      if (!spec_fits(s.nbi2)) return st;
      for (std::int32_t ip = 0; ip <= s.nbi2; ++ip)
        for (std::size_t isp = 0; isp < nspec; ++isp) col(s.abpi0, ip)[isp] = col(s.abpin, ip)[isp];
      iotst = -1;  // :642-644
      s.flbpi = 0;
      return st;
    }
  }

  // ---- spectra (:651-760) ----------------------------------------------------------------
  if (wr) {
    const std::int32_t ntot = s.nfbpo > 0 ? s.nbo2[static_cast<std::size_t>(s.nfbpo)] : 0;
    if (s.nth <= 0 || v.va.extent(0) < nspec || v.sig2.extent(0) < nspec ||
        v.abpos.extent(0) < nspec ||
        v.abpos.extent(1) < static_cast<std::size_t>(ntot > 0 ? ntot + 1 : 1) ||
        v.isbpo.extent(0) < static_cast<std::size_t>(ntot > 0 ? ntot : 0) ||
        (nspec > 0 && v.cg.extent(0) < (nspec - 1) / static_cast<std::size_t>(s.nth) + 2)) {
      st.exit_code = ww::iogr::kExitShape;
      return st;
    }
    for (int ifile = 1; ifile <= s.nfbpo; ++ifile) {  // :659-711
      const auto f = static_cast<std::size_t>(ifile);
      const std::int32_t lo = s.nbo2[f - 1] + 1, hi = s.nbo2[f];
      iobc_gather_shrd(space, v, lo, hi, s.nspec, s.nth);  // :666-671, W3_SHRD
      const auto h = Kokkos::create_mirror_view(v.abpos);
      Kokkos::deep_copy(space, h, v.abpos);
      space.fence("ww::iobc::w3iobc abpos");
      for (std::int32_t isout = lo; isout <= hi; ++isout) {  // :695
        if (!write_rec(s.ndsl[f - 1], [&](RecordWriter& w) {
              for (std::size_t is = 0; is < nspec; ++is) w.scalar(h(is, isout));
            }))
          return st;
      }
    }
  }
  if (dp) {  // :714-718
    if (!spec_fits(s.nbi2)) return st;
    for (std::int32_t i = 1; i <= s.nbi2; ++i)
      if (!write_rec(ndsb, [&](RecordWriter& w) { w.array(col(s.abpin, i), nspec); })) return st;
  }
  if (rd) {  // :720-742
    if (!spec_fits(s.nbi2)) return st;
    int ierr = 0;
    if (s.spconv == 0) {
      for (std::int32_t ip = 1; ip <= s.nbi2; ++ip) {  // :723-726
        if (!read_rec(ndsb, [&](RecordReader& r) { r.array(col(s.abpin, ip), nspec); }, ierr))
          return st;
        if (ierr > 0) return extiof41(ierr), st;
      }
    } else {  // :733-741: ALLOCATE TMPSPC(NKI*NTHI,NBI2), read, W3CSPC, DEALLOCATE
      const auto ni = static_cast<std::size_t>(s.nki * s.nthi > 0 ? s.nki * s.nthi : 0);
      const auto nc = static_cast<std::size_t>(s.nbi2 > 0 ? s.nbi2 : 0);
      std::vector<float> tmpspc(ni * nc, 0.f);  // undefined in the Fortran until read
      for (std::size_t ip = 0; ip < nc; ++ip) {
        if (!read_rec(ndsb, [&](RecordReader& r) { r.array(tmpspc.data() + ni * ip, ni); }, ierr))
          return st;
        if (ierr > 0) return extiof41(ierr), st;
      }
      if (missing(hooks.w3cspc)) return st;
      if (const int c = hooks.w3cspc(tmpspc.data(), s.nki, s.nthi, s.xfri, s.fr1i, s.th1i,
                                     col(s.abpin, 1), s.nk, s.nth, s.xfr, s.fr1, s.th[0], s.nbi2,
                                     s.ndst, s.ndse, s.fachfe);
          c != 0) {
        st.exit_code = c;
        return st;
      }
    }
  }

  // ---- set first spectra on first read (:763-773) ----------------------------------------
  if (rd && s.filer != 0) {
    time1 = time2;
    for (std::int32_t ip = 1; ip <= s.nbi2; ++ip)
      for (std::size_t isp = 0; isp < nspec; ++isp) col(s.abpi0, ip)[isp] = col(s.abpin, ip)[isp];
    for (std::size_t isp = 0; isp < nspec; ++isp) {
      col(s.abpi0, 0)[isp] = 0.f;
      col(s.abpin, 0)[isp] = 0.f;
    }
  }

  // ---- reset flags (:777-779) ------------------------------------------------------------
  if (wr) s.filew = 0;
  if (dp) s.filed = 0;
  if (rd) s.filer = 0;
  return st;
}

}  // namespace ww::iobc
