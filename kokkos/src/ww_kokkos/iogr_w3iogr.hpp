// kokkos/src/ww_kokkos/iogr_w3iogr.hpp
// W3IOGR, ported: writes and reads the model definition file mod_def.<ext> record by record,
// rebuilds the tables W3IOGR derives from what it reads (the rectilinear XGRD/YGRD, MAPSTA and
// MAPST2 from the packed map, MAPSF(:,3)) and calls DISTAB once per process. Phase 1,
// translated, not wired into WW3, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3iogrmd.F90:116-2236 at NOAA-EMC/WW3 03adda91 (identical in
// the h0ffmann/WW3 fork at the WW3/ pin), authors H. L. Tolman and F. Ardhuin. Only the paths
// switches/switch_lab_shrd compiles are ported: LN1, ST4, NL1, BT1, DB1, TR0, BS0, IC0, FLX0,
// UQ, PR3, RWND, SHRD (no MPI). The other source-term, propagation and output switches, W3_T
// test prints, W3_ASCII, W3_SMC, W3_TRNK and W3_S STRACE are not compiled under it and are not
// ported; FLTEST is a .FALSE. local never set (:349), so its test writes are dead.
//
// Arithmetic, checked against the -O3 GIMPLE of w3iogrmd.F90 (gfortran 16.2, WW3's Release
// flags, switch_lab_shrd; .text and .rodata byte-identical to the CMake object):
// - XGRD(:,IX) = REAL(X0 + REAL(IX-1)*SX) (:987) is (double)(X0 + (float)(IX-1)*SX): one
//   cvtsi2ss, one float product, one float sum, one widening; the same for YGRD (:990).
//   rect_coord() is that statement; contraction is off in the ww_kokkos target.
// - WRITE of REAL(XGRD) (:791) narrows each double to float; READ of XGRD4 (:996) widens back.
// - MAPSTA = MOD(MAPTMP+2,8)-2, MAPST2 = (MAPTMP-MAPSTA)/8 (:1071-1072) and MAPTMP =
//   MAPSTA+8*MAPST2 (:768) are integer % and / with C's truncation, as Fortran's MOD and /.
// - HUGE(X0) is FLT_MAX, 3.4028234663852885981170418348451692544e+38 (:1000-1001).
// - SINTAILPAR(1).GT.0.5 and TTAUWSHELTER.GT.0 are float compares with 0.5f and 0.0f.
// No other floating-point operation is done by W3IOGR itself; the tables of the frequency and
// direction grid (SIG, DSIP, TH, ESIN, ...) are not computed here but transferred as written
// by ww3_grid.
#pragma once

#include <cerrno>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <Kokkos_Core.hpp>

#include "disp_distab.hpp"
#include "iogr_state.hpp"
#include "iogr_unformatted.hpp"

namespace ww::iogr {

// ---- Element functions of the tables W3IOGR derives (host and device) -------------------

/// XGRD(IY,IX) = REAL(X0 + REAL(IX-1)*SX), w3iogrmd.F90:987 (and YGRD with IY, :990); i is
/// the Fortran index, 1-based.
KOKKOS_INLINE_FUNCTION double rect_coord(float origin, int i, float step) {
  const float fi = static_cast<float>(i - 1);
  const float p = fi * step;
  const float s = origin + p;
  return static_cast<double>(s);
}

/// MAPSTA = MOD(MAPTMP+2,8) - 2 and MAPST2 = (MAPTMP-MAPSTA)/8, w3iogrmd.F90:1071-1072.
KOKKOS_INLINE_FUNCTION void decode_maptmp(std::int32_t maptmp, std::int32_t& mapsta,
                                          std::int32_t& mapst2) {
  mapsta = (maptmp + 2) % 8 - 2;
  mapst2 = (maptmp - mapsta) / 8;
}

/// MAPTMP = MAPSTA + 8*MAPST2, w3iogrmd.F90:768.
KOKKOS_INLINE_FUNCTION std::int32_t encode_maptmp(std::int32_t mapsta, std::int32_t mapst2) {
  return mapsta + 8 * mapst2;
}

/// MAPSF(ISEA,3) = MAPSF(ISEA,2) + (MAPSF(ISEA,1)-1)*NY, w3iogrmd.F90:1073.
KOKKOS_INLINE_FUNCTION std::int32_t mapsf3(std::int32_t ix, std::int32_t iy, std::int32_t ny) {
  return iy + (ix - 1) * ny;
}

// ---- Launchers for device copies of those tables ----------------------------------------
// Each element depends only on its own inputs, with no reduction, so any execution space and
// any order give the bits of the Fortran's array assignments. Arrays are LayoutLeft, extents in
// Fortran order; index (i-1) for Fortran index i.

/// XGRD(NY,NX), YGRD(NY,NX) of a rectilinear or SMC grid, w3iogrmd.F90:986-991.
template <class ExecSpace>
void iogr_rect_xy(const ExecSpace& space,
                  const Kokkos::View<double**, Kokkos::LayoutLeft,
                                     typename ExecSpace::memory_space>& xgrd,
                  const Kokkos::View<double**, Kokkos::LayoutLeft,
                                     typename ExecSpace::memory_space>& ygrd,
                  float x0, float y0, float sx, float sy) {
  const auto ny = static_cast<std::int64_t>(xgrd.extent(0));
  const auto nx = static_cast<std::int64_t>(xgrd.extent(1));
  Kokkos::parallel_for(
      "ww::iogr::rect_xy",
      Kokkos::MDRangePolicy<ExecSpace, Kokkos::Rank<2>>(space, {0, 0}, {ny, nx}),
      KOKKOS_LAMBDA(const std::int64_t iy, const std::int64_t ix) {
        xgrd(iy, ix) = rect_coord(x0, static_cast<int>(ix) + 1, sx);
        ygrd(iy, ix) = rect_coord(y0, static_cast<int>(iy) + 1, sy);
      });
  space.fence("ww::iogr::rect_xy");
}

/// MAPSTA and MAPST2 from MAPTMP, element by element over the NY*NX entries, :1071-1072.
template <class ExecSpace>
void iogr_decode_maptmp(
    const ExecSpace& space,
    const Kokkos::View<const std::int32_t*, typename ExecSpace::memory_space>& maptmp,
    const Kokkos::View<std::int32_t*, typename ExecSpace::memory_space>& mapsta,
    const Kokkos::View<std::int32_t*, typename ExecSpace::memory_space>& mapst2) {
  Kokkos::parallel_for(
      "ww::iogr::decode_maptmp",
      Kokkos::RangePolicy<ExecSpace>(space, 0, maptmp.extent(0)),
      KOKKOS_LAMBDA(const std::size_t i) { decode_maptmp(maptmp(i), mapsta(i), mapst2(i)); });
  space.fence("ww::iogr::decode_maptmp");
}

/// MAPTMP from MAPSTA and MAPST2, :768.
template <class ExecSpace>
void iogr_encode_maptmp(
    const ExecSpace& space,
    const Kokkos::View<const std::int32_t*, typename ExecSpace::memory_space>& mapsta,
    const Kokkos::View<const std::int32_t*, typename ExecSpace::memory_space>& mapst2,
    const Kokkos::View<std::int32_t*, typename ExecSpace::memory_space>& maptmp) {
  Kokkos::parallel_for(
      "ww::iogr::encode_maptmp",
      Kokkos::RangePolicy<ExecSpace>(space, 0, mapsta.extent(0)),
      KOKKOS_LAMBDA(const std::size_t i) { maptmp(i) = encode_maptmp(mapsta(i), mapst2(i)); });
  space.fence("ww::iogr::encode_maptmp");
}

/// MAPSF(:,3) of MAPSF(NSEA,3), :1073.
template <class ExecSpace>
void iogr_mapsf3(const ExecSpace& space,
                 const Kokkos::View<std::int32_t**, Kokkos::LayoutLeft,
                                    typename ExecSpace::memory_space>& mapsf,
                 std::int32_t ny) {
  Kokkos::parallel_for(
      "ww::iogr::mapsf3", Kokkos::RangePolicy<ExecSpace>(space, 0, mapsf.extent(0)),
      KOKKOS_LAMBDA(const std::size_t is) { mapsf(is, 2) = mapsf3(mapsf(is, 0), mapsf(is, 1), ny); });
  space.fence("ww::iogr::mapsf3");
}

// ---- Constants of the file ---------------------------------------------------------------

inline const FChar<35>& idstr() {  // IDSTR, w3iogrmd.F90:92-93
  static const FChar<35> v{std::string("WAVEWATCH III MODEL DEFINITION FILE")};
  return v;
}
inline const FChar<10>& vergrd() {  // VERGRD, w3iogrmd.F90:91
  static const FChar<10> v{std::string("2024-11-25")};
  return v;
}

/// The package names switch_lab_shrd compiles into TNAME0..TNAMEI (w3iogrmd.F90:390-520); the
/// Fortran literals are blank padded to 30 characters, as FChar pads.
struct LabNames {
  FChar<30> t0{std::string("Cavaleri and M.-R. (1982)")};     // LN1, :421
  FChar<30> t1{std::string("Ardhuin et al. (2009+)")};        // ST4, :436
  FChar<30> t2{std::string("Discrete Interaction Approx.")};  // NL1, :445
  FChar<30> t3{std::string("JONSWAP")};                       // BT1, :463
  FChar<30> t4{std::string("Battjes and Janssen (1978)")};    // DB1, :490
  FChar<30> t5{std::string("Not defined")};                   // TR0, :493
  FChar<30> t6{std::string("Not defined")};                   // BS0, :496
  FChar<30> tp{std::string("3rd order UQ scheme")};           // UQ, :505
  FChar<30> tg{std::string("Averaging operator")};            // PR3, :520
  FChar<30> tf{std::string(30, '-')};                         // FLX0 sets none, :390
  FChar<30> ti{std::string(30, '-')};                         // IC0 sets none, :400
};

// ---- Status and hooks --------------------------------------------------------------------

/// Codes outside the Fortran's EXTCDE codes, for conditions the Fortran cannot reach or where
/// it dies in the gfortran run time.
inline constexpr int kExitGfortranRuntime = 2;  // a WRITE without IOSTAT failed: exit_error(2)
inline constexpr int kExitMissingHook = -101;   // a hook the path needs is empty
inline constexpr int kExitShape = -102;         // an array does not have its Fortran extent
inline constexpr int kExitNfbpoBounds = -103;   // NFBPO < 0 would index NBO(NFBPO) out of bounds

/// What W3IOGR ends with. exit_code 0: it returned. Otherwise the code EXTCDE would exit with,
/// and the FORMAT label and values of the message the Fortran writes to NDSE when IAPROC equals
/// NAPERR (w3iogrmd.F90:2035-2067); the caller prints it, a kernel never does.
struct IogrStatus {
  int exit_code = 0;
  int fmt = 0;              // 900, 901, 902, 904, 905, 906, 907, 908; 0 for EXTIOF/EXTOPN
  int iostat = 0;           // IERR of the OPEN or READ handed to EXTOPN or EXTIOF
  int record = 0;           // 1-based record number in file order of the failing statement
  int name_index = -1;      // the I2 of FORMAT 905 (3 for FNAMEI, as :713 writes)
  std::string found, expected;  // the strings FORMATs 901-908 print
  std::string filename;     // FNMPRE(:IPRE)//'mod_def.'//FILEXT(:IEXT)
};

/// A routine W3IOGR calls and this file does not port. Returns 0, or the EXTCDE code it ends
/// with. igrd is W3IOGR's IGRD.
using IogrHook = std::function<int(IogrState&, int igrd)>;

/// The calls of W3IOGR, in its order. A path that reaches an empty hook ends with
/// kExitMissingHook rather than skip the call.
struct IogrHooks {
  IogrHook w3dimx;        // :972, READ, when .NOT.GINIT
  IogrHook w3dimug;       // :1025, READ, UNGTYPE, when .NOT.GUGINIT
  IogrHook w3gntx;        // :1039, READ, GTYPE /= UNGTYPE: grid metrics, not ported here
  IogrHook w3dims;        // :1168, READ, when .NOT.SINIT
  IogrHook w3dmo5_out;    // :1217, READ, W3DMO5(IGRD,NDSE,NDST,2)
  IogrHook insin4_write;  // :1540, WRITE, INSIN4(.TRUE.): fills the ST4 tables written next
  IogrHook insin4_read;   // :1605, READ, INSIN4(.FALSE.) when SINTAILPAR(1) > 0.5
  IogrHook insnl1;        // :1802, READ, IQTPE > 0
  IogrHook insnlgqm;      // :1804, READ, IQTPE <= 0
  std::function<int(IogrSaved&)> distab;  // :2019, READ, once per process (FLDISP)
};

/// DISTAB (disp_distab.hpp) on the default host space into the process-wide s.ewn1, s.ecg1,
/// s.n1max, s.dsie. Kokkos must be initialised; not inside an OpenMP region (ww3-port skill, §3).
inline int iogr_distab_host(IogrSaved& s) {
  using H = Kokkos::DefaultHostExecutionSpace;
  Kokkos::View<ww::Real*, H::memory_space> e("ww::iogr::ewn1", kNar1d + 1);
  Kokkos::View<ww::Real*, H::memory_space> c("ww::iogr::ecg1", kNar1d + 1);
  const auto sc = ww::disp::distab(H(), e, c);
  for (int i = 0; i <= kNar1d; ++i) {
    s.ewn1[static_cast<std::size_t>(i)] = e(i);
    s.ecg1[static_cast<std::size_t>(i)] = c(i);
  }
  s.n1max = sc.n1max;
  s.dsie = sc.dsie;
  return 0;
}

/// Hooks with the allocating routines replaced by the shape helpers of iogr_state.hpp and
/// DISTAB by its port. W3GNTX, INSIN4, INSNL1 and INSNLGQM are left empty: the caller supplies
/// them (a READ hook for INSIN4 must at least call iogr_shape_insin4).
inline IogrHooks iogr_shape_hooks() {
  IogrHooks h;
  h.w3dimx = [](IogrState& s, int) { iogr_shape_w3dimx(s); return 0; };
  h.w3dimug = [](IogrState& s, int) { iogr_shape_w3dimug(s); return 0; };
  h.w3dims = [](IogrState& s, int) { iogr_shape_w3dims(s); return 0; };
  h.w3dmo5_out = [](IogrState& s, int) { iogr_shape_w3dmo5_out(s); return 0; };
  h.distab = iogr_distab_host;
  return h;
}

// ---- Records -----------------------------------------------------------------------------
// One function per record, shared by WRITE and READ so the field order cannot drift; IO is
// WriteIO or ReadIO. Line numbers: WRITE statement / READ statement in w3iogrmd.F90.

namespace detail {

template <class Rec>
struct IoBase {
  Rec& r;
  template <class T>
  void v(T& x) { r.scalar(x); }
  template <class T>
  void a(std::vector<T>& x) { r.array(x.data(), x.size()); }
  template <class T, std::size_t N>
  void a(std::array<T, N>& x) { r.array(x.data(), N); }
  template <std::size_t N>
  void c(FChar<N>& x) { r.chars(x.c.data(), N); }
};
struct WriteIO : IoBase<RecordWriter> {
  static constexpr bool kWrite = true;
};
struct ReadIO : IoBase<RecordReader> {
  static constexpr bool kWrite = false;
};

// Record 3, :769-770 / :969.
template <class IO>
void rec_gtype(IO& io, IogrState& s) {
  io.v(s.gtype);
  io.v(s.flagll);
  io.v(s.iclose);
}
// Record 4, RLGTYPE or SMCTYPE, :782-783 / :984.
template <class IO>
void rec_rect(IO& io, IogrState& s) {
  io.v(s.sx);
  io.v(s.sy);
  io.v(s.x0);
  io.v(s.y0);
}
// Record 4, UNGTYPE, scheme flags, :798-817 / :1003-1022.
template <class IO>
void rec_ug_scheme(IO& io, IogrState& s) {
  io.v(s.fsn);
  io.v(s.fspsi);
  io.v(s.fsfct);
  io.v(s.fsnimp);
  io.v(s.fstotalimp);
  io.v(s.fstotalexp);
  io.v(s.fsbccfl);
  io.v(s.fsrefraction);
  io.v(s.fsfreqshift);
  io.v(s.fssource);
  io.v(s.do_change_wlv);
  io.v(s.solverthr_stp);
  io.v(s.crit_dep_stp);
  io.v(s.ntri);
  io.v(s.countot);
  io.v(s.countri);
  io.v(s.nnz);
  io.v(s.b_jgs_terminate_maxiter);
  io.v(s.b_jgs_terminate_difference);
  io.v(s.b_jgs_terminate_norm);
  io.v(s.b_jgs_limiter);
  io.v(s.b_jgs_block_gauss_seidel);
  io.v(s.b_jgs_use_jacobi);
  io.v(s.b_jgs_maxiter);
  io.v(s.b_jgs_pmin);
  io.v(s.b_jgs_diff_thr);
  io.v(s.b_jgs_norm_thr);
  io.v(s.b_jgs_nlevel);
  io.v(s.b_jgs_source_nonlinear);
}
// Record 5, UNGTYPE, mesh, :860-864 / :1029-1033. DXYMAX appears twice, as in the Fortran.
template <class IO>
void rec_ug_mesh(IO& io, IogrState& s) {
  io.v(s.x0);
  io.v(s.y0);
  io.v(s.sx);
  io.v(s.sy);
  io.v(s.dxymax);
  io.a(s.xgrd);
  io.a(s.ygrd);
  io.a(s.trigp);
  io.a(s.tria);
  io.a(s.len);
  io.a(s.ien);
  io.a(s.angle0);
  io.a(s.angle);
  io.a(s.si);
  io.v(s.maxx);
  io.v(s.maxy);
  io.v(s.dxymax);
  io.a(s.index_cell);
  io.a(s.ccon);
  io.a(s.countcon);
  io.a(s.ie_cell);
  io.a(s.pos_cell);
  io.a(s.iobp);
  io.a(s.iobpa);
  io.a(s.iobdp);
  io.a(s.iobpd);
  io.a(s.iaa);
  io.a(s.jaa);
  io.a(s.posi);
}
// Bottom and maps, :878-879 / :1040-1041.
template <class IO>
void rec_maps(IO& io, IogrState& s, std::vector<std::int32_t>& maptmp) {
  io.a(s.zb);
  io.a(maptmp);
  io.a(s.mapfs);
  io.a(s.mapsf);
  io.v(s.trflag);
}
// Transparencies, :913 / :1075.
template <class IO>
void rec_trans(IO& io, IogrState& s) {
  io.a(s.trnx);
  io.a(s.trny);
}
// Time steps, flags, ice, :917-924 / :1085-1092. The WRITE lists IICEHDISP, IICEDDISP and the
// READ IICEDDISP, IICEHDISP (v, the GIMPLE transfers them in those orders), so a file read back
// swaps the two values; reproduced, not corrected.
template <class IO>
void rec_times(IO& io, IogrState& s) {
  io.v(s.dtcfl);
  io.v(s.dtcfli);
  io.v(s.dtmax);
  io.v(s.dtmin);
  io.v(s.dmin);
  io.v(s.ctmax);
  io.v(s.fice0);
  io.v(s.ficen);
  io.v(s.ficel);
  io.v(s.pfmove);
  io.v(s.fldry);
  io.v(s.flcx);
  io.v(s.flcy);
  io.v(s.flcth);
  io.v(s.flck);
  io.v(s.flsou);
  io.v(s.flbpi);
  io.v(s.flbpo);
  io.a(s.clats);
  io.a(s.clatis);
  io.a(s.cthg0s);
  io.v(s.stexu);
  io.v(s.steyu);
  io.v(s.stedu);
  io.v(s.iicehmin);
  io.v(s.iicehinit);
  io.v(s.iicedisp);
  io.a(s.icescales);
  io.c(s.caltype);
  io.v(s.cmprtrck);
  io.v(s.iicehfac);
  if constexpr (IO::kWrite) {
    io.v(s.iicehdisp);
    io.v(s.iiceddisp);
  } else {
    io.v(s.iiceddisp);
    io.v(s.iicehdisp);
  }
  io.v(s.iicefdisp);
  io.v(s.btbeta);
  io.v(s.ic_numerics);
  io.v(s.aaircmin);
  io.v(s.aairgb);
}
// :943 / :1095.
template <class IO>
void rec_gridshift(IO& io, IogrState& s) { io.v(s.gridshift); }
// Spectral grid, :1154-1157 / :1169-1172.
template <class IO>
void rec_spectral(IO& io, IogrState& s) {
  io.a(s.mapwn);
  io.a(s.mapth);
  io.v(s.dth);
  io.a(s.th);
  io.a(s.esin);
  io.a(s.ecos);
  io.a(s.es2);
  io.a(s.esc);
  io.a(s.ec2);
  io.v(s.xfr);
  io.v(s.fr1);
  io.a(s.sig);
  io.a(s.sig2);
  io.a(s.dsip);
  io.a(s.dsii);
  io.a(s.dden);
  io.a(s.dden2);
  io.v(s.fte);
  io.v(s.ftf);
  io.v(s.ftwn);
  io.v(s.fttr);
  io.v(s.ftwl);
  io.v(s.facti1);
  io.v(s.facti2);
  io.v(s.fachfa);
  io.v(s.fachfe);
}
// 3-D output indices, :1188-1189 / :1196.
template <class IO>
void rec_out3d(IO& io, IogrState& s) {
  io.a(s.e3df);
  io.a(s.p2msf);
  io.a(s.us3df);
  io.a(s.usspf);
  io.a(s.ussp_wn);
}
// Output boundary points, :1209-1210 / :1218.
template <class IO>
void rec_bndout(IO& io, IogrState& s) {
  io.a(s.xbpo);
  io.a(s.ybpo);
  io.a(s.rdbpo);
  io.a(s.ipbpo);
  io.a(s.isbpo);
}
// Partitioning, :1238-1240 / :1249-1251.
template <class IO>
void rec_part(IO& io, IogrState& s) {
  io.v(s.ihmax);
  io.v(s.hspmin);
  io.v(s.wsmult);
  io.v(s.wscut);
  io.v(s.flcomb);
  io.v(s.noswll);
  io.v(s.ptmeth);
  io.v(s.ptfcut);
}
// Numerics, :1263-1265 / :1346-1348.
template <class IO>
void rec_numerics(IO& io, IogrState& s) {
  io.v(s.facp);
  io.v(s.xrel);
  io.v(s.xflt);
  io.v(s.fxfm);
  io.v(s.fxpm);
  io.v(s.xft);
  io.v(s.xfc);
  io.v(s.facsd);
  io.v(s.fhmax);
  io.v(s.ffacberg);
  io.v(s.delab);
  io.a(s.fwtable);
}
// RWND, :1274-1275 / :1351.
template <class IO>
void rec_rwind(IO& io, IogrState& s) { io.v(s.rwindc); }
// LN1, :1442 / :1448.
template <class IO>
void rec_ln1(IO& io, IogrState& s) {
  io.v(s.slnc1);
  io.v(s.fspm);
  io.v(s.fshf);
}
// ST4, :1541-1552 / :1591-1602. SSTXFTF and SSTXFTWN appear twice, as in the Fortran.
template <class IO>
void rec_st4(IO& io, IogrState& s) {
  io.v(s.zzwnd);
  io.v(s.aalpha);
  io.v(s.zz0max);
  io.v(s.bbeta);
  io.v(s.ssinthp);
  io.v(s.zzalp);
  io.v(s.ttauwshelter);
  io.v(s.sswellfpar);
  io.a(s.sswellf);
  io.v(s.ssinbr);
  io.v(s.zz0rat);
  io.a(s.ssdsc);
  io.v(s.ssdsiso);
  io.v(s.ssdsbr);
  io.v(s.ssdsbt);
  io.a(s.ssdsbm);
  io.v(s.ssdsp);
  io.v(s.ssdscos);
  io.v(s.ssdsdth);
  io.v(s.wwnmeanp);
  io.v(s.wwnmeanptail);
  io.v(s.sstxftf);
  io.v(s.sstxftftail);
  io.v(s.sstxftwn);
  io.v(s.sstxftf);
  io.v(s.sstxftwn);
  io.v(s.ssdsbrf1);
  io.v(s.ssdsbrf2);
  io.v(s.ssdsbrfdf);
  io.v(s.ssdsbck);
  io.v(s.ssdsabk);
  io.v(s.ssdspbk);
  io.v(s.ssdsbint);
  io.v(s.ffxpm);
  io.v(s.ffxfm);
  io.v(s.ffxfa);
  io.v(s.ssdshck);
  io.a(s.iktab);
  io.a(s.dcki);
  io.a(s.qbi);
  io.a(s.satindices);
  io.a(s.satweights);
  io.v(s.dikcumul);
  io.a(s.cumulw);
  io.a(s.sintailpar);
  io.a(s.capchnk);
}
// ST4 stress tables, :1579-1580 / :1606-1608.
template <class IO>
void rec_st4_stress(IO& io, IogrState& s) {
  io.v(s.delust);
  io.v(s.deltail);
  io.v(s.deltauw);
  io.v(s.delu);
  io.v(s.delalp);
  io.a(s.taut);
  io.a(s.tauhft);
}
// :1581 / :1611.
template <class IO>
void rec_st4_tauhft2(IO& io, IogrState& s) { io.a(s.tauhft2); }
// NL1, :1648-1651 / :1662-1665.
template <class IO>
void rec_nl1(IO& io, IogrState& s) {
  io.v(s.snlc1);
  io.v(s.lam);
  io.v(s.kdcon);
  io.v(s.kdmn);
  io.v(s.snls1);
  io.v(s.snls2);
  io.v(s.snls3);
  io.v(s.iqtpe);
  io.v(s.nltail);
  io.v(s.gqnf1);
  io.v(s.gqnt1);
  io.v(s.gqnq_om2);
  io.v(s.gqthrsat);
  io.v(s.gqthrcou);
  io.a(s.gqamp);
}
// BT1, :1839 / :1844.
template <class IO>
void rec_bt1(IO& io, IogrState& s) { io.v(s.sbtc1); }
// DB1, :1871-1872 / :1879.
template <class IO>
void rec_db1(IO& io, IogrState& s) {
  io.v(s.sdbc1);
  io.v(s.sdbc2);
  io.v(s.fdonly);
}
// PR3 GSE alleviation, :1957 / :1962.
template <class IO>
void rec_prop(IO& io, IogrState& s) {
  io.v(s.wdcg);
  io.v(s.wdth);
}

/// Fortran .EQ. of a CHARACTER(*) dummy with a literal: the shorter is padded with blanks.
inline bool fstr_eq(const std::string& a, const std::string& b) {
  const std::size_t n = a.size() > b.size() ? a.size() : b.size();
  for (std::size_t i = 0; i < n; ++i) {
    const char ca = i < a.size() ? a[i] : ' ';
    const char cb = i < b.size() ? b[i] : ' ';
    if (ca != cb) return false;
  }
  return true;
}

struct FileCloser {
  void operator()(std::FILE* f) const { std::fclose(f); }
};

}  // namespace detail

// ---- W3IOGR ------------------------------------------------------------------------------

/// W3IOGR(INXOUT, NDSM, IMOD, FEXT), w3iogrmd.F90:116-2236, host only. inxout is 'READ',
/// 'WRITE' or 'GRID' (READ that stops after the 3-D output record, :1200-1203); s is the state
/// of grid IGRD = imod (1 when absent, :544-548), already selected as W3SETO/W3SETG select it;
/// fext defaults to 'ww3' (:550-554). The unit NDSM becomes the C stream of the file
/// FNMPRE(:IPRE)//'mod_def.'//FILEXT(:IEXT) (:575-590).
/// @pre For a WRITE, every array of s has the extent its comment in iogr_state.hpp gives
///      (the Fortran writes what is allocated; only MAPSTA/MAPST2 are checked, for MAPTMP).
inline IogrStatus w3iogr(const std::string& inxout, IogrState& s, IogrSaved& saved,
                         const IogrHooks& hooks, std::optional<int> imod = std::nullopt,
                         std::optional<std::string> fext = std::nullopt) {
  IogrStatus st;
  const LabNames tn;
  const int igrd = imod ? *imod : 1;
  FChar<13> tempxt{fext ? *fext : std::string("ww3")};

  if (!detail::fstr_eq(inxout, "READ") && !detail::fstr_eq(inxout, "WRITE") &&
      !detail::fstr_eq(inxout, "GRID")) {  // :556-560
    st.exit_code = 1;
    st.fmt = 900;
    st.found = inxout;
    return st;
  }
  const bool write = detail::fstr_eq(inxout, "WRITE");  // :562
  s.filext = tempxt;                                     // :570
  st.filename = s.fnmpre.trimmed() + "mod_def." + s.filext.trimmed();

  // :579-592. An OPEN without STATUS keeps an existing file and the first WRITE truncates it
  // after its record (v, libgfortran io/unix.c regular_file2, io/transfer.c st_write_done, gcc
  // 16.2.0); "wb" truncates at once and the file ends the same. An OPEN without ACTION that is
  // refused read-write retries read-only, then write-only (regular_file2), so a file it can open
  // only the other way fails at the first transfer with LIBERROR_BAD_ACTION, not at the OPEN.
  std::unique_ptr<std::FILE, detail::FileCloser> file(
      std::fopen(st.filename.c_str(), write ? "wb" : "rb"));
  if (!file) {
    const int err = errno;
    if (err == EACCES || err == EPERM || err == EROFS) {
      const std::unique_ptr<std::FILE, detail::FileCloser> other(
          std::fopen(st.filename.c_str(), write ? "rb" : "ab"));
      if (other) {
        st.record = 1;
        if (write) {
          st.exit_code = kExitGfortranRuntime;  // the WRITE at :599 has no IOSTAT
        } else {
          st.iostat = kIostatBadAction;
          st.exit_code = 52;  // EXTIOF at :650, IERR > 0
        }
        return st;
      }
    }
    st.exit_code = 50;  // EXTOPN(..., 50) -> EXTCDE(50), w3servmd.F90:1042
    st.iostat = err;    // LIBERROR_OS sets IOSTAT to errno (runtime/error.c)
    return st;
  }
  std::FILE* f = file.get();
  RecordWriter rw(f);
  RecordReader rr(f);
  detail::WriteIO wio{{rw}};
  detail::ReadIO rio{{rr}};

  auto wr_end = [&]() {
    ++st.record;
    if (!rw.end()) {
      st.exit_code = kExitGfortranRuntime;
      return false;
    }
    return true;
  };
  // EXTIOF(NDSE, IERR, 'W3IOGR', ..., 51): IERR > 0 exits 52, IERR < 0 exits 51
  // (w3servmd.F90:910-923).
  auto rd_end = [&]() {
    rr.finish();
    const int ierr = rr.iostat();
    if (ierr == 0) return true;
    st.iostat = ierr;
    st.exit_code = ierr > 0 ? 52 : 51;
    return false;
  };
  auto rd_begin = [&]() {
    ++st.record;
    rr.begin();
  };
  auto call = [&](const IogrHook& h) {
    if (!h) {
      st.exit_code = kExitMissingHook;
      return false;
    }
    const int code = h(s, igrd);
    if (code != 0) {
      st.exit_code = code;
      return false;
    }
    return true;
  };
  auto fail_name = [&](int code, int fmt, int idx, const FChar<30>& got, const FChar<30>& exp) {
    st.exit_code = code;
    st.fmt = fmt;
    st.name_index = idx;
    st.found = std::string(got.c.data(), 30);
    st.expected = std::string(exp.c.data(), 30);
  };

  // ---- Record 1, header :599-603 / :645-649; record 2, NBO :629-630 / :751-752 -----------
  if (write) {
    FChar<35> id = idstr();
    FChar<10> ver = vergrd();
    FChar<30> f0 = tn.t0, f1 = tn.t1, f2 = tn.t2, f3 = tn.t3, f4 = tn.t4, f5 = tn.t5,
              f6 = tn.t6, fp = tn.tp, fg = tn.tg, ff = tn.tf, fi = tn.ti;  // :523-533
    wio.c(id);
    wio.c(ver);
    wio.v(s.nx);
    wio.v(s.ny);
    wio.v(s.nsea);
    wio.v(s.nth);
    wio.v(s.nk);
    wio.v(s.nbi);
    wio.v(s.nfbpo);
    wio.c(s.gname);
    for (FChar<30>* p : {&f0, &f1, &f2, &f3, &f4, &f5, &f6, &fp, &fg, &ff, &fi}) wio.c(*p);
    if (!wr_end()) return st;
    // NBO(0:NFBPO), NBO2(0:NFBPO); an implied DO with NFBPO < 0 transfers nothing.
    if (s.nfbpo > 9) {
      st.exit_code = kExitNfbpoBounds;
      return st;
    }
    for (int i = 0; i <= s.nfbpo; ++i) wio.v(s.nbo[static_cast<std::size_t>(i)]);
    for (int i = 0; i <= s.nfbpo; ++i) wio.v(s.nbo2[static_cast<std::size_t>(i)]);
    if (!wr_end()) return st;
  } else {
    FChar<35> idtst;
    FChar<10> vertst;
    std::int32_t mth = 0, mk = 0;
    FChar<30> f0, f1, f2, f3, f4, f5, f6, fp, fg, ff, fi;
    rd_begin();
    rio.c(idtst);
    rio.c(vertst);
    rio.v(s.nx);
    rio.v(s.ny);
    rio.v(s.nsea);
    rio.v(mth);
    rio.v(mk);
    rio.v(s.nbi);
    rio.v(s.nfbpo);
    rio.c(s.gname);
    for (FChar<30>* p : {&f0, &f1, &f2, &f3, &f4, &f5, &f6, &fp, &fg, &ff, &fi}) rio.c(*p);
    if (!rd_end()) return st;
    s.nk = mk;  // :661-664, before the checks
    s.nth = mth;
    s.nk2 = s.nk + 2;
    s.nspec = s.nk * s.nth;
    if (!(idtst == idstr())) {  // :672-676
      st.exit_code = 10;
      st.fmt = 901;
      st.found = std::string(idtst.c.data(), 35);
      st.expected = std::string(idstr().c.data(), 35);
      return st;
    }
    if (!(vertst == vergrd())) {  // :677-681
      st.exit_code = 11;
      st.fmt = 902;
      st.found = std::string(vertst.c.data(), 10);
      st.expected = std::string(vergrd().c.data(), 10);
      return st;
    }
    if (s.nfbpo > 9) {  // :682-686
      st.exit_code = 13;
      st.fmt = 904;
      return st;
    }
    // :687-749, in the Fortran's order; FNAMEI reports index 3 and exits 17, as :711-716.
    if (!(f0 == tn.t0)) return fail_name(14, 905, 0, f0, tn.t0), st;
    if (!(f1 == tn.t1)) return fail_name(15, 905, 1, f1, tn.t1), st;
    if (!(f2 == tn.t2)) return fail_name(16, 905, 2, f2, tn.t2), st;
    if (!(f3 == tn.t3)) return fail_name(17, 905, 3, f3, tn.t3), st;
    if (!(fi == tn.ti)) return fail_name(17, 905, 3, fi, tn.ti), st;
    if (!(f4 == tn.t4)) return fail_name(18, 905, 4, f4, tn.t4), st;
    if (!(f5 == tn.t5)) return fail_name(19, 905, 5, f5, tn.t5), st;
    if (!(f6 == tn.t6)) return fail_name(20, 905, 6, f6, tn.t6), st;
    if (!(fp == tn.tp)) return fail_name(22, 906, -1, fp, tn.tp), st;
    if (!(fg == tn.tg)) return fail_name(22, 907, -1, fg, tn.tg), st;
    if (!(ff == tn.tf)) return fail_name(24, 908, -1, ff, tn.tf), st;
    rd_begin();
    for (int i = 0; i <= s.nfbpo; ++i) rio.v(s.nbo[static_cast<std::size_t>(i)]);
    for (int i = 0; i <= s.nfbpo; ++i) rio.v(s.nbo2[static_cast<std::size_t>(i)]);
    if (!rd_end()) return st;
  }

  // ---- Grid: records 3 to 8 (:765-1109) --------------------------------------------------
  const std::size_t nxy = static_cast<std::size_t>(s.ny) * static_cast<std::size_t>(s.nx);
  std::vector<std::int32_t> maptmp(nxy, 0);  // ALLOCATE(MAPTMP(NY,NX)), :765
  if (write) {
    if (s.mapsta.size() != nxy || s.mapst2.size() != nxy) {
      st.exit_code = kExitShape;
      return st;
    }
    for (std::size_t i = 0; i < nxy; ++i) maptmp[i] = encode_maptmp(s.mapsta[i], s.mapst2[i]);
    detail::rec_gtype(wio, s);
    if (!wr_end()) return st;
    switch (s.gtype) {  // :779-876; no CASE DEFAULT
      case kRlgType:
      case kSmcType:
        detail::rec_rect(wio, s);
        if (!wr_end()) return st;
        break;
      case kClgType: {  // :790-791, REAL(XGRD), REAL(YGRD)
        for (double x : s.xgrd) {
          float x4 = static_cast<float>(x);  // cvtsd2ss, round to nearest
          wio.v(x4);
        }
        for (double y : s.ygrd) {
          float y4 = static_cast<float>(y);
          wio.v(y4);
        }
        if (!wr_end()) return st;
        break;
      }
      case kUngType:
        detail::rec_ug_scheme(wio, s);
        if (!wr_end()) return st;
        // :858-859: the WRITE branch zeroes COUNTCON and IOBDP of the state before writing.
        for (auto& v : s.countcon) v = 0;
        for (auto& v : s.iobdp) v = 0;
        detail::rec_ug_mesh(wio, s);
        if (!wr_end()) return st;
        break;
      default:
        break;
    }
    detail::rec_maps(wio, s, maptmp);
    if (!wr_end()) return st;
    if (s.trflag != 0) {  // :913
      detail::rec_trans(wio, s);
      if (!wr_end()) return st;
    }
    detail::rec_times(wio, s);
    if (!wr_end()) return st;
    detail::rec_gridshift(wio, s);
    if (!wr_end()) return st;
  } else {
    rd_begin();
    detail::rec_gtype(rio, s);
    if (!rd_end()) return st;
    if (!s.ginit && !call(hooks.w3dimx)) return st;  // :972
    switch (s.gtype) {                                // :981-1037
      case kRlgType:
      case kSmcType: {
        rd_begin();
        detail::rec_rect(rio, s);
        if (!rd_end()) return st;
        if (s.xgrd.size() != nxy || s.ygrd.size() != nxy) {
          st.exit_code = kExitShape;
          return st;
        }
        const auto ny = static_cast<std::size_t>(s.ny);
        for (int ix = 1; ix <= s.nx; ++ix)  // :986-988
          for (std::size_t iy = 0; iy < ny; ++iy)
            s.xgrd[iy + ny * static_cast<std::size_t>(ix - 1)] = rect_coord(s.x0, ix, s.sx);
        for (int iy = 1; iy <= s.ny; ++iy)  // :989-991
          for (std::size_t ix = 0; ix < static_cast<std::size_t>(s.nx); ++ix)
            s.ygrd[static_cast<std::size_t>(iy - 1) + ny * ix] = rect_coord(s.y0, iy, s.sy);
        break;
      }
      case kClgType: {  // :993-1001
        std::vector<float> xgrd4(nxy, 0.f), ygrd4(nxy, 0.f);
        rd_begin();
        rio.a(xgrd4);
        rio.a(ygrd4);
        if (!rd_end()) return st;
        if (s.xgrd.size() != nxy || s.ygrd.size() != nxy) {
          st.exit_code = kExitShape;
          return st;
        }
        for (std::size_t i = 0; i < nxy; ++i) s.xgrd[i] = static_cast<double>(xgrd4[i]);
        for (std::size_t i = 0; i < nxy; ++i) s.ygrd[i] = static_cast<double>(ygrd4[i]);
        s.x0 = FLT_MAX;  // HUGE(X0), :1000-1001
        s.y0 = FLT_MAX;
        s.sx = FLT_MAX;
        s.sy = FLT_MAX;
        break;
      }
      case kUngType:
        rd_begin();
        detail::rec_ug_scheme(rio, s);
        if (!rd_end()) return st;
        if (!s.guginit && !call(hooks.w3dimug)) return st;  // :1024-1026
        rd_begin();
        detail::rec_ug_mesh(rio, s);
        if (!rd_end()) return st;
        break;
      default:
        break;
    }
    if (s.gtype != kUngType && !call(hooks.w3gntx)) return st;  // :1039
    rd_begin();
    detail::rec_maps(rio, s, maptmp);
    if (!rd_end()) return st;
    if (s.mapsta.size() != nxy || s.mapst2.size() != nxy ||
        s.mapsf.size() != 3 * static_cast<std::size_t>(s.nsea)) {
      st.exit_code = kExitShape;
      return st;
    }
    for (std::size_t i = 0; i < nxy; ++i) decode_maptmp(maptmp[i], s.mapsta[i], s.mapst2[i]);
    const auto ns = static_cast<std::size_t>(s.nsea);
    for (std::size_t is = 0; is < ns; ++is)  // :1073
      s.mapsf[is + 2 * ns] = mapsf3(s.mapsf[is], s.mapsf[is + ns], s.ny);
    if (s.trflag != 0) {  // :1074-1077
      rd_begin();
      detail::rec_trans(rio, s);
      if (!rd_end()) return st;
    }
    rd_begin();
    detail::rec_times(rio, s);
    if (!rd_end()) return st;
    rd_begin();
    detail::rec_gridshift(rio, s);
    if (!rd_end()) return st;
  }
  maptmp.clear();  // DEALLOCATE(MAPTMP), :1138

  // ---- Spectral grid and 3-D output indices (:1153-1198) ---------------------------------
  if (write) {
    detail::rec_spectral(wio, s);
    if (!wr_end()) return st;
    detail::rec_out3d(wio, s);
    if (!wr_end()) return st;
  } else {
    if (!s.sinit && !call(hooks.w3dims)) return st;  // :1168
    rd_begin();
    detail::rec_spectral(rio, s);
    if (!rd_end()) return st;
    rd_begin();
    detail::rec_out3d(rio, s);
    if (!rd_end()) return st;
  }
  if (detail::fstr_eq(inxout, "GRID")) return st;  // CLOSE and RETURN, :1200-1203

  // ---- Output boundary points, partitioning, numerics, RWND, LN1 (:1208-1450) ------------
  if (write) {
    detail::rec_bndout(wio, s);
    if (!wr_end()) return st;
    detail::rec_part(wio, s);
    if (!wr_end()) return st;
    detail::rec_numerics(wio, s);
    if (!wr_end()) return st;
    detail::rec_rwind(wio, s);
    if (!wr_end()) return st;
    detail::rec_ln1(wio, s);
    if (!wr_end()) return st;
  } else {
    if (s.nfbpo < 0) {  // NBO(NFBPO) in W3DMO5 would be out of bounds
      st.exit_code = kExitNfbpoBounds;
      return st;
    }
    if (!call(hooks.w3dmo5_out)) return st;  // :1217
    rd_begin();
    detail::rec_bndout(rio, s);
    if (!rd_end()) return st;
    rd_begin();
    detail::rec_part(rio, s);
    if (!rd_end()) return st;
    rd_begin();
    detail::rec_numerics(rio, s);
    if (!rd_end()) return st;
    rd_begin();
    detail::rec_rwind(rio, s);
    if (!rd_end()) return st;
    rd_begin();
    detail::rec_ln1(rio, s);
    if (!rd_end()) return st;
  }

  // ---- ST4 (:1539-1615) --------------------------------------------------------------------
  if (write) {
    if (!call(hooks.insin4_write)) return st;  // INSIN4(.TRUE.), :1540
    detail::rec_st4(wio, s);
    if (!wr_end()) return st;
    if (s.sintailpar[0] > 0.5f) {  // :1578
      detail::rec_st4_stress(wio, s);
      if (!wr_end()) return st;
      if (s.ttauwshelter > 0.f) {  // :1581
        detail::rec_st4_tauhft2(wio, s);
        if (!wr_end()) return st;
      }
    }
  } else {
    rd_begin();
    detail::rec_st4(rio, s);
    if (!rd_end()) return st;
    if (s.sintailpar[0] > 0.5f) {  // :1604
      if (!call(hooks.insin4_read)) return st;  // INSIN4(.FALSE.), :1605
      rd_begin();
      detail::rec_st4_stress(rio, s);
      if (!rd_end()) return st;
      if (s.ttauwshelter > 0.f) {  // :1610
        rd_begin();
        detail::rec_st4_tauhft2(rio, s);
        if (!rd_end()) return st;
      }
    }
  }

  // ---- NL1 (:1647-1806), BT1 (:1838-1846), DB1 (:1870-1881), PR3 (:1956-1964) -----------
  if (write) {
    detail::rec_nl1(wio, s);
    if (!wr_end()) return st;
  } else {
    rd_begin();
    detail::rec_nl1(rio, s);
    if (!rd_end()) return st;
    if (s.iqtpe > 0) {  // :1800-1806
      if (!call(hooks.insnl1)) return st;
    } else {
      if (!call(hooks.insnlgqm)) return st;
    }
  }
  if (write) {
    detail::rec_bt1(wio, s);
    if (!wr_end()) return st;
    detail::rec_db1(wio, s);
    if (!wr_end()) return st;
    detail::rec_prop(wio, s);
    if (!wr_end()) return st;
  } else {
    rd_begin();
    detail::rec_bt1(rio, s);
    if (!rd_end()) return st;
    rd_begin();
    detail::rec_db1(rio, s);
    if (!rd_end()) return st;
    rd_begin();
    detail::rec_prop(rio, s);
    if (!rd_end()) return st;
  }

  // ---- DISTAB once per process, CLOSE (:2015-2023) -----------------------------------------
  if (!write && !saved.fldisp) {
    if (!hooks.distab) {
      st.exit_code = kExitMissingHook;
      return st;
    }
    if (const int code = hooks.distab(saved); code != 0) {
      st.exit_code = code;
      return st;
    }
    saved.fldisp = 1;
  }
  return st;  // the stream closes with `file`, as CLOSE(NDSM) at :2023
}

}  // namespace ww::iogr
