// kokkos/src/ww_kokkos/updt_ubpt.hpp
// W3UBPT, ported to Kokkos: the spectra at the active boundary points, as the
// space interpolation of the two boundary spectra W3IOBC read (ABPI0, ABPIN),
// converted from sigma spectra to the model's action by CG/SIG2. Phase 1,
// translated, not wired, not regtested.
//
// WW3 heritage: WW3/model/src/w3updtmd.F90:1329-1545 at h0ffmann/WW3 96892c95
// (byte-identical to NOAA-EMC/WW3 03adda91: git -C WW3 diff 03adda91 HEAD --
// model/src/w3updtmd.F90 is empty); author H. L. Tolman (RTD part J.-G. Li,
// C. Hansen). Called from W3WAVE after W3IOBC (w3wavemd.F90:1344 at the pin).
// Evidence: kokkos/port_report/W3UBPT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Only the switch_lab_shrd branch is ported: no PDLIB, so the #else branches
// with CG(MAPWN(ISP),ISEA) (:1461-1465, :1487-1491). Not compiled there and not
// ported: PDLIB (no CG factor), RTD (the W3ACTURN rotation, :1496-1506), T0
// (the wave-height test output, :1513-1532, a print), S.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3updtmd.F90 (WW3's own flags, switch_lab_shrd; the recompiled object's
// disassembly equals the CMake build's), 2026-11-12 (v):
// - :1445 BBPI0(1,0) .EQ. -1. is an ordered float compare with -1.0.
// - :1461-1465 and :1487-1491 are (CG/SIG2) * PAREN(((R1*A1 + R2*A2) + R3*A3)
//   + R4*A4): one divss, four mulss, three addss left to right, one mulss; the
//   parentheses are kept (PAREN_EXPR in GIMPLE). 18 scalar FP instructions in
//   the routine, nine per loop; no FMA, no reciprocal.
// - Operand positions: each Fortran RDBPI*ABPI product has RDBPI first (it stays
//   in a register over the ISP loop); the C++ kernel has ABPI first, which
//   neither operand order in the source nor an ISP-inner iteration changes. Only
//   the payload of a product of two NaNs (a NaN weight RDBPI and a NaN spectrum)
//   can differ ⚠; the sums, the division and the last product have the Fortran's
//   positions.
// - :1447-1450 the column-0 fills and :1473 BBPI0 = BBPIN (through a malloc'd
//   temporary, as gfortran cannot prove BBPI0 and BBPIN distinct) copy bits.
// - No loop of :1329-1545 is vectorised (vec.txt has no line there).
// No libm call, so the GPU caveat of the transcendental functions does not
// apply; on CUDA the division and products are IEEE with --fmad=false ⚠ (not
// compiled for CUDA here).
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::updt {

/// :1461-1465 (and :1487-1491) for one (ISP, IBI): cg = CG(MAPWN(ISP),ISEA),
/// sig2 = SIG2(ISP), r1..r4 = RDBPI(IBI,1:4), a1..a4 = ABPI*(ISP,IPBPI(IBI,1:4)).
KOKKOS_INLINE_FUNCTION Real w3ubpt_point(Real cg, Real sig2, Real r1, Real a1, Real r2, Real a2,
                                         Real r3, Real a3, Real r4, Real a4) {
  const Real s = ((r1 * a1 + r2 * a2) + r3 * a3) + r4 * a4;
  return (cg / sig2) * s;
}

/// W3UBPT under switch_lab_shrd. Module state comes in as Views, LayoutLeft,
/// 0-based, Fortran index values kept 1-based where they are data:
///   isbpi   ISBPI(NBI), w3odatmd.F90:1450, values ISEA (1-based)
///   ipbpi   IPBPI(NBI,4), w3odatmd.F90:1449, values 0..NBI2 (columns of ABPI0/N)
///   rdbpi   RDBPI(NBI,4), w3odatmd.F90:1453
///   mapwn   MAPWN(MSPEC+MTH), w3gdatmd.F90:2058, values IK (1-based)
///   sig2    SIG2(MSPEC), w3gdatmd.F90:2067, element ISP-1
///   cg      CG(0:NK+1,0:NSEA), w3adatmd.F90:1325, element (IK, ISEA)
///   abpi0, abpin  ABPI0/ABPIN(NSPEC,0:NBI2), w3odatmd.F90:1484-1485, element (ISP-1, J)
///   bbpi0, bbpin  BBPI0/BBPIN(NSPEC,0:NBI), w3odatmd.F90:1486-1487, element (ISP-1, J)
/// nspec is NSPEC; NBI is isbpi.extent(0).
///
/// The test of :1445 reads one REAL of BBPI0 back to the host (4 bytes); the
/// boundary arrays are not VA, and nothing else crosses.
///
/// Why parallel gives the serial bits: each (ISP, IBI) writes only
/// BBPI0/BBPIN(ISP,IBI) from inputs nothing in the same pass writes; there is no
/// reduction and no accumulation across (ISP, IBI). The passes run in the
/// Fortran's order on one execution space instance: the column-0 fills before
/// the BBPI0 pass that reads ABPI0(:,0) through IPBPI = 0, and BBPI0 = BBPIN
/// before the BBPIN pass overwrites BBPIN.
template <class ExecSpace>
void w3ubpt(const ExecSpace& exec, int nspec,
            Kokkos::View<const int*, ExecSpace> isbpi,
            Kokkos::View<const int**, Kokkos::LayoutLeft, ExecSpace> ipbpi,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> rdbpi,
            Kokkos::View<const int*, ExecSpace> mapwn, Kokkos::View<const Real*, ExecSpace> sig2,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> cg,
            Kokkos::View<Real**, Kokkos::LayoutLeft, ExecSpace> abpi0,
            Kokkos::View<Real**, Kokkos::LayoutLeft, ExecSpace> abpin,
            Kokkos::View<Real**, Kokkos::LayoutLeft, ExecSpace> bbpi0,
            Kokkos::View<Real**, Kokkos::LayoutLeft, ExecSpace> bbpin) {
  const int nbi = static_cast<int>(isbpi.extent(0));
  using Policy = Kokkos::MDRangePolicy<ExecSpace, Kokkos::Rank<2, Kokkos::Iterate::Left>>;
  const Policy pts(exec, {0, 0}, {nspec, nbi});

  Kokkos::View<Real, Kokkos::HostSpace> first("ww::updt::w3ubpt_bbpi0_10");
  Kokkos::deep_copy(exec, first, Kokkos::subview(bbpi0, 0, 0));
  exec.fence();

  if (first() == static_cast<Real>(-1.)) {  // :1445
    Kokkos::deep_copy(exec, Kokkos::subview(bbpi0, Kokkos::ALL, 0), Real(0));  // :1447-1450
    Kokkos::deep_copy(exec, Kokkos::subview(bbpin, Kokkos::ALL, 0), Real(0));
    Kokkos::deep_copy(exec, Kokkos::subview(abpi0, Kokkos::ALL, 0), Real(0));
    Kokkos::deep_copy(exec, Kokkos::subview(abpin, Kokkos::ALL, 0), Real(0));
    Kokkos::parallel_for(  // :1452-1468
        "ww::updt::w3ubpt_bbpi0", pts, KOKKOS_LAMBDA(const int isp, const int ib) {
          const int isea = isbpi(ib);  // :1453
          bbpi0(isp, ib + 1) = w3ubpt_point(
              cg(mapwn(isp), isea), sig2(isp), rdbpi(ib, 0), abpi0(isp, ipbpi(ib, 0)),
              rdbpi(ib, 1), abpi0(isp, ipbpi(ib, 1)), rdbpi(ib, 2), abpi0(isp, ipbpi(ib, 2)),
              rdbpi(ib, 3), abpi0(isp, ipbpi(ib, 3)));
        });
  } else {
    Kokkos::deep_copy(exec, bbpi0, bbpin);  // :1473
  }

  Kokkos::parallel_for(  // :1478-1507
      "ww::updt::w3ubpt_bbpin", pts, KOKKOS_LAMBDA(const int isp, const int ib) {
        const int isea = isbpi(ib);  // :1479
        bbpin(isp, ib + 1) = w3ubpt_point(
            cg(mapwn(isp), isea), sig2(isp), rdbpi(ib, 0), abpin(isp, ipbpi(ib, 0)),
            rdbpi(ib, 1), abpin(isp, ipbpi(ib, 1)), rdbpi(ib, 2), abpin(isp, ipbpi(ib, 2)),
            rdbpi(ib, 3), abpin(isp, ipbpi(ib, 3)));
      });
}

}  // namespace ww::updt
