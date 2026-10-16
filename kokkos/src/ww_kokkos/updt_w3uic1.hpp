// kokkos/src/ww_kokkos/updt_w3uic1.hpp
// W3UIC1, ported to Kokkos: ice thickness ICEH of each sea point from the input
// field ICEP1, ICEH = MAX(IICEHMIN, IICEHFAC*ICEP1). Phase 1, translated, not
// regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3updtmd.F90:1555-1659 at NOAA-EMC/WW3 03adda91;
// author C. Sevigny. Called from W3WAVE (w3wavemd.F90:1381). FLFRST is not read
// (:1547). No switch changes its arithmetic: W3_T only prints (:1635-1637), and a
// kernel never prints.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3updtmd.F90 (WW3's own flags, switch_lab_shrd): :1647 is one scalar
// mulss (ICEP1 in the register, times IICEHFAC) then maxss IICEHMIN, so a NaN
// or a tie gives IICEHMIN (fmax2 below). The loop :1643-1648 is not vectorised.
//
// Evidence: kokkos/port_report/W3UIC1_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::updt {

namespace uic1_detail {
// Fortran MAX of two REALs as maxss computes it: fmax2(a,b) = b>a?b:a, so a NaN
// or a tie gives `a`, the memory operand of the build's maxss.
KOKKOS_INLINE_FUNCTION Real fmax2(Real a, Real b) { return (b > a) ? b : a; }
}  // namespace uic1_detail

/// :1647 for one ISEA, icep1 = ICEP1(IX,IY) with (IX,IY) = MAPSF(ISEA,1:2).
KOKKOS_INLINE_FUNCTION Real w3uic1_point(Real iicehmin, Real iicehfac, Real icep1) {
  return uic1_detail::fmax2(iicehmin, icep1 * iicehfac);
}

/// W3UIC1(FLFRST). tic1 and ti1 are TIC1(2) and TI1(2) (date, time), host arrays.
/// mapsf is MAPSF(NSEA,3) (LayoutLeft, Fortran 1-based IX, IY values), icep1 is
/// ICEP1(NX,NY) (w3idatmd.F90:594), LayoutLeft. iceh has extent NSEA and element
/// ISEA-1: the caller passes ICEH(1:NSEA) of the 0:NSEA array (w3wdatmd.F90:567).
///
/// Why parallel gives the serial bits: point ISEA reads only inputs and writes
/// only ICEH(ISEA); no reduction, no cross-point read.
template <class ExecSpace>
void w3uic1(const ExecSpace& exec, int* tic1, const int* ti1, Real iicehmin, Real iicehfac,
            Kokkos::View<const int**, Kokkos::LayoutLeft, ExecSpace> mapsf,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> icep1,
            Kokkos::View<Real*, ExecSpace> iceh) {
  tic1[0] = ti1[0];  // :1638-1639
  tic1[1] = ti1[1];
  const int nsea = static_cast<int>(iceh.extent(0));
  Kokkos::parallel_for(  // :1643-1648
      "ww::updt::w3uic1", Kokkos::RangePolicy<ExecSpace>(exec, 0, nsea),
      KOKKOS_LAMBDA(const int i) {
        const int ix = mapsf(i, 0) - 1;  // :1645-1646
        const int iy = mapsf(i, 1) - 1;
        iceh(i) = w3uic1_point(iicehmin, iicehfac, icep1(ix, iy));
      });
}

}  // namespace ww::updt
