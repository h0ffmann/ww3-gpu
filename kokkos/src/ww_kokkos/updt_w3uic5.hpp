// kokkos/src/ww_kokkos/updt_w3uic5.hpp
// W3UIC5, ported to Kokkos: ice floe diameters ICEF and ICEDMAX of each sea point
// from the input field ICEP5, or 0 and 1000 where there is no ice. Phase 1,
// translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3updtmd.F90:1670-1786 at NOAA-EMC/WW3 03adda91;
// authors C. Sevigny, F. Ardhuin. Called from W3WAVE (w3wavemd.F90:1407), only
// under W3_IS2. FLFRST is not read. No switch changes its arithmetic: W3_T only
// prints (:1753-1755), and a kernel never prints; the object under
// switch_lab_shrd and under lab + NL4 IC5 BT9 NLS is the same.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3updtmd.F90 (WW3's own flags): no floating-point operation, only two ucomiss
// against 0 and copies. .OR. short-circuits (ICEH is read only when ICE /= 0), a
// NaN compares unequal, -0 equals 0; ICEP5(IX,IY) is loaded once and stored to
// ICEF then ICEDMAX. The loop :1761-1773 is not vectorised.
//
// Evidence: kokkos/port_report/W3UIC5_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::updt {

/// FLFLOE of :1765 for one ISEA.
KOKKOS_INLINE_FUNCTION bool w3uic5_flfloe(Real ice, Real iceh) {
  return ice == Real(0) || iceh == Real(0);
}

/// W3UIC5(FLFRST). tic5 and ti5 are TIC5(2) and TI5(2) (date, time), host arrays.
/// mapsf is MAPSF(NSEA,3) (LayoutLeft, Fortran 1-based IX, IY values), icep5 is
/// ICEP5(NX,NY) (w3idatmd.F90:610), LayoutLeft. ice, iceh and icef have extent
/// NSEA and element ISEA-1: the caller passes ICE/ICEH/ICEF(1:NSEA) of the 0:NSEA
/// arrays (w3wdatmd.F90:561, :567, :568); icedmax is ICEDMAX(1:NSEA) (:569).
///
/// Why parallel gives the serial bits: point ISEA reads only inputs and writes
/// only ICEF(ISEA) and ICEDMAX(ISEA); no reduction, no cross-point read.
template <class ExecSpace>
void w3uic5(const ExecSpace& exec, int* tic5, const int* ti5,
            Kokkos::View<const int**, Kokkos::LayoutLeft, ExecSpace> mapsf,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> icep5,
            Kokkos::View<const Real*, ExecSpace> ice, Kokkos::View<const Real*, ExecSpace> iceh,
            Kokkos::View<Real*, ExecSpace> icef, Kokkos::View<Real*, ExecSpace> icedmax) {
  tic5[0] = ti5[0];  // :1756-1757
  tic5[1] = ti5[1];
  const int nsea = static_cast<int>(icef.extent(0));
  Kokkos::parallel_for(  // :1761-1773
      "ww::updt::w3uic5", Kokkos::RangePolicy<ExecSpace>(exec, 0, nsea),
      KOKKOS_LAMBDA(const int i) {
        if (w3uic5_flfloe(ice(i), iceh(i))) {
          icef(i) = Real(0);
          icedmax(i) = Real(1000);
        } else {
          const Real p = icep5(mapsf(i, 0) - 1, mapsf(i, 1) - 1);  // :1763-1764, :1770-1771
          icef(i) = p;
          icedmax(i) = p;
        }
      });
}

}  // namespace ww::updt
