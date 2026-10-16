// kokkos/src/ww_kokkos/updt_uwnd.hpp
// W3UWND, ported to Kokkos: wind at the model time by linear interpolation of
// speed, direction and air-sea temperature difference between the two input
// fields, then the 10 m wind relative to the surface current. Phase 1,
// translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3updtmd.F90:488-822 at NOAA-EMC/WW3 03adda91;
// author H. L. Tolman. Called from W3WAVE (w3wavemd.F90:1233). Only the
// switch_lab_shrd branch is ported: WNT1 (RD = DT0T/MAX(1.E-7,DT0N), :684) and RWND
// (the current correction, :746-770). Not compiled there and not ported: WNT0,
// WNT2, OASACM, MGW (so VGX, VGY are unused), SMC, WCOR, STAB2, OMPG, S, T.
// DSEC21 (w3timemd.F90) is integer and double time arithmetic on the host: the
// caller computes DT0N = DSEC21(TW0,TWN) and DT0T = DSEC21(TW0,TIME) and passes
// them as the REALs the Fortran stores (:677-678).
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3updtmd.F90 (WW3's own flags, switch_lab_shrd):
// - SQRT(x**2+y**2) is (x*x)+(y*y) then __builtin_sqrtf (sqrtss), not hypotf.
// - ATAN2 is a call of libm atan2f; COS/SIN of UD are one sincosf call
//   (__builtin_cexpif).
// - MOD(a,TPI) is __builtin_fmodf, expanded inline as an x87 fprem loop (exact
//   remainder, then fstps). g++ expands std::fmod to the same loop and, under
//   -fmath-errno, calls fmodf on a NaN result only to set errno: the value it
//   returns is still the fprem one, so the bits match for every input.
// - SIGN(1.,DD) is copysignf(1.,DD); ABS is a mask.
// - MAX operand order is the build's maxss (see fmax2): MAX(1.E-7,DT0N) is
//   maxss 1.E-7,DT0N; MAX(0.001,SQRT(..)) and MAX(UA,0.001) are maxss 0.001,x.
//   So a NaN gives 1.E-7 or 0.001, as in the Fortran.
// - No loop of :488-822 is vectorised (w3updtmd.vec.txt has no line there), so
//   no libmvec variant is involved; contraction is off (no FMA).
//
// On a GPU atan2, fmod, sin and cos are CUDA's, not glibc's: their bits are not
// shown to match the Fortran's (AGENTS.md, "Bit for bit") ⚠.
#pragma once

#include <cmath>

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::updt {

// constants.F90:72-73: PI = 3.141592653589793 as REAL, TPI = 2.0*PI in REAL (exact
// doubling). GIMPLE literals 3.1415927410125732421875 and 6.283185482025146484375.
inline constexpr Real kUwndPi = static_cast<Real>(3.141592653589793);
inline constexpr Real kUwndTpi = static_cast<Real>(2.0) * kUwndPi;

namespace uwnd_detail {
// Fortran MAX of two REALs as maxss computes it: fmax2(a,b) = b>a?b:a, so a NaN
// or a tie gives `a`, the source operand of the build's maxss.
KOKKOS_INLINE_FUNCTION Real fmax2(Real a, Real b) { return (b > a) ? b : a; }

KOKKOS_INLINE_FUNCTION Real atan2_r(Real y, Real x) {
  Real r = 0;
  KOKKOS_IF_ON_HOST((r = ::atan2f(y, x);))
  KOKKOS_IF_ON_DEVICE((r = Kokkos::atan2(y, x);))
  return r;
}

// MOD(a, TPI) (header: the host expansion is the Fortran's fprem loop).
KOKKOS_INLINE_FUNCTION Real mod_tpi(Real a) {
  Real r = 0;
  KOKKOS_IF_ON_HOST((r = std::fmod(a, kUwndTpi);))
  KOKKOS_IF_ON_DEVICE((r = Kokkos::fmod(a, kUwndTpi);))
  return r;
}

KOKKOS_INLINE_FUNCTION void sincos_r(Real x, Real* s, Real* c) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((::sincosf(x, s, c);))
  KOKKOS_IF_ON_DEVICE((*c = Kokkos::cos(x); *s = Kokkos::sin(x);))
#else
  *c = Kokkos::cos(x);
  *s = Kokkos::sin(x);
#endif
}
}  // namespace uwnd_detail

/// The FLFRST outputs of one sea point.
struct UwndFirst {
  Real ua0, uai, ud0, udi, as0, asi;
};

/// :643-671 for one ISEA, from WX0..DTN at (IX,IY) = MAPSF(ISEA,1:2).
KOKKOS_INLINE_FUNCTION UwndFirst w3uwnd_first(Real wx0, Real wy0, Real dt0, Real wxn,
                                              Real wyn, Real dtn) {
  using uwnd_detail::atan2_r;
  using uwnd_detail::mod_tpi;
  UwndFirst o{};
  o.ua0 = Kokkos::sqrt(wx0 * wx0 + wy0 * wy0);  // :649
  const Real uai = Kokkos::sqrt(wxn * wxn + wyn * wyn);  // :650
  Real d0;
  if (o.ua0 > static_cast<Real>(1.E-7)) {  // :651-655
    d0 = mod_tpi(kUwndTpi + atan2_r(wy0, wx0));
  } else {
    d0 = 0;
  }
  Real dn;
  if (uai > static_cast<Real>(1.E-7)) {  // :656-660
    dn = mod_tpi(kUwndTpi + atan2_r(wyn, wxn));
  } else {
    dn = d0;
  }
  o.ud0 = (o.ua0 > static_cast<Real>(1.E-7)) ? d0 : dn;  // :661-665
  Real dd = dn - o.ud0;  // :666
  if (Kokkos::fabs(dd) > kUwndPi) {  // :667
    dd = dd - kUwndTpi * Kokkos::copysign(static_cast<Real>(1.), dd);
  }
  o.udi = dd;  // :668
  o.uai = uai - o.ua0;  // :669
  o.as0 = dt0;  // :670
  o.asi = dtn - dt0;  // :671
  return o;
}

/// :684, WNT1. MAX(1.E-7,DT0N) as the build's maxss 1.E-7,DT0N.
KOKKOS_INLINE_FUNCTION Real w3uwnd_rd(Real dt0t, Real dt0n) {
  return dt0t / uwnd_detail::fmax2(static_cast<Real>(1.E-7), dt0n);
}

/// :706, :712, :727: X = X0 + RD*XI (the GIMPLE forms XI*RD, which is the same bits).
KOKKOS_INLINE_FUNCTION Real w3uwnd_interp(Real x0, Real rd, Real xi) { return x0 + rd * xi; }

/// :751-754 (RWND, FLCUR): U10 and U10D of one point.
KOKKOS_INLINE_FUNCTION void w3uwnd_u10_rwnd(Real ua, Real ud, Real rwindc, Real cx, Real cy,
                                            Real* u10, Real* u10d) {
  Real s = 0;
  Real c = 0;
  uwnd_detail::sincos_r(ud, &s, &c);
  const Real uxr = ua * c - rwindc * cx;  // :751
  const Real uyr = ua * s - rwindc * cy;  // :752
  *u10 = uwnd_detail::fmax2(static_cast<Real>(0.001),
                            Kokkos::sqrt(uxr * uxr + uyr * uyr));  // :753
  *u10d = uwnd_detail::mod_tpi(kUwndTpi + uwnd_detail::atan2_r(uyr, uxr));  // :754
}

/// :767, no current: MAX(UA,0.001) as maxss 0.001,UA.
KOKKOS_INLINE_FUNCTION Real w3uwnd_u10(Real ua) {
  return uwnd_detail::fmax2(static_cast<Real>(0.001), ua);
}

/// W3UWND(FLFRST, VGX, VGY) under switch_lab_shrd. All 1-D Views have extent NSEA
/// and element ISEA-1 (the caller passes UA(1:NSEA), CX(1:NSEA), ... of the 0:NSEA
/// arrays of w3adatmd.F90:1008-1016; U10, U10D are 1:NSEA already, w3adatmd.F90:1015,
/// and so are UA0..ASI, w3adatmd.F90:1344-1350). mapsf is MAPSF(NSEA,3) and wx0..dtn are WX0(NX,NY)
/// (w3idatmd.F90:684-689), LayoutLeft, Fortran order, Fortran 1-based IX, IY
/// values in mapsf. dt0n, dt0t are DSEC21's results as REAL (:677-678).
///
/// Why parallel gives the serial bits: in each of the three loops, point ISEA
/// reads only inputs and its own entries and writes only its own entries; there is
/// no reduction and no sum across points, so the order of points cannot change a
/// bit. The loops run in the Fortran's order with a fence between them (one
/// execution space instance, in order).
template <class ExecSpace>
void w3uwnd(const ExecSpace& exec, bool flfrst, Real dt0n, Real dt0t, bool flcur, Real rwindc,
            Kokkos::View<const int**, Kokkos::LayoutLeft, ExecSpace> mapsf,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> wx0,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> wy0,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> dt0,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> wxn,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> wyn,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> dtn,
            Kokkos::View<const Real*, ExecSpace> cx, Kokkos::View<const Real*, ExecSpace> cy,
            Kokkos::View<Real*, ExecSpace> ua0, Kokkos::View<Real*, ExecSpace> uai,
            Kokkos::View<Real*, ExecSpace> ud0, Kokkos::View<Real*, ExecSpace> udi,
            Kokkos::View<Real*, ExecSpace> as0, Kokkos::View<Real*, ExecSpace> asi,
            Kokkos::View<Real*, ExecSpace> ua, Kokkos::View<Real*, ExecSpace> ud,
            Kokkos::View<Real*, ExecSpace> as, Kokkos::View<Real*, ExecSpace> u10,
            Kokkos::View<Real*, ExecSpace> u10d) {
  const int nsea = static_cast<int>(ua.extent(0));
  using Policy = Kokkos::RangePolicy<ExecSpace>;

  if (flfrst) {  // :633-673
    Kokkos::parallel_for(
        "ww::updt::w3uwnd_first", Policy(exec, 0, nsea), KOKKOS_LAMBDA(const int i) {
          const int ix = mapsf(i, 0) - 1;  // :643-644
          const int iy = mapsf(i, 1) - 1;
          const UwndFirst o = w3uwnd_first(wx0(ix, iy), wy0(ix, iy), dt0(ix, iy),
                                           wxn(ix, iy), wyn(ix, iy), dtn(ix, iy));
          ua0(i) = o.ua0;
          uai(i) = o.uai;
          ud0(i) = o.ud0;
          udi(i) = o.udi;
          as0(i) = o.as0;
          asi(i) = o.asi;
        });
  }

  const Real rd = w3uwnd_rd(dt0t, dt0n);  // :684, host scalar

  Kokkos::parallel_for(  // :704-731
      "ww::updt::w3uwnd_interp", Policy(exec, 0, nsea), KOKKOS_LAMBDA(const int i) {
        ua(i) = w3uwnd_interp(ua0(i), rd, uai(i));
        ud(i) = w3uwnd_interp(ud0(i), rd, udi(i));
        as(i) = w3uwnd_interp(as0(i), rd, asi(i));
      });

  if (flcur) {  // :746-755
    Kokkos::parallel_for(
        "ww::updt::w3uwnd_u10_rwnd", Policy(exec, 0, nsea), KOKKOS_LAMBDA(const int i) {
          Real a = 0;
          Real d = 0;
          w3uwnd_u10_rwnd(ua(i), ud(i), rwindc, cx(i), cy(i), &a, &d);
          u10(i) = a;
          u10d(i) = d;
        });
  } else {  // :766-769
    Kokkos::parallel_for(
        "ww::updt::w3uwnd_u10", Policy(exec, 0, nsea), KOKKOS_LAMBDA(const int i) {
          u10(i) = w3uwnd_u10(ua(i));
          u10d(i) = ud(i);
        });
  }
}

}  // namespace ww::updt
