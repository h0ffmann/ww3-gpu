// kokkos/src/ww_kokkos/pdlib_w3xypug.hpp
// PDLIB_W3XYPUG: the driver of the explicit unstructured-grid schemes for one spectral
// component. Gathers VA into AC, builds C, calls FSN2 / FSPSI2 / FSFCT2, stores VA back.
// Phase 1, translated, not regtested. WW3 heritage: WW3-noaa/model/src/w3profsmd_pdlib.F90:626-866
// at NOAA-EMC/WW3 03adda91; A. Roland, M. Dutour-Sikiric. kRef1 only reaches the schemes (the
// routine's own object is the same with REF1). Not ported: W3_S, W3_DEBUGSOLVER, W3_MGP (no PDLIB
// switch has them) and NOCGTABLE (no W3_ prefix, CMake never defines it). Evidence:
// kokkos/port_report/PDLIB_W3XYPUG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "pdlib_fsfct2.hpp"
#include "pdlib_fsn2.hpp"
#include "pdlib_fspsi2.hpp"
#include "pdlib_params.hpp"
#include "real.hpp"

namespace ww::pdlib {

/// Run-time flags the routine reads from modules, 0/1 (LOGICAL).
struct XypugFlags {
  int fsn = 0, fspsi = 0, fsfct = 0, fsnimp = 0;  ///< W3GDATMD, :830-838
  int flcur = 0;                                  ///< W3IDATMD, :787
  int mapsta_hack = 0;  ///< module variable, .FALSE. and never set (:113)
};

/// The W3WDATMD / W3ADATMD arrays, lower bounds kept: va(isp-1, jsea), cg(ik, isea), cx(isea).
template <class MemSpace>
struct XypugFields {
  Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace> va;        ///< VA(NSPEC,0:NSEALM)
  Kokkos::View<const Real**, Kokkos::LayoutLeft, MemSpace> cg;  ///< CG(0:NK+1,0:NSEA)
  Kokkos::View<const Real*, Kokkos::LayoutLeft, MemSpace> cx;   ///< CX(0:NSEA)
  Kokkos::View<const Real*, Kokkos::LayoutLeft, MemSpace> cy;   ///< CY(0:NSEA)
  int nseal = 0;                                                ///< NSEAL <= npa
};

/// The automatic arrays (:724-731).
template <class MemSpace>
struct XypugWork {
  using W1 = Kokkos::View<Real*, Kokkos::LayoutLeft, MemSpace>;
  Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace> c;  ///< C(npa,2)
  W1 vlcflx, vlcfly, ac;                                 ///< (npa)
  W1 ac_map;                                             ///< AC_MAP(NBND_MAP)

  static XypugWork allocate(int npa, int nbnd_map) {
    XypugWork w;
    w.c = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>("xypug_c", npa, 2);
    w.vlcflx = W1("xypug_vlcflx", npa);
    w.vlcfly = W1("xypug_vlcfly", npa);
    w.ac = W1("xypug_ac", npa);
    w.ac_map = W1("xypug_ac_map", nbnd_map);
    return w;
  }
};

/// The schemes' work arrays and PDLIB_TRIA (FSFCT2 only).
template <class MemSpace>
struct XypugSchemeWork {
  Fsn2Work<MemSpace> fsn2;
  Fspsi2Work<MemSpace> fspsi2;
  Fsfct2Work<MemSpace> fsfct2;
  Fsfct2TriaView<MemSpace> tria;
};

/// kXypugStopFsnimp: the Fortran reaches STOP 'For PDLIB and FSNIMP, no function has been
/// programmed yet' (:837) with VA untouched; the shim stops.
enum XypugStatus : int { kXypugDone = 0, kXypugStopFsnimp = 1 };

/// PDLIB_W3XYPUG(ISP, FACX, FACY, DTG, VGX, VGY, LCALC); VGX/VGY are read only under W3_MGP.
/// Host callables, called in the Fortran's order: boundary_times(rd1, rd2) sets
/// RD1 = DSEC21(TBPI0,TIME), RD2 = DSEC21(TBPI0,TBPIN) (:808-809), only when FLBPI;
/// allreduce_min and exchange go to the scheme (their headers). refpars3: REFPARS(3), kRef1 only.
template <bool kRef1 = false, class ExecSpace, class BoundaryTimes, class AllreduceMin,
          class Exchange>
XypugStatus pdlib_w3xypug(const ExecSpace& exec, int isp, Real facx, Real facy, Real dtg,
                          bool lcalc, const XypugFlags& flags,
                          const PdlibGeometry<typename ExecSpace::memory_space>& g,
                          const PdlibState<typename ExecSpace::memory_space>& s,
                          const PdlibBoundary<typename ExecSpace::memory_space>& b,
                          const XypugFields<typename ExecSpace::memory_space>& f,
                          const XypugWork<typename ExecSpace::memory_space>& w,
                          const XypugSchemeWork<typename ExecSpace::memory_space>& sw,
                          BoundaryTimes&& boundary_times, AllreduceMin&& allreduce_min,
                          Exchange&& exchange, Real refpars3 = kZero) {
  using Policy = Kokkos::RangePolicy<ExecSpace>;
  const int ith = 1 + (isp - 1) % g.nth;  // :745
  const int ik = 1 + (isp - 1) / g.nth;   // :746
  const int isp0 = isp - 1;
  const auto iplg = g.iplg;
  const auto mapfs = g.mapfs;
  const auto clats = g.clats;
  const auto ecos = g.ecos;
  const auto esin = g.esin;
  const auto va = f.va;
  const auto cg = f.cg;
  const auto vlcflx = w.vlcflx;
  const auto vlcfly = w.vlcfly;
  const auto ac = w.ac;

  Kokkos::deep_copy(exec, vlcflx, kZero);  // :754-756
  Kokkos::deep_copy(exec, vlcfly, kZero);
  Kokkos::deep_copy(exec, ac, kZero);

  // :760-778. CCOS*CG/CLATS is (CCOS*CG)/CLATS; the object commutes CG*CCOS (same bits).
  Kokkos::parallel_for(
      "pdlib_xypug_velocities", Policy(exec, 1, f.nseal + 1), KOKKOS_LAMBDA(const int jsea) {
        const Real ccos = facx * ecos(ith - 1);  // :747
        const Real csin = facy * esin(ith - 1);  // :748
        const int ip = jsea;
        const int isea = mapfs(0, iplg(ip - 1) - 1);
        const Real cgv = cg(ik, isea);
        ac(ip - 1) = va(isp0, jsea) / cgv * clats(isea);  // :770
        vlcflx(ip - 1) = ccos * cgv / clats(isea);         // :771
        vlcfly(ip - 1) = csin * cgv;                       // :772
      });

  if (flags.flcur) {  // :787-800
    const auto iobp_loc = g.iobp_loc;
    const auto cx = f.cx;
    const auto cy = f.cy;
    Kokkos::parallel_for(
        "pdlib_xypug_currents", Policy(exec, 1, f.nseal + 1), KOKKOS_LAMBDA(const int jsea) {
          const int ip = jsea;
          const int isea = mapfs(0, iplg(ip - 1) - 1);
          if (iobp_loc(ip - 1) > 0) {
            vlcflx(ip - 1) = vlcflx(ip - 1) + facx * cx(isea) / clats(isea);  // :796, CCURX = FACX
            vlcfly(ip - 1) = vlcfly(ip - 1) + facy * cy(isea);                // :797, CCURY = FACY
          }
        });
  }

  {  // :802-803, REAL(IOBDP_LOC) times VLCFL, element-wise (the vector lanes commute *)
    const auto c = w.c;
    const auto iobdp = g.iobdp_loc;
    Kokkos::parallel_for(
        "pdlib_xypug_c", Policy(exec, 0, g.npa), KOKKOS_LAMBDA(const int ip0) {
          c(ip0, 0) = vlcflx(ip0) * static_cast<Real>(iobdp(ip0));
          c(ip0, 1) = vlcfly(ip0) * static_cast<Real>(iobdp(ip0));
        });
  }

  Real rd1 = 1.0f;  // :811-812
  Real rd2 = kZero;
  if (b.flbpi) boundary_times(rd1, rd2);  // :807-809, DSEC21 in the shim

  const int nbnd_map = static_cast<int>(g.index_map.extent(0));
  const auto index_map = g.index_map;
  const auto ac_map = w.ac_map;
  if (flags.mapsta_hack) {  // :817-822
    Kokkos::parallel_for(
        "pdlib_xypug_map_save", Policy(exec, 0, nbnd_map),
        KOKKOS_LAMBDA(const int i) { ac_map(i) = ac(index_map(i) - 1); });
  }
  exec.fence();

  if (flags.fsn) {  // :830-838, the first true flag wins
    w3xypfsn2<kRef1>(exec, g, s, b, sw.fsn2, isp, w.c, lcalc, rd1, rd2, dtg, ac, allreduce_min,
                     exchange, refpars3);
  } else if (flags.fspsi) {
    pdlib_w3xypfspsi2<kRef1>(exec, isp, w.c, lcalc, rd1, rd2, dtg, ac, g, s, b, sw.fspsi2,
                             allreduce_min, exchange, refpars3);
  } else if (flags.fsfct) {
    w3xypfsfct2<kRef1>(exec, g, s, b, sw.fsfct2, sw.tria, isp, w.c, lcalc, rd1, rd2, dtg, ac,
                       allreduce_min, exchange, refpars3);
  } else if (flags.fsnimp) {
    return kXypugStopFsnimp;  // :837
  }

  if (flags.mapsta_hack) {  // :840-845, one thread: a repeated INDEX_MAP keeps the last store
    Kokkos::parallel_for(
        "pdlib_xypug_map_restore", Policy(exec, 0, 1), KOKKOS_LAMBDA(const int) {
          for (int i = 0; i < nbnd_map; ++i) ac(index_map(i) - 1) = ac_map(i);
        });
  }

  // :853-858. maxss %xmm1(+0),%xmm0(x): x > 0 ? x : +0, so NaN and -0 store +0.
  Kokkos::parallel_for(
      "pdlib_xypug_store", Policy(exec, 1, f.nseal + 1), KOKKOS_LAMBDA(const int jsea) {
        const int ip = jsea;
        const int isea = mapfs(0, iplg(ip - 1) - 1);
        const Real x = cg(ik, isea) / clats(isea) * ac(ip - 1);
        va(isp0, jsea) = (x > kZero) ? x : kZero;
      });
  exec.fence();
  return kXypugDone;
}

}  // namespace ww::pdlib
