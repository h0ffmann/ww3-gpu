// kokkos/src/ww_kokkos/iopo_w3iope.hpp
// W3IOPE, ported: the extraction of the point output. For each output point W3IOPP accepted, it
// counts the land, sea and ice corners of the enclosing cell, renormalises the bilinear weights,
// interpolates ice concentration, thickness and floe size, depth, wind, air-sea temperature
// difference and current (ignoring ice), then interpolates the spectrum from the sea corners only,
// converted from action N(k,theta) to F(f,theta) by TPI*SIG/CG, into OUTPTS(IMOD)%OUT2. Host
// only, no kernel. Phase 1, translated, not wired into WW3, not regtested.
//
// WW3 heritage: SUBROUTINE W3IOPE, WW3-noaa/model/src/w3iopomd.F90:873-1293 at NOAA-EMC/WW3
// 03adda91 (identical in the h0ffmann/WW3 fork at the WW3/ pin), author H. L. Tolman and the
// authors of its history block. Called by W3WAVE (w3wavemd.F90:2690) on the point-output process.
// Only what switches/switch_lab_shrd compiles is ported (read from `gfortran -E` with the defines
// of WW3's flags.make): SHRD (the spectra copied from A, :1211-1217). Not compiled and not
// ported: W3_RTD (the unrotation of WDO, TAUDO, CDO and of the spectrum, :1151, :1171, :1190,
// :1255-1260), W3_SETUP (DPO from ZETA_SETUP, :1127-1130), W3_FLX5 (DAIRO, TAUAO, TAUDO,
// :1134-1175), W3_MPI (the persistent receives, :1223-1225), W3_DIST, W3_T test output, W3_S
// STRACE. LPDLIB is a run-time flag (constants.F90:102); both of its branches are compiled
// and ported, and under the lab switch W3INIT sets it .FALSE. (w3initmd.F90:642).
//
// Arithmetic, from the -O3 GIMPLE and assembly of w3iopomd.F90 (gfortran 16.2.0, WW3's Release
// flags, object identical to the CMake build; port report):
// - Every weighted sum is left to right, RD(J)*X first: ((p1+p2)+p3)+p4. Products are commuted
//   in places in the object, which changes no bit of a finite or infinite result.
// - RDS, RDI and the non-PDLIB ICEFO start from +0.0 and keep the addition 0.0+x (a -0.0 term
//   gives +0.0); the PDLIB ICEFO and the other sums have no such leading zero.
// - The second normalisation divides the same RDI+RDS sum the first one tested (:1090, :1199).
// - COS and SIN of UD(IS(J)) are one sincosf call per corner (__builtin_cexpif); ATAN2 is atan2f,
//   only when the magnitude exceeds 1.E-7; SQRT(X**2+Y**2) is x*x+y*y then sqrtss.
// - TPI*SIG(IK) is folded to the REAL 6.2831855 times SIG and computed once per IK for the four
//   corners; the FAC loop is vectorised by four and two (mulps, divps), element by element, so
//   each FAC is the scalar (TPI*SIG)/CG. The SPCO loop is scalar. No FMA (contraction off).
//
// Index 0 is a valid sea index: MAPFS of a land corner is 0, and A(:,:,0), CG(:,0), DW(0), ...
// are read for it with weight 0 (CX(0) and CY(0) are zeroed first, :1038-1039). An index the
// Fortran would read outside its array (no bounds checks in WW3's Release flags) ends here with
// kIopeExitBounds instead, before any write.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "iopo_state.hpp"

namespace ww::iopo {

/// An array is shorter than the extents W3IOPE indexes it with (or NOPTS exceeds the NPT the
/// OUT2 arrays were allocated with). The same value as kExitShape of iogr_w3iogr.hpp.
inline constexpr int kIopeExitShape = -102;
/// A corner index IPTINT(:,J,I) outside MAPSTA(NY,NX), or a sea index MAPFS(IY,IX) outside
/// 0:NSEA (0:NSEAL for A): the Fortran reads out of bounds there. The same value as
/// kExitIndexBounds of iopo_w3iopp.hpp.
inline constexpr int kIopeExitBounds = -120;

/// constants.F90:72-73: PI = 3.141592653589793 as REAL, TPI = 2.0*PI in REAL; the GIMPLE literal
/// of TPI in W3IOPE is 6.283185482025146484375.
inline constexpr float kIopeTpi = 2.0f * static_cast<float>(3.141592653589793);
static_assert(kIopeTpi == 6.283185482025146484375f);

/// The module arrays W3IOPE reads and writes besides OUT2 (w3iopomd.F90:956-973), as pointers to
/// the caller's storage in Fortran element order, element 0 first. Extents as allocated:
/// SIG(0:NK+1) (w3gdatmd.F90:2066), MAPSTA and MAPFS (NY,NX) (:1733-1735), ICE, ICEH, ICEF
/// (0:NSEA) (w3wdatmd.F90:561-568), DW, CX, CY, UA, UD, AS (0:NSEA) (w3adatmd.F90:1004-1016),
/// CG(0:NK+1,0:NSEA) (:1325), SPPNT(NTH,NK,4) (:1451), and the argument A(NTH,NK,0:NSEAL).
struct IopeFields {
  std::int32_t nk = 0, nth = 0;
  std::int32_t nsea = 0;   // NSEA, upper bound of the (0:NSEA) arrays
  std::int32_t nseal = 0;  // NSEAL, upper bound of A's third index (NSEA under SHRD)
  std::int32_t nx = 0, ny = 0;
  FLogical lpdlib = 0;  // LPDLIB, constants.F90:102
  const float* sig = nullptr;
  const std::int32_t* mapsta = nullptr;
  const std::int32_t* mapfs = nullptr;
  const float* ice = nullptr;
  const float* iceh = nullptr;
  const float* icef = nullptr;
  const float* cg = nullptr;
  const float* dw = nullptr;
  const float* ua = nullptr;
  const float* ud = nullptr;
  const float* as = nullptr;
  float* cx = nullptr;     // CX(0) = 0. is written (:1038)
  float* cy = nullptr;     // CY(0) = 0. is written (:1039)
  float* sppnt = nullptr;  // SP => SPPNT, overwritten with the four corner spectra of each point
};

namespace iope_detail {
/// A weighted sum of :1114-1125, :1178-1184: RD(1)*X(IS(1)) + ... + RD(4)*X(IS(4)), left to right.
inline float wsum4(const float rd[4], const float* x, const std::int32_t is[4]) {
  float s = rd[0] * x[is[0]] + rd[1] * x[is[1]];
  s = s + rd[2] * x[is[2]];
  return s + rd[3] * x[is[3]];
}
}  // namespace iope_detail

/// W3IOPE(A): fills IL, IW, II, ICEFO, ICEO, ICEHO, DPO, WAO, WDO, ASO, CAO, CDO and
/// SPCO(:,1:NOPTS) of `o`, sets CX(0), CY(0) to 0 and leaves the last point's four corner spectra
/// in SPPNT. `a` is A(NTH,NK,0:NSEAL). Returns 0, or kIopeExitShape / kIopeExitBounds before any
/// write.
inline int w3iope(IopoState& o, const IopeFields& f, const float* a) {
  using std::size_t;
  const std::int32_t nk = f.nk, nth = f.nth, nopts = o.nopts;
  // CX(0) and CY(0) are written even when NOPTS is 0 (:1038-1039).
  if (f.cx == nullptr || f.cy == nullptr) return kIopeExitShape;
  if (nopts > 0) {
    if (nopts > o.npt || f.nx < 1 || f.ny < 1 || f.mapsta == nullptr || f.mapfs == nullptr ||
        f.ice == nullptr || f.iceh == nullptr || f.icef == nullptr || f.dw == nullptr ||
        f.ua == nullptr || f.ud == nullptr || f.as == nullptr || f.sppnt == nullptr ||
        a == nullptr || (nk > 0 && (f.sig == nullptr || f.cg == nullptr)))
      return kIopeExitShape;
    const size_t n = static_cast<size_t>(o.npt);
    if (o.iptint.size() < 8 * n || o.ptifac.size() < 4 * n || o.il.size() < n ||
        o.iw.size() < n || o.ii.size() < n || o.icefo.size() < n || o.iceo.size() < n ||
        o.iceho.size() < n || o.dpo.size() < n || o.wao.size() < n || o.wdo.size() < n ||
        o.aso.size() < n || o.cao.size() < n || o.cdo.size() < n)
      return kIopeExitShape;
    // SPCO(NSPEC,NPT): the stride of its second index is the NSPEC of W3DMO2.
    if (nk > 0 && nth > 0 && (n == 0 || o.spco.size() / n < static_cast<size_t>(nk) * nth))
      return kIopeExitShape;
    for (std::int32_t i = 0; i < nopts; ++i)
      for (int j = 0; j < 4; ++j) {
        const std::int32_t ix = o.iptint[0 + 2 * (j + 4 * static_cast<size_t>(i))];
        const std::int32_t iy = o.iptint[1 + 2 * (j + 4 * static_cast<size_t>(i))];
        if (ix < 1 || ix > f.nx || iy < 1 || iy > f.ny) return kIopeExitBounds;
        const std::int32_t is =
            f.mapfs[static_cast<size_t>(iy - 1) + static_cast<size_t>(f.ny) * (ix - 1)];
        if (is < 0 || is > f.nsea || is > f.nseal) return kIopeExitBounds;
      }
  }
  const size_t nspec_stride = o.npt > 0 ? o.spco.size() / static_cast<size_t>(o.npt) : 0;
  const size_t nk1 = static_cast<size_t>(nk < 0 ? 0 : nk);
  const size_t nth1 = static_cast<size_t>(nth < 0 ? 0 : nth);

  f.cx[0] = 0.0f;  // :1038
  f.cy[0] = 0.0f;  // :1039

  std::vector<float> fac1(nk1), fac2(nk1), fac3(nk1), fac4(nk1);  // FAC1..FAC4(NK), :1017-1018
  float* const fac[4] = {fac1.data(), fac2.data(), fac3.data(), fac4.data()};

  for (std::int32_t i = 0; i < nopts; ++i) {  // DO I=1, NOPTS (:1043)
    const size_t ip = static_cast<size_t>(i);
    std::int32_t ix[4], iy[4], is[4], im[4];
    float rd[4];
    for (int j = 0; j < 4; ++j) {  // :1051-1053
      ix[j] = o.iptint[0 + 2 * (j + 4 * ip)];
      iy[j] = o.iptint[1 + 2 * (j + 4 * ip)];
      rd[j] = o.ptifac[j + 4 * ip];
    }
    o.il[ip] = 0;  // :1062-1066
    o.iw[ip] = 0;
    o.ii[ip] = 0;
    float rds = 0.0f;
    float rdi = 0.0f;
    for (int j = 0; j < 4; ++j) {  // :1068-1086
      const size_t m = static_cast<size_t>(iy[j] - 1) + static_cast<size_t>(f.ny) * (ix[j] - 1);
      is[j] = f.mapfs[m];
      im[j] = f.mapsta[m];
      if (im[j] > 0) {
        o.iw[ip] = o.iw[ip] + 1;
        rds = rds + rd[j];
      } else if (im[j] < 0) {
        o.ii[ip] = o.ii[ip] + 1;
        rdi = rdi + rd[j];
      } else {
        o.il[ip] = o.il[ip] + 1;
        rd[j] = 0.0f;
      }
    }
    // Depth, wind and current, ignore ice (:1090-1093).
    if (rds + rdi > 1.e-7f) {
      const float facrd = 1.0f / (rds + rdi);
      for (int j = 0; j < 4; ++j) rd[j] = rd[j] * facrd;
    }

    if (!f.lpdlib) {  // :1101-1112; JSEA = ISEA under SHRD (:1109)
      o.icefo[ip] = 0.0f;
      for (int j = 0; j < 4; ++j) o.icefo[ip] = o.icefo[ip] + rd[j] * f.icef[is[j]];
    } else {  // :1114-1115
      o.icefo[ip] = iope_detail::wsum4(rd, f.icef, is);
    }
    o.iceo[ip] = iope_detail::wsum4(rd, f.ice, is);    // :1118-1119
    o.iceho[ip] = iope_detail::wsum4(rd, f.iceh, is);  // :1121-1122
    o.dpo[ip] = iope_detail::wsum4(rd, f.dw, is);      // :1124-1125

    // :1138-1145: (RD(J)*UA(IS(J)))*COS(UD(IS(J))), summed left to right.
    float sn[4], cs[4];
    for (int j = 0; j < 4; ++j) ::sincosf(f.ud[is[j]], &sn[j], &cs[j]);
    float ru[4];
    for (int j = 0; j < 4; ++j) ru[j] = rd[j] * f.ua[is[j]];
    float wndx = ru[0] * cs[0] + ru[1] * cs[1];
    wndx = wndx + ru[2] * cs[2];
    wndx = wndx + ru[3] * cs[3];
    float wndy = ru[0] * sn[0] + ru[1] * sn[1];
    wndy = wndy + ru[2] * sn[2];
    wndy = wndy + ru[3] * sn[3];

    o.wao[ip] = std::sqrt(wndx * wndx + wndy * wndy);  // :1147
    o.wdo[ip] = o.wao[ip] > 1.e-7f ? ::atan2f(wndy, wndx) : 0.0f;  // :1148-1155

    o.aso[ip] = iope_detail::wsum4(rd, f.as, is);  // :1178-1179
    const float curx = iope_detail::wsum4(rd, f.cx, is);  // :1181-1182
    const float cury = iope_detail::wsum4(rd, f.cy, is);  // :1183-1184
    o.cao[ip] = std::sqrt(curx * curx + cury * cury);  // :1186
    o.cdo[ip] = o.cao[ip] > 1.e-7f ? ::atan2f(cury, curx) : 0.0f;  // :1187-1194

    // Interp. weights for spectra, no ice points (:1198-1201).
    if (rds > 1.e-7f) {
      const float facrd = (rds + rdi) / rds;
      for (int j = 0; j < 4; ++j) rd[j] = rd[j] * facrd;
    }

    // SHRD: SP(ITH,IK,J) = A(ITH,IK,IS(J)) (:1211-1217).
    for (int j = 0; j < 4; ++j)
      for (size_t ik = 0; ik < nk1; ++ik)
        for (size_t ith = 0; ith < nth1; ++ith)
          f.sppnt[ith + nth1 * (ik + nk1 * static_cast<size_t>(j))] =
              a[ith + nth1 * (ik + nk1 * static_cast<size_t>(is[j]))];

    // FACn(IK) = TPI * SIG(IK) / CG(IK,IS(n)) (:1230-1235); SIG and CG have lower bound 0.
    for (size_t ik = 1; ik <= nk1; ++ik) {
      const float ts = kIopeTpi * f.sig[ik];
      for (int j = 0; j < 4; ++j)
        fac[j][ik - 1] = ts / f.cg[ik + (nk1 + 2) * static_cast<size_t>(is[j])];
    }

    // SPCO(ISP,I), ISP = ITH + (IK-1)*NTH (:1237-1248).
    for (size_t ik = 0; ik < nk1; ++ik)
      for (size_t ith = 0; ith < nth1; ++ith) {
        const size_t isp = ith + ik * nth1;
        const float* sp = f.sppnt + ith + nth1 * ik;
        const size_t s4 = nth1 * nk1;  // stride of SP's third index
        float v = rd[0] * sp[0] * fac[0][ik] + rd[1] * sp[s4] * fac[1][ik];
        v = v + rd[2] * sp[2 * s4] * fac[2][ik];
        v = v + rd[3] * sp[3 * s4] * fac[3][ik];
        o.spco[isp + nspec_stride * ip] = v;
      }
  }
  return 0;
}

}  // namespace ww::iopo
