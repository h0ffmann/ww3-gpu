// kokkos/src/ww_kokkos/updt_w3urho.hpp
// W3URHO, ported to Kokkos: air density at the model time by linear
// interpolation between the two input fields. Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3updtmd.F90:2626-2791 at NOAA-EMC/WW3 03adda91;
// author J. M. Castillo. Called from W3WAVE when FLRHOA (w3wavemd.F90:1277-1278).
// Evidence: kokkos/port_report/W3URHO_KOKKOS.md.
//
// Variants: SMC with FSWND (:2731-2739, RH0 is RH0(NSEA,1), w3idatmd.F90:726-728)
// is the `fswnd` argument; the time factor RD (:2752-2763) is UrhoTime: WNT0 gives
// 0, WNT1 and WNT2 the same DT0T/MAX(1.E-7,DT0N), OASACM (set after the WNT line,
// so it wins) gives 1. OMPG is not translated: its PRIVATE(ISEA,RA0,RAI) (:2772)
// privatises the module pointers RA0, RAI, whose association is then undefined in
// each thread; the C++ runs the serial semantics. S and T are output only.
// DSEC21 (w3timemd.F90:330) is integer and double time arithmetic on the host: the
// caller passes DT0N = DSEC21(TR0,TRN) and DT0T = DSEC21(TR0,TIME) as the REALs the
// Fortran stores (:2749-2750).
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3updtmd.F90 (WW3's flags, switch_lab_shrd and switch_SMCMlt):
// - RAI = RHN - RH0 is one subss. RHOAIR = RA0 + RD*RAI is mulss with RAI as
//   the first operand, then addss with the product first (w3uwnd_interp). g++
//   puts RD first in the mulss whichever way the C++ is written: the bits differ
//   only when RD and RAI are both NaN (the payload x86 keeps) ⚠.
// - MAX(1.E-7,DT0N) is maxss 1.E-7,DT0N: a NaN DT0N gives 1.E-7 (w3uwnd_rd).
// - Neither loop of :2626-2791 is vectorised; no FMA, no libm call.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "updt_uwnd.hpp"

namespace ww::updt {

/// The #ifdef that sets RD (:2752-2763).
enum class UrhoTime { wnt0, wnt1, wnt2, oasacm };

/// RD of :2752-2763 under the given switch.
template <UrhoTime V>
KOKKOS_INLINE_FUNCTION Real w3urho_rd(Real dt0t, Real dt0n) {
  if constexpr (V == UrhoTime::oasacm) {
    return static_cast<Real>(1.);
  } else if constexpr (V == UrhoTime::wnt0) {
    return static_cast<Real>(0.);
  } else {
    return w3uwnd_rd(dt0t, dt0n);
  }
}

/// :2742-2743 for one ISEA: RA0 = RH0, RAI = RHN - RH0.
KOKKOS_INLINE_FUNCTION void w3urho_first_point(Real rh0, Real rhn, Real* ra0, Real* rai) {
  *ra0 = rh0;
  *rai = rhn - rh0;
}

/// :2777 for one ISEA.
KOKKOS_INLINE_FUNCTION Real w3urho_point(Real ra0, Real rd, Real rai) {
  return w3uwnd_interp(ra0, rd, rai);
}

/// W3URHO(FLFRST). ra0, rai, rhoair have extent NSEA, element ISEA-1
/// (w3adatmd.F90:1364-1365, w3wdatmd.F90:562). mapsf is MAPSF(NSEA,3) with
/// Fortran 1-based IX, IY values; rh0, rhn are RH0(NX,NY), or RH0(NSEA,1) when
/// fswnd (SMC only; pass false in a build without SMC), LayoutLeft, Fortran order.
///
/// Why parallel gives the serial bits: point ISEA reads only inputs and its own
/// entries and writes only its own entries; no sum across points. The two loops
/// run in the Fortran's order on one execution space instance.
template <UrhoTime V, class ExecSpace>
void w3urho(const ExecSpace& exec, bool flfrst, Real dt0n, Real dt0t, bool fswnd,
            Kokkos::View<const int**, Kokkos::LayoutLeft, ExecSpace> mapsf,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> rh0,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> rhn,
            Kokkos::View<Real*, ExecSpace> ra0, Kokkos::View<Real*, ExecSpace> rai,
            Kokkos::View<Real*, ExecSpace> rhoair) {
  const int nsea = static_cast<int>(rhoair.extent(0));
  using Policy = Kokkos::RangePolicy<ExecSpace>;

  if (flfrst) {  // :2726-2745
    Kokkos::parallel_for(
        "ww::updt::w3urho_first", Policy(exec, 0, nsea), KOKKOS_LAMBDA(const int i) {
          int ix;
          int iy;
          if (fswnd) {  // :2731-2733
            ix = i;
            iy = 0;
          } else {  // :2736-2737
            ix = mapsf(i, 0) - 1;
            iy = mapsf(i, 1) - 1;
          }
          Real a = 0;
          Real d = 0;
          w3urho_first_point(rh0(ix, iy), rhn(ix, iy), &a, &d);
          ra0(i) = a;
          rai(i) = d;
        });
  }

  const Real rd = w3urho_rd<V>(dt0t, dt0n);  // host scalar

  Kokkos::parallel_for(  // :2775-2779
      "ww::updt::w3urho", Policy(exec, 0, nsea),
      KOKKOS_LAMBDA(const int i) { rhoair(i) = w3urho_point(ra0(i), rd, rai(i)); });
}

}  // namespace ww::updt
