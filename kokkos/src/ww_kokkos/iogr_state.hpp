// kokkos/src/ww_kokkos/iogr_state.hpp
// Host state of W3IOGR: the module variables that the model-definition reader/writer reads or
// writes, one plain struct per grid, as switches/switch_lab_shrd compiles them. Phase 1,
// translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3iogrmd.F90:116-2236 (W3IOGR; its module ends at :2240) at NOAA-EMC/WW3 03adda91,
// authors H. L. Tolman and F. Ardhuin; the declarations it uses are in w3gdatmd.F90,
// w3odatmd.F90, constants.F90, w3src4md.F90, w3timemd.F90 and w3dispmd.F90 at the same commit.
//
// Kinds are the ones the -O3 GIMPLE transfers (gfortran 16.2, WW3's Release flags):
// REAL is float (transfer_real ..., 4), REAL(8) and DOUBLE PRECISION are double (..., 8),
// INTEGER is int32_t, INTEGER*2 int16_t, INTEGER*1 int8_t, default LOGICAL is 4 bytes
// (transfer_logical ..., 4). A LOGICAL is kept as its int32_t storage, not as bool, so a value
// read from a file is written back with the same bytes. CHARACTER(LEN=n) is n bytes, blank
// padded, no terminator.
//
// Arrays are std::vector in Fortran element order (column-major, first index fastest). Each
// field's comment gives the Fortran shape with its lower bounds: element (i,j) of an array
// declared (l1:u1, l2:u2) is v[(i-l1) + (u1-l1+1)*(j-l2)]. Every record writes or reads whole
// arrays, so the stream order is the vector order. The extents are the ones the allocating
// routine gives (W3DIMX, W3DIMUG, W3DIMS, W3DMO5, INSIN4); the iogr_shape_* helpers below
// reproduce those extents and nothing else of those routines.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace ww::iogr {

using FLogical = std::int32_t;  // default LOGICAL storage; gfortran writes .TRUE. as 1

/// CHARACTER(LEN=N): N bytes, blank padded, as Fortran assignment pads or truncates.
template <std::size_t N>
struct FChar {
  std::array<char, N> c{};
  FChar() { c.fill(' '); }
  explicit FChar(const std::string& s) { assign(s); }
  void assign(const std::string& s) {
    c.fill(' ');
    std::memcpy(c.data(), s.data(), s.size() < N ? s.size() : N);
  }
  /// Fortran .EQ. between two strings of the same length: byte comparison.
  bool operator==(const FChar& o) const { return c == o.c; }
  /// LEN_TRIM: trailing blanks only (not tabs).
  std::size_t len_trim() const {
    std::size_t n = N;
    while (n > 0 && c[n - 1] == ' ') --n;
    return n;
  }
  std::string trimmed() const { return std::string(c.data(), len_trim()); }
};

// Grid types, w3gdatmd.F90:625-628.
inline constexpr std::int32_t kRlgType = 1;
inline constexpr std::int32_t kClgType = 2;
inline constexpr std::int32_t kUngType = 3;
inline constexpr std::int32_t kSmcType = 4;

// Table sizes fixed by PARAMETER statements.
inline constexpr int kNkhs = 2000;        // NKHS, w3gdatmd.F90:637
inline constexpr int kNkd = 1300;         // NKD, w3gdatmd.F90:637 (the ST4 value)
inline constexpr int kNdtab = 2000;       // NDTAB, w3gdatmd.F90:638
inline constexpr int kItaumax = 200;      // ITAUMAX, w3src4md.F90:92
inline constexpr int kJumax = 200;        // JUMAX, w3src4md.F90:92
inline constexpr int kIustar = 100;       // IUSTAR, w3src4md.F90:93
inline constexpr int kIalpha = 200;       // IALPHA, w3src4md.F90:93
inline constexpr int kIlevtail = 50;      // ILEVTAIL, w3src4md.F90:93
inline constexpr int kSizeFwtable = 300;  // SIZEFWTABLE, constants.F90:92
inline constexpr int kNar1d = 121;        // NAR1D, w3dispmd.F90:75

/// The fields of GRIDS(IGRD), SGRDS(IGRD), MPARS(IGRD), OUTPTS(IGRD) and the module scalars
/// that W3IOGR transfers or tests, after W3SETG/W3SETO have selected grid IGRD
/// (w3iogrmd.F90:568-569); the caller owns one IogrState per grid. FNMPRE, DELAB, FWTABLE,
/// CALTYPE and the W3SRC4MD tables (DIKCUMUL, DELUST to DELALP, TAUT, TAUHFT, TAUHFT2) are module
/// variables WW3 shares between grids, the last READ winning; a multi-grid caller shares them.
struct IogrState {
  // --- W3GDATMD, TYPE GRID (w3gdatmd.F90:644-793) ----------------------------------------
  std::int32_t gtype = 0, iclose = 0;
  std::int32_t nx = 0, ny = 0, nsea = 0, trflag = 0;
  FLogical ginit = 0, guginit = 0;
  FLogical flagll = 0;
  FChar<30> gname;
  FChar<13> filext;
  float sx = 0, sy = 0, x0 = 0, y0 = 0;
  float dtcfl = 0, dtcfli = 0, dtmax = 0, dtmin = 0, dmin = 0, ctmax = 0;
  float fice0 = 0, ficen = 0, ficel = 0, pfmove = 0;
  float stexu = 0, steyu = 0, stedu = 0, iicehmin = 0, iicehinit = 0;
  std::array<float, 4> icescales{};  // ICESCALES(4)
  float iicehfac = 0, iicehdisp = 0, iiceddisp = 0, iicefdisp = 0;
  float btbeta = 0, aaircmin = 0, aairgb = 0, ffacberg = 0;
  double gridshift = 0;  // REAL(8)
  FLogical fldry = 0, flcx = 0, flcy = 0, flcth = 0, flck = 0, flsou = 0, iicedisp = 0;
  FLogical ic_numerics = 0, cmprtrck = 0;
  std::vector<float> zb;            // ZB(NSEA); ZB(NX) on an unstructured grid (W3DIMUG)
  std::vector<std::int32_t> mapsta;  // MAPSTA(NY,NX)
  std::vector<std::int32_t> mapst2;  // MAPST2(NY,NX)
  std::vector<std::int32_t> mapfs;   // MAPFS(NY,NX)
  std::vector<std::int32_t> mapsf;   // MAPSF(NSEA,3)
  std::vector<double> xgrd, ygrd;    // XGRD(NY,NX), YGRD(NY,NX); (1,NX) unstructured
  std::vector<float> trnx, trny;     // TRNX(NY,NX), TRNY(NY,NX)
  std::vector<float> clats, clatis, cthg0s;  // CLATS(0:NSEA), CLATIS(0:NSEA), CTHG0S(0:NSEA)
  std::array<std::int32_t, 15> e3df{};       // E3DF(3,5)
  std::array<std::int32_t, 3> p2msf{};       // P2MSF(3)
  std::array<std::int32_t, 3> us3df{};       // US3DF(3)
  std::array<std::int32_t, 2> usspf{};       // USSPF(2)
  std::array<float, 25> ussp_wn{};           // USSP_WN(25)

  // Unstructured grid (GTYPE = UNGTYPE), w3gdatmd.F90:753-778, TYPE SCHM :1038-1066.
  FLogical fsn = 0, fspsi = 0, fsfct = 0, fsnimp = 0, fstotalimp = 0, fstotalexp = 0;
  FLogical fsbccfl = 0, fsrefraction = 0, fsfreqshift = 0, fssource = 0, do_change_wlv = 0;
  double solverthr_stp = 0, crit_dep_stp = 0;
  std::int32_t ntri = 0, countot = 0, countri = 0, nnz = 0;
  FLogical b_jgs_terminate_maxiter = 0, b_jgs_terminate_difference = 0;
  FLogical b_jgs_terminate_norm = 0, b_jgs_limiter = 0, b_jgs_block_gauss_seidel = 0;
  FLogical b_jgs_use_jacobi = 0, b_jgs_source_nonlinear = 0;
  std::int32_t b_jgs_maxiter = 0, b_jgs_nlevel = 0;
  double b_jgs_pmin = 0, b_jgs_diff_thr = 0, b_jgs_norm_thr = 0;  // REAL*8
  float dxymax = 0, maxx = 0, maxy = 0;
  std::vector<std::int32_t> trigp;    // TRIGP(3,NTRI)
  std::vector<double> tria;           // TRIA(NTRI), REAL(8)
  std::vector<double> len;            // LEN(NTRI,3), REAL(8)
  std::vector<double> ien;            // IEN(NTRI,6), REAL(8)
  std::vector<float> angle0, angle;   // ANGLE0(NTRI,3), ANGLE(NTRI,3)
  std::vector<double> si;             // SI(NX), REAL(8)
  std::vector<std::int32_t> index_cell;  // INDEX_CELL(NX+1)
  std::vector<std::int32_t> ccon, countcon;  // CCON(NX), COUNTCON(NX)
  std::vector<std::int32_t> ie_cell, pos_cell;  // IE_CELL(COUNTOT), POS_CELL(COUNTOT)
  std::vector<std::int16_t> iobp;     // IOBP(NX), INTEGER*2
  std::vector<std::int8_t> iobpa, iobdp;  // IOBPA(NX), IOBDP(NX), INTEGER*1
  std::vector<std::int8_t> iobpd;     // IOBPD(NTH,NX), INTEGER*1
  std::vector<std::int32_t> iaa;      // IAA(NX+1)
  std::vector<std::int32_t> jaa;      // JAA(NNZ)
  std::vector<std::int32_t> posi;     // POSI(3,COUNTOT)

  // --- W3GDATMD, TYPE SGRD (w3gdatmd.F90:795-804) ----------------------------------------
  std::int32_t nk = 0, nk2 = 0, nth = 0, nspec = 0;
  FLogical sinit = 0;
  std::vector<std::int32_t> mapwn, mapth;  // MAPWN(NSPEC+NTH), MAPTH(NSPEC+NTH)
  float dth = 0, xfr = 0, fr1 = 0, fte = 0, ftf = 0, ftwn = 0, fttr = 0, ftwl = 0;
  float facti1 = 0, facti2 = 0, fachfa = 0, fachfe = 0;
  std::vector<float> th;                          // TH(NTH)
  std::vector<float> esin, ecos, es2, esc, ec2;   // (NSPEC+NTH)
  std::vector<float> sig, dsip;                   // SIG(0:NK+1), DSIP(0:NK+1)
  std::vector<float> sig2, dden2;                 // SIG2(NSPEC), DDEN2(NSPEC)
  std::vector<float> dsii, dden;                  // DSII(NK), DDEN(NK)

  // --- W3GDATMD, TYPE NPAR (:806-815), constants.F90:93-94 -------------------------------
  float facp = 0, xrel = 0, xflt = 0, fxfm = 0, fxpm = 0, xft = 0, xfc = 0, facsd = 0;
  float fhmax = 0, rwindc = 0;
  float delab = 0;                                     // DELAB (constants.F90:94)
  std::array<float, kSizeFwtable + 1> fwtable{};       // FWTABLE(0:SIZEFWTABLE)

  // --- SLNP (LN1, :867-877), SBTP (BT1, :970-986), SDBP (DB1, :988-997), PROP (:817-834) -
  float slnc1 = 0, fspm = 0, fshf = 0;
  float sbtc1 = 0;
  float sdbc1 = 0, sdbc2 = 0;
  FLogical fdonly = 0;
  float wdcg = 0, wdth = 0;

  // --- SRCP, ST4 (w3gdatmd.F90:879-924) and W3SRC4MD module variables (w3src4md.F90:92-101)
  float zzwnd = 0, aalpha = 0, zz0max = 0, bbeta = 0, ssinthp = 0, zzalp = 0;
  float ttauwshelter = 0, ssinbr = 0, zz0rat = 0, ssdsbr = 0, ssdsbt = 0, ssdsp = 0;
  float ssdscos = 0, ssdsdth = 0, wwnmeanp = 0, wwnmeanptail = 0, sstxftf = 0;
  float sstxftftail = 0, sstxftwn = 0, ssdsbrf1 = 0, ssdsbrf2 = 0, ssdsbck = 0;
  float ssdsabk = 0, ssdspbk = 0, ssdsbint = 0, ffxpm = 0, ffxfm = 0, ffxfa = 0;
  float ssdshck = 0;
  std::int32_t sswellfpar = 0, ssdsiso = 0, ssdsbrfdf = 0;
  std::array<float, 7> sswellf{};      // SSWELLF(1:7)
  std::array<float, 21> ssdsc{};       // SSDSC(1:21)
  std::array<float, 5> ssdsbm{};       // SSDSBM(0:4)
  std::array<float, 5> sintailpar{};   // SINTAILPAR(1:5)
  std::array<float, 10> capchnk{};     // CAPCHNK(1:10)
  std::vector<std::int32_t> iktab;     // IKTAB(NK,NDTAB)
  std::vector<float> dcki, qbi;        // DCKI(NKHS,NKD), QBI(NKHS,NKD)
  std::vector<std::int32_t> satindices;  // SATINDICES(2*SDSNTH+1,NTH), SDSNTH = NTH/2-1
  std::vector<float> satweights;       // SATWEIGHTS(2*SDSNTH+1,NTH)
  std::vector<float> cumulw;           // CUMULW(NSPEC,NSPEC)
  std::int32_t dikcumul = 0;           // W3SRC4MD
  float delust = 0, deltail = 0, deltauw = 0, delu = 0, delalp = 0;  // W3SRC4MD
  std::vector<float> taut;     // TAUT(0:ITAUMAX,0:JUMAX), allocated by INSIN4
  std::vector<float> tauhft;   // TAUHFT(0:IUSTAR,0:IALPHA), allocated by INSIN4
  std::vector<float> tauhft2;  // TAUHFT2(0:IUSTAR,0:IALPHA,0:ILEVTAIL), allocated by INSIN4

  // --- SNLP, NL1 (w3gdatmd.F90:926-968) --------------------------------------------------
  float snlc1 = 0, lam = 0, kdcon = 0, kdmn = 0, snls1 = 0, snls2 = 0, snls3 = 0;
  std::int32_t iqtpe = 0, gqnf1 = 0, gqnt1 = 0, gqnq_om2 = 0;
  float nltail = 0, gqthrsat = 0, gqthrcou = 0;
  std::array<float, 4> gqamp{};  // GQAMP(4)

  // --- W3ODATMD (w3odatmd.F90:337, OTYPE5 :410-429, OTYPE6 :431-440, OUTPUT :442-461) ----
  FChar<80> fnmpre{std::string("./")};  // FNMPRE = './' (w3odatmd.F90:337)
  std::int32_t nbi = 0, nfbpo = 0;
  std::array<std::int32_t, 10> nbo{}, nbo2{};  // NBO(0:9), NBO2(0:9)
  FLogical flbpi = 0, flbpo = 0;
  std::vector<float> xbpo, ybpo;          // XBPO(NBO(NFBPO)), YBPO(NBO(NFBPO))
  std::vector<float> rdbpo;               // RDBPO(NBO(NFBPO),4)
  std::vector<std::int32_t> ipbpo;        // IPBPO(NBO(NFBPO),4)
  std::vector<std::int32_t> isbpo;        // ISBPO(4*NBO(NFBPO))
  std::int32_t ihmax = 0, noswll = 0, ptmeth = 0;
  float hspmin = 0, wsmult = 0, wscut = 0, ptfcut = 0;
  FLogical flcomb = 0;

  // --- W3TIMEMD (w3timemd.F90:79) ---------------------------------------------------------
  FChar<8> caltype;
};

/// Process-wide state, one per process as in the Fortran: FLDISP, a SAVE local (w3iogrmd.F90:353),
/// and the W3DISPMD tables DISTAB fills (w3dispmd.F90:78-79), which every grid shares.
struct IogrSaved {
  FLogical fldisp = 0;
  std::array<float, kNar1d + 1> ewn1{}, ecg1{};  // EWN1(0:NAR1D), ECG1(0:NAR1D)
  std::int32_t n1max = 0;
  float dsie = 0;
};

// --- Extents of the allocating routines. Shape only: the error checks, pointer setup and
// other fields of those routines are not reproduced, and they are not ports of them. Every
// element allocated here is overwritten by the record that follows the call in W3IOGR.

/// W3DIMX's arrays that W3IOGR transfers (w3gdatmd.F90:1725-1757); sets GINIT (:1816).
inline void iogr_shape_w3dimx(IogrState& s) {
  const auto nxy = static_cast<std::size_t>(s.ny) * static_cast<std::size_t>(s.nx);
  const auto ns = static_cast<std::size_t>(s.nsea);
  if (s.gtype != kUngType) {
    s.zb.assign(ns, 0.f);
    s.xgrd.assign(nxy, 0.);
    s.ygrd.assign(nxy, 0.);
  }
  s.mapsta.assign(nxy, 0);
  s.mapst2.assign(nxy, 0);
  s.mapfs.assign(nxy, 0);
  s.mapsf.assign(ns * 3, 0);
  s.clats.assign(ns + 1, 0.f);
  s.clatis.assign(ns + 1, 0.f);
  s.cthg0s.assign(ns + 1, 0.f);
  s.trnx.assign(nxy, 0.f);
  s.trny.assign(nxy, 0.f);
  s.ginit = 1;
}

/// W3DIMUG's arrays that W3IOGR transfers (w3gdatmd.F90:3235-3260), MX = NX, MTRI = NTRI,
/// COUNTOTA = COUNTOT; IOBPD uses the module NTH set from the first record; sets GUGINIT.
inline void iogr_shape_w3dimug(IogrState& s) {
  const auto nx = static_cast<std::size_t>(s.nx);
  const auto nt = static_cast<std::size_t>(s.ntri);
  const auto nc = static_cast<std::size_t>(s.countot);
  s.trigp.assign(3 * nt, 0);
  s.si.assign(nx, 0.);
  s.xgrd.assign(nx, 0.);
  s.ygrd.assign(nx, 0.);
  s.zb.assign(nx, 0.f);
  s.tria.assign(nt, 0.);
  s.ien.assign(nt * 6, 0.);
  s.len.assign(nt * 3, 0.);
  s.angle.assign(nt * 3, 0.f);
  s.angle0.assign(nt * 3, 0.f);
  s.ccon.assign(nx, 0);
  s.countcon.assign(nx, 0);
  s.index_cell.assign(nx + 1, 0);
  s.ie_cell.assign(nc, 0);
  s.pos_cell.assign(nc, 0);
  s.iaa.assign(nx + 1, 0);
  s.jaa.assign(static_cast<std::size_t>(s.nnz), 0);
  s.posi.assign(3 * nc, 0);
  s.iobp.assign(nx, 1);  // IOBP(:)=1, w3gdatmd.F90:3263
  s.iobpd.assign(static_cast<std::size_t>(s.nth) * nx, 0);
  s.iobdp.assign(nx, 0);
  s.iobpa.assign(nx, 0);
  s.guginit = 1;
}

/// W3DIMS (w3gdatmd.F90:2049-2127), MK = NK, MTH = NTH: the spectral arrays and the ST4
/// tables W3IOGR transfers, NK2 and NSPEC, SINIT. SDSNTH = MTH/2-1 is integer division (:2097).
inline void iogr_shape_w3dims(IogrState& s) {
  const auto nk = static_cast<std::size_t>(s.nk);
  const auto nth = static_cast<std::size_t>(s.nth);
  const std::size_t nspec = nk * nth;
  s.mapwn.assign(nspec + nth, 0);
  s.mapth.assign(nspec + nth, 0);
  s.th.assign(nth, 0.f);
  s.esin.assign(nspec + nth, 0.f);
  s.ecos.assign(nspec + nth, 0.f);
  s.es2.assign(nspec + nth, 0.f);
  s.esc.assign(nspec + nth, 0.f);
  s.ec2.assign(nspec + nth, 0.f);
  s.sig.assign(nk + 2, 0.f);
  s.sig2.assign(nspec, 0.f);
  s.dsip.assign(nk + 2, 0.f);
  s.dsii.assign(nk, 0.f);
  s.dden.assign(nk, 0.f);
  s.dden2.assign(nspec, 0.f);
  s.iktab.assign(nk * static_cast<std::size_t>(kNdtab), 0);
  s.dcki.assign(static_cast<std::size_t>(kNkhs) * kNkd, 0.f);
  s.qbi.assign(static_cast<std::size_t>(kNkhs) * kNkd, 0.f);
  const int sdsnth = s.nth / 2 - 1;
  const auto nsat = static_cast<std::size_t>(2 * sdsnth + 1) * nth;
  s.satindices.assign(nsat, 1);  // :2103
  s.satweights.assign(nsat, 0.f);
  s.cumulw.assign(nspec * nspec, 0.f);
  s.sinit = 1;
  s.nk2 = s.nk + 2;
  s.nspec = s.nk * s.nth;
}

/// W3DMO5 with IBLOCK = 2 (w3odatmd.F90:1466-1480): the output boundary arrays, sized by
/// NBO(NFBPO), ISBPO zeroed. The caller has checked 0 <= NFBPO <= 9.
inline void iogr_shape_w3dmo5_out(IogrState& s) {
  const auto n = static_cast<std::size_t>(s.nbo[static_cast<std::size_t>(s.nfbpo)]);
  s.ipbpo.assign(n * 4, 0);
  s.isbpo.assign(4 * n, 0);
  s.xbpo.assign(n, 0.f);
  s.ybpo.assign(n, 0.f);
  s.rdbpo.assign(n * 4, 0.f);
}

/// The allocations of INSIN4 that W3IOGR's later records need (w3src4md.F90:1118-1128):
/// TAUT and TAUHFT when SINTAILPAR(1) > 0.5, TAUHFT2 when also TTAUWSHELTER > 0, each only if
/// not yet allocated. A hook standing in for INSIN4(.FALSE.) must do at least this.
inline void iogr_shape_insin4(IogrState& s) {
  if (!(s.sintailpar[0] > 0.5f)) return;
  if (s.taut.empty()) s.taut.assign(static_cast<std::size_t>(kItaumax + 1) * (kJumax + 1), 0.f);
  if (s.tauhft.empty())
    s.tauhft.assign(static_cast<std::size_t>(kIustar + 1) * (kIalpha + 1), 0.f);
  if (s.ttauwshelter > 0.f && s.tauhft2.empty())
    s.tauhft2.assign(static_cast<std::size_t>(kIustar + 1) * (kIalpha + 1) * (kIlevtail + 1), 0.f);
}

}  // namespace ww::iogr
