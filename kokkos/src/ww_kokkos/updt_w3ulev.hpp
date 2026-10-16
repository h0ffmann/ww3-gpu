// kokkos/src/ww_kokkos/updt_w3ulev.hpp
// W3ULEV, ported to Kokkos: the new water level and depth, the wavenumbers and
// group velocities at the new depth, the dry and newly wet points (MAPSTA,
// MAPST2, UST, a zeroed spectrum), and the action spectrum redistributed onto
// the new wavenumber grid where it moved by more than RDKMIN band widths.
// Phase 1, translated, not regtested.
//
// WW3 heritage: WW3/model/src/w3updtmd.F90:2066-2614 at the h0ffmann/WW3 pin
// 8e433f99; author H. L. Tolman. Its one call, in W3WAVE, is W3ULEV(VA, VA)
// (w3wavemd.F90:1437; :1434 at NOAA 03adda91), so A(NTH,NK,0:NSEAL) and
// VA(NSPEC,0:NSEAL) are the same
// memory; the port takes VA once and reads A(ITH,IK,JSEA) as
// VA(ITH+(IK-1)*NTH, JSEA). Evidence: kokkos/port_report/W3ULEV_KOKKOS.md.
//
// Only what switch_lab_shrd compiles is ported. Not compiled there and not
// ported: W3_PDLIB (WAVNU3 at :2361 and :2378, SET_IOBDP_PDLIB at :2580),
// W3_REF1 (W3SETREF at :2584), W3_TIDE (the blocks between :2261 and :2335),
// W3_SETUP (:2327-2332),
// W3_S, W3_T, W3_T2, W3_T3 (trace and test output). Under the lab switch the
// GTYPE.EQ.UNGTYPE block of :2578-2586 is empty and the object has no load of
// GTYPE there. DWO (:2285) is written and never read; it is not translated.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3updtmd.F90 (WW3's CMake Release flags, switch_lab_shrd):
// - MAX operand order is the build's maxss (fmax2(a,b) = b>a?b:a, so a NaN or a
//   tie gives a): :2336 MAX(0.,WLVeff-ZB) is maxss 0,d; :2359 MAX(DMIN,-ZB) and
//   :2370 MAX(DMIN,DW) are maxss DMIN,x. A NaN depth gives 0 or DMIN.
// - KDMAX = 4., RDKMIN = 0.05 (:2207) are folded: 4.0, 0x3D4CCCCD; UST = 0.05
//   (:2422) is the same float; 0.95 (:2543) is 0x3F733333.
// - ABS of a REAL (:2438) is andps with the sign mask; -ZB (:2359) is xorps.
// - WAVNU1 (w3dispmd.F90:85-202) is an out-of-line call at :2363 and :2380,
//   ported as ww::prop::wavnu1; GET_JSEA_IBELONG (w3parall.F90:1265-1384) is an
//   out-of-line call, ported as ww::parall::get_jsea_ibelong.
// - The loops at :2256, :2384, :2462, :2521, :2527, :2537, :2545 are vectorised
//   by gfortran. Each does one elementwise integer op, divss, mulss or
//   mulss+addss per element with no reduction, so the packed and the scalar form
//   give the same bits; the C++ loops in Fortran order.
// - RD2 is carried from one sea point to the next: in the GIMPLE
//   rd2_928 = PHI <rd2_567 (back edge), rd2_714(D) (undefined)> at the head of
//   the :2347 loop. :2543 reads it with I2 <= NK at a point whose :2502 loop
//   never assigned it only when COND is false (:2487; IK0 <= NK otherwise, and
//   a point where every IK cycles at :2509 leaves I2 = NK+1), and then gets the
//   RD2 of the last point that did. So :2347-2570 runs as one serial loop, in
//   ISEA order. At such a point A(:,:,JSEA) is all +0 after :2470 and :2538, so
//   the tail writes FACHFA*(+0) = +0 (FACHFA = XFR**(-FACHF-2) > 0,
//   w3gridmd.F90:3564) or nothing: the stale RD2 cannot change a bit while DWN
//   is finite and non-zero.
// - No FMA (contraction off), no libm call.
#pragma once

#include <Kokkos_Core.hpp>

#include "parall_get_jsea_ibelong.hpp"
#include "prop_wavnu1.hpp"
#include "real.hpp"

namespace ww::updt {

/// KDMAX, RDKMIN of :2207 as REAL.
inline constexpr Real kUlevKdmax = static_cast<Real>(4.);
inline constexpr Real kUlevRdkmin = static_cast<Real>(0.05);

/// The Fortran calls of W3ULEV that have no translation. ctx is passed back
/// unchanged.
struct UlevHooks {
  void* ctx;
  /// :2239-2242, NK < 2: IF (IAPROC.EQ.NAPERR) WRITE (NDSE,1000), then
  /// EXTCDE(1) (w3servmd.F90:1064), which does not return in WW3. When the hook
  /// returns, w3ulev returns without touching anything.
  void (*nk_error)(void* ctx);
};

/// The module scalars W3ULEV reads.
struct UlevScalars {
  int nk;        ///< W3GDATMD NK
  int nth;       ///< W3GDATMD NTH
  int nspec;     ///< W3GDATMD NSPEC
  Real dmin;     ///< W3GDATMD DMIN
  Real fachfa;   ///< W3GDATMD FACHFA (w3gdatmd.F90:2513)
  int iaproc;    ///< W3ODATMD IAPROC (1 under W3_SHRD)
  int naproc;    ///< W3ODATMD NAPROC (1 under W3_SHRD)
  bool lpdlib;   ///< CONSTANTS LPDLIB, read by GET_JSEA_IBELONG
  int gtype;     ///< W3GDATMD GTYPE, read by GET_JSEA_IBELONG
};

namespace ulev_detail {
KOKKOS_INLINE_FUNCTION Real fmax2(Real a, Real b) { return (b > a) ? b : a; }
KOKKOS_INLINE_FUNCTION int iabs(int i) { return (i < 0) ? -i : i; }
}  // namespace ulev_detail

/// Sections 2 and 2.a-2.f (:2347-2570), every sea point in ISEA order, run as one
/// work item (header: RD2 is carried across points). Indices as in the launcher.
template <class ExecSpace>
struct UlevSeaLoop {
  using IV2 = Kokkos::View<int**, Kokkos::LayoutLeft, ExecSpace>;
  using RV2 = Kokkos::View<Real**, Kokkos::LayoutLeft, ExecSpace>;
  using RV1 = Kokkos::View<Real*, ExecSpace>;

  UlevScalars s;
  ww::prop::DispTable disp;
  int nsea;
  Kokkos::View<const int**, Kokkos::LayoutLeft, ExecSpace> mapsf;
  Kokkos::View<const Real*, ExecSpace> zb, wlv, dw, sig, dsip;
  Kokkos::View<const int*, ExecSpace> mapwn;
  IV2 mapsta, mapst2, mapdry;
  RV2 wn, cg, va, ta;
  RV1 ust, wno, cgo, own, dwn;

  KOKKOS_INLINE_FUNCTION Real& a(int ith, int ik, int jsea) const {
    return va((ith - 1) + (ik - 1) * s.nth, jsea);
  }

  KOKKOS_INLINE_FUNCTION void zero_va(int jsea) const {
    for (int ispec = 0; ispec < s.nspec; ++ispec) va(ispec, jsea) = 0;
  }

  KOKKOS_INLINE_FUNCTION void jsea_ibelong(int isea, int& jsea, int& ibelong) const {
    ww::parall::get_jsea_ibelong(isea, jsea, ibelong, false, s.lpdlib, s.gtype, s.iaproc,
                                 s.naproc, nullptr, nullptr, nullptr);
  }

  KOKKOS_INLINE_FUNCTION void operator()(const int) const {
    using ulev_detail::fmax2;
    using ulev_detail::iabs;
    const int nk = s.nk;
    const int nth = s.nth;
    const bool local = s.iaproc <= s.naproc;  // :2230
    // The Fortran's RD2 is undefined until a point assigns it; any start value
    // gives the same bits (header).
    Real rd2 = 0;

    for (int isea = 1; isea <= nsea; ++isea) {  // :2347
      const int ix = mapsf(isea - 1, 0);         // :2349
      const int iy = mapsf(isea - 1, 1);         // :2350

      const Real depthbat = fmax2(s.dmin, -zb(isea - 1));  // :2359
      Real wnbat = 0;
      Real cgbat = 0;
      ww::prop::wavnu1(disp, sig(1), depthbat, wnbat, cgbat);  // :2363
      const Real kdchck = wnbat * depthbat;                    // :2365
      if (!(kdchck < kUlevKdmax)) continue;                    // :2366

      const Real depth = fmax2(s.dmin, dw(isea));  // :2370
      for (int ik = 0; ik <= nk + 1; ++ik) {       // :2372-2382
        wno(ik) = wn(ik, isea);
        cgo(ik) = cg(ik, isea);
        ww::prop::wavnu1(disp, sig(ik), depth, wn(ik, isea), cg(ik, isea));
      }
      for (int ik = 1; ik <= nk; ++ik) {  // :2384-2387
        own(ik - 1) = dsip(ik) / cgo(ik);
        dwn(ik - 1) = dsip(ik) / cg(ik, isea);
      }

      int jsea = 0;
      int ibelong = 0;
      const Real dwet = wlv(isea - 1) - zb(isea - 1);  // :2391, reused at :2412
      if (dwet <= static_cast<Real>(0.)) {             // 2.c, :2391-2408
        if (mapdry(iy - 1, ix - 1) == 0) {
          jsea_ibelong(isea, jsea, ibelong);
          if (local && ibelong == 1) zero_va(jsea);
          mapdry(iy - 1, ix - 1) = 1;
          mapsta(iy - 1, ix - 1) = -iabs(mapsta(iy - 1, ix - 1));
        }
        continue;
      }
      if (dwet > static_cast<Real>(0.) && mapdry(iy - 1, ix - 1) == 1) {  // 2.d, :2412-2434
        mapdry(iy - 1, ix - 1) = 0;
        jsea_ibelong(isea, jsea, ibelong);
        if (local && ibelong == 1) zero_va(jsea);
        ust(isea) = static_cast<Real>(0.05);  // :2422
        if (mapst2(iy - 1, ix - 1) == 0) {
          mapsta(iy - 1, ix - 1) = iabs(mapsta(iy - 1, ix - 1));
        }
        continue;
      }

      const Real rdk = Kokkos::fabs(wno(1) - wn(1, isea)) / dwn(0);  // :2438
      if (rdk < kUlevRdkmin || mapsta(iy - 1, ix - 1) < 0) continue;  // :2453
      jsea_ibelong(isea, jsea, ibelong);                              // :2454
      if (ibelong == 0) continue;                                     // :2455
      if (!local) continue;                                           // :2457

      for (int ik = 1; ik <= nk; ++ik) {  // :2461-2468
        for (int ith = 1; ith <= nth; ++ith) {
          ta(ith - 1, ik - 1) = a(ith, ik, jsea) * own(ik - 1);
        }
      }
      zero_va(jsea);  // :2470

      // 2.e, :2479-2499
      bool cond = true;
      int ik0;
      int i1;
      int i2;
      if (wno(1) < wn(1, isea)) {
        ik0 = 1;
        i1 = 0;
        i2 = 1;
        for (;;) {
          ik0 = ik0 + 1;
          if (ik0 > nk + 1) {
            cond = false;
            break;
          }
          if (wno(ik0) >= wn(1, isea)) {
            ik0 = ik0 - 1;
            break;
          }
        }
      } else {
        ik0 = 1;
        i1 = 1;
        i2 = 2;
      }

      if (cond) {                          // :2501-2533
        for (int ik = ik0; ik <= nk; ++ik) {
          while (wno(ik) > wn(i2, isea)) {  // :2504-2508
            i1 = i1 + 1;
            if (i1 > nk) break;
            i2 = i1 + 1;
          }
          if (i1 > nk) continue;  // :2509

          Real rd1;
          if (i1 == 0) {  // :2511-2518
            rd1 = (wn(1, isea) - wno(ik)) / dwn(0);
            rd2 = static_cast<Real>(1.) - rd1;
          } else {
            rd1 = (wn(i2, isea) - wno(ik)) / (wn(i2, isea) - wn(i1, isea));
            rd2 = static_cast<Real>(1.) - rd1;
          }

          if (i1 >= 1) {  // :2520-2524
            for (int ith = 1; ith <= nth; ++ith) {
              a(ith, i1, jsea) = a(ith, i1, jsea) + rd1 * ta(ith - 1, ik - 1);
            }
          }
          if (i2 <= nk) {  // :2526-2530
            for (int ith = 1; ith <= nth; ++ith) {
              a(ith, i2, jsea) = a(ith, i2, jsea) + rd2 * ta(ith - 1, ik - 1);
            }
          }
        }
      }

      for (int ispec = 1; ispec <= s.nspec; ++ispec) {  // 2.f, :2537-2539
        va(ispec - 1, jsea) = va(ispec - 1, jsea) / dwn(mapwn(ispec - 1) - 1);
      }

      if (i2 <= nk && rd2 <= static_cast<Real>(0.95)) {  // :2543-2549
        const int ikstart = (i2 > 2) ? i2 : 2;            // MAX(I2,2)
        for (int ik = ikstart; ik <= nk; ++ik) {
          for (int ith = 1; ith <= nth; ++ith) {
            a(ith, ik, jsea) = s.fachfa * a(ith, ik - 1, jsea);
          }
        }
      }
    }
  }
};

/// W3ULEV(VA, VA) under switch_lab_shrd. Views, LayoutLeft, Fortran bounds kept
/// by index, Fortran 1-based IX, IY values in mapsf:
///   mapsf MAPSF(MSEA,3) [isea-1, 0..1]; mapsta, mapst2 MAPSTA, MAPST2(NY,NX)
///   [iy-1, ix-1] (w3gdatmd.F90:1733-1734); zb ZB(MSEA) [isea-1]
///   (w3gdatmd.F90:1726); wlev WLEV(NX,NY) [ix-1, iy-1] (w3idatmd.F90:628);
///   wlv WLV(NSEA) [isea-1] (w3wdatmd.F90:560); dw DW(0:NSEA) [isea]
///   (w3adatmd.F90:1004); wn, cg WN, CG(0:NK+1,0:NSEA) [ik, isea]
///   (w3adatmd.F90:1312,1325); ust UST(0:NSEATM) [isea] (w3wdatmd.F90:570);
///   sig, dsip SIG, DSIP(0:NK+1) [ik] (w3gdatmd.F90:2066-2068); mapwn
///   MAPWN(NSPEC+NTH) [ispec-1] (w3gdatmd.F90:2058); va VA(NSPEC,0:NSEAL)
///   [ispec-1, jsea].
/// tlev, tln: host pointers to TLEV(2) and TLN(2), the integer dates of :2249.
/// disp: the DISTAB table WAVNU1 reads, its pointers accessible from ExecSpace.
/// The local arrays MAPDRY(NY,NX), WNO, CGO(0:NK+1), OWN, DWN(NK), TA(NTH,NK)
/// (:2202-2206) are allocated here, outside the kernels.
///
/// Why the parallel loops give the serial bits: :2256-2257 and :2574 touch
/// element (IY,IX) only from itself, and :2282-2338 point ISEA only from
/// itself; none has a reduction. :2347-2570 is serial (header). The kernels run
/// in the Fortran's order on one execution space instance.
template <class ExecSpace>
void w3ulev(const ExecSpace& exec, const UlevHooks& hooks, const UlevScalars& s,
            const ww::prop::DispTable& disp, int* tlev, const int* tln,
            Kokkos::View<const int**, Kokkos::LayoutLeft, ExecSpace> mapsf,
            Kokkos::View<int**, Kokkos::LayoutLeft, ExecSpace> mapsta,
            Kokkos::View<int**, Kokkos::LayoutLeft, ExecSpace> mapst2,
            Kokkos::View<const Real*, ExecSpace> zb,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> wlev,
            Kokkos::View<Real*, ExecSpace> wlv, Kokkos::View<Real*, ExecSpace> dw,
            Kokkos::View<Real**, Kokkos::LayoutLeft, ExecSpace> wn,
            Kokkos::View<Real**, Kokkos::LayoutLeft, ExecSpace> cg,
            Kokkos::View<Real*, ExecSpace> ust, Kokkos::View<const Real*, ExecSpace> sig,
            Kokkos::View<const Real*, ExecSpace> dsip,
            Kokkos::View<const int*, ExecSpace> mapwn,
            Kokkos::View<Real**, Kokkos::LayoutLeft, ExecSpace> va) {
  using Policy = Kokkos::RangePolicy<ExecSpace>;
  using Mem = typename ExecSpace::memory_space;

  if (s.nk < 2) {  // :2239-2242
    hooks.nk_error(hooks.ctx);
    return;
  }

  tlev[0] = tln[0];  // :2249
  tlev[1] = tln[1];

  const int ny = static_cast<int>(mapst2.extent(0));
  const int nx = static_cast<int>(mapst2.extent(1));
  const int nsea = static_cast<int>(wlv.extent(0));
  const int nk = s.nk;
  const int nth = s.nth;

  Kokkos::View<int**, Kokkos::LayoutLeft, Mem> mapdry("w3ulev_mapdry", ny, nx);
  Kokkos::View<Real*, Mem> wno("w3ulev_wno", nk + 2);
  Kokkos::View<Real*, Mem> cgo("w3ulev_cgo", nk + 2);
  Kokkos::View<Real*, Mem> own("w3ulev_own", nk);
  Kokkos::View<Real*, Mem> dwn("w3ulev_dwn", nk);
  Kokkos::View<Real**, Kokkos::LayoutLeft, Mem> ta("w3ulev_ta", nth, nk);

  Kokkos::parallel_for(  // 1.c, :2256-2257
      "ww::updt::w3ulev_mapdry", Policy(exec, 0, ny * nx), KOKKOS_LAMBDA(const int n) {
        const int iy = n % ny;
        const int ix = n / ny;
        const int m = mapst2(iy, ix);
        const int d = (m / 2) % 2;
        mapdry(iy, ix) = d;
        mapst2(iy, ix) = m - 2 * d;
      });

  Kokkos::parallel_for(  // 1.d, :2282-2338
      "ww::updt::w3ulev_level", Policy(exec, 0, nsea), KOKKOS_LAMBDA(const int i) {
        const int ix = mapsf(i, 0) - 1;  // :2283-2284
        const int iy = mapsf(i, 1) - 1;
        wlv(i) = wlev(ix, iy);           // :2324
        const Real wlveff = wlv(i);      // :2325
        dw(i + 1) = ulev_detail::fmax2(static_cast<Real>(0.), wlveff - zb(i));  // :2336
      });

  UlevSeaLoop<ExecSpace> loop{s,    disp, nsea, mapsf, zb, wlv, dw,  sig, dsip, mapwn, mapsta,
                              mapst2, mapdry, wn, cg, va, ta, ust, wno, cgo, own, dwn};
  Kokkos::parallel_for("ww::updt::w3ulev_sea_loop", Policy(exec, 0, 1), loop);  // :2347-2570

  Kokkos::parallel_for(  // 3, :2574
      "ww::updt::w3ulev_mapst2", Policy(exec, 0, ny * nx), KOKKOS_LAMBDA(const int n) {
        const int iy = n % ny;
        const int ix = n / ny;
        mapst2(iy, ix) = mapst2(iy, ix) + 2 * mapdry(iy, ix);
      });
}

}  // namespace ww::updt
