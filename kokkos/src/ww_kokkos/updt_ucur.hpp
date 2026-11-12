// kokkos/src/ww_kokkos/updt_ucur.hpp
// W3UCUR, ported to Kokkos: surface current at the model time by linear
// interpolation of speed and direction between the two input fields, stored as
// the components CX, CY. Phase 1, translated, not wired, not regtested.
//
// WW3 heritage: WW3/model/src/w3updtmd.F90:171-472 at h0ffmann/WW3 96892c95
// (byte-identical to NOAA-EMC/WW3 03adda91: git -C WW3 diff 03adda91 HEAD --
// model/src/w3updtmd.F90 is empty); author H. L. Tolman. Called from W3WAVE
// (w3wavemd.F90:1208 at the pin). Evidence: kokkos/port_report/W3UCUR_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Only the switch_lab_shrd branch is ported: CRT1 (RD = DT0T/MAX(1.E-7,DT0N),
// :344). Not compiled there and not ported: CRT0, CRT2 (quasi-quadratic speed),
// OASOCM, SMC (sea-point currents, the Arctic rotation), TIDE, TIDET, S, T.
// DSEC21 (w3timemd.F90) is integer and double time arithmetic on the host: the
// caller computes DT0N = DSEC21(TC0,TCN) and DT0T = DSEC21(TC0,TIME) and passes
// them as the REALs the Fortran stores (:337-338).
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3updtmd.F90 (WW3's own flags, switch_lab_shrd; the recompiled object's
// disassembly equals the CMake build's), 2026-11-12 (v):
// - :311-312 SQRT(x**2+y**2) is (x*x)+(y*y) then sqrtss, stored to CA0/CAI and
//   reloaded for the 1.E-7 tests (same value).
// - :314, :319 ATAN2 is a libm atan2f call, MOD(a,TPI) an inline x87 fprem loop
//   (__builtin_fmodf); :329 SIGN(1.,DD) is copysignf, ABS a mask; :331 CAI-CA0.
//   This first pass is W3UWND's :643-671 with other names, so it uses the same
//   helpers (updt_uwnd.hpp, where each trap is documented).
// - :344 MAX(1.E-7,DT0N) is maxss .LC10(1.E-7),DT0N (xmm0 = DT0N), so a NaN or a
//   tie gives 1.E-7: uwnd_detail::fmax2(1.E-7, DT0N). Then one divss.
// - :436 CABS = CA0 + CAI*RD, :442 CDIR = CD0 + CDI*RD (GIMPLE order of the
//   product; the product commutes bit for bit).
// - :452-453 COS/SIN of CDIR are one sincosf call (__builtin_cexpif), then
//   CABS*COS, CABS*SIN.
// - Operand positions (the first source is the NaN a two-NaN product returns):
//   in the kernel the :436 product has RD first where the Fortran has CAI first,
//   and the :452-453 products CABS first where the Fortran has COS/SIN first
//   (g++ folds the memory operand into the second slot; swapping the C++ operands
//   does not change it). Both operands are never NaN together: DSEC21 is
//   REAL(NS)+86400.*REAL(ND) of integers (w3timemd.F90:330-568), so RD is finite,
//   and CD0, CDI are finite (a NaN ATAN2 needs a NaN component, which fails the
//   1.E-7 test), so CDIR and its COS/SIN are finite (v, read from the source and
//   both objects). The :311 sum and :442 product have the Fortran's positions.
// - No loop of :171-472 is vectorised (vec.txt has no line there); no FMA.
//
// On a GPU atan2f, fmodf and sincosf are the device copies of libm_device.hpp
// (atan2f, sincosf) and Kokkos::fmod: their bits are not shown to match the
// Fortran's (AGENTS.md, "Bit for bit") ⚠.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "updt_uwnd.hpp"

namespace ww::updt {

/// The FLFRST outputs of one sea point.
struct UcurFirst {
  Real ca0, cai, cd0, cdi;
};

/// :303-332 for one ISEA, from CX0..CYN at (IX,IY) = MAPSF(ISEA,1:2).
KOKKOS_INLINE_FUNCTION UcurFirst w3ucur_first(Real cx0, Real cy0, Real cxn, Real cyn) {
  using uwnd_detail::atan2_r;
  using uwnd_detail::mod_tpi;
  UcurFirst o{};
  o.ca0 = Kokkos::sqrt(cx0 * cx0 + cy0 * cy0);  // :311
  const Real cai = Kokkos::sqrt(cxn * cxn + cyn * cyn);  // :312
  Real d0;
  if (o.ca0 > static_cast<Real>(1.E-7)) {  // :313-317
    d0 = mod_tpi(kUwndTpi + atan2_r(cy0, cx0));
  } else {
    d0 = 0;
  }
  Real dn;
  if (cai > static_cast<Real>(1.E-7)) {  // :318-322
    dn = mod_tpi(kUwndTpi + atan2_r(cyn, cxn));
  } else {
    dn = d0;
  }
  o.cd0 = (o.ca0 > static_cast<Real>(1.E-7)) ? d0 : dn;  // :323-327
  Real dd = dn - o.cd0;  // :328
  if (Kokkos::fabs(dd) > kUwndPi) {  // :329
    dd = dd - kUwndTpi * Kokkos::copysign(static_cast<Real>(1.), dd);
  }
  o.cdi = dd;  // :330
  o.cai = cai - o.ca0;  // :331
  return o;
}

/// :344, CRT1. MAX(1.E-7,DT0N) as the build's maxss 1.E-7,DT0N.
KOKKOS_INLINE_FUNCTION Real w3ucur_rd(Real dt0t, Real dt0n) {
  return dt0t / uwnd_detail::fmax2(static_cast<Real>(1.E-7), dt0n);
}

/// :436-453 for one ISEA: CX and CY from the interpolated speed and direction.
KOKKOS_INLINE_FUNCTION void w3ucur_point(Real ca0, Real cai, Real cd0, Real cdi, Real rd,
                                         Real* cx, Real* cy) {
  const Real cabs = ca0 + cai * rd;  // :436
  const Real cdir = cd0 + cdi * rd;  // :442
  Real s = 0;
  Real c = 0;
  uwnd_detail::sincos_r(cdir, &s, &c);
  *cx = cabs * c;  // :452 (Fortran COS*CABS; see the operand note above)
  *cy = cabs * s;  // :453
}

/// W3UCUR(FLFRST) under switch_lab_shrd. Module state comes in as Views:
///   mapsf             MAPSF(MSEA,3), w3gdatmd.F90:1736, LayoutLeft, Fortran 1-based IX, IY
///   cx0, cy0, cxn, cyn CX0(NX,NY) ..., w3idatmd.F90:641-644 (non-SMC), LayoutLeft
///   ca0, cai, cd0, cdi CA0(NSEA) ..., w3adatmd.F90:1335-1338, element ISEA-1
///   cx, cy            CX(1:NSEA), CY(1:NSEA) of the 0:NSEA arrays (w3adatmd.F90:1008),
///                     element ISEA-1; extent NSEA
/// dt0n, dt0t are DSEC21's results as REAL (:337-338).
///
/// Why parallel gives the serial bits: in both loops point ISEA reads only inputs
/// and its own entries and writes only its own entries; no reduction, no sum
/// across points. The loops run in the Fortran's order on one execution space
/// instance (in order).
template <class ExecSpace>
void w3ucur(const ExecSpace& exec, bool flfrst, Real dt0n, Real dt0t,
            Kokkos::View<const int**, Kokkos::LayoutLeft, ExecSpace> mapsf,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> cx0,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> cy0,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> cxn,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> cyn,
            Kokkos::View<Real*, ExecSpace> ca0, Kokkos::View<Real*, ExecSpace> cai,
            Kokkos::View<Real*, ExecSpace> cd0, Kokkos::View<Real*, ExecSpace> cdi,
            Kokkos::View<Real*, ExecSpace> cx, Kokkos::View<Real*, ExecSpace> cy) {
  const int nsea = static_cast<int>(cx.extent(0));
  using Policy = Kokkos::RangePolicy<ExecSpace>;

  if (flfrst) {  // :295-333
    Kokkos::parallel_for(
        "ww::updt::w3ucur_first", Policy(exec, 0, nsea), KOKKOS_LAMBDA(const int i) {
          const int ix = mapsf(i, 0) - 1;  // :305-306
          const int iy = mapsf(i, 1) - 1;
          const UcurFirst o = w3ucur_first(cx0(ix, iy), cy0(ix, iy), cxn(ix, iy), cyn(ix, iy));
          ca0(i) = o.ca0;
          cai(i) = o.cai;
          cd0(i) = o.cd0;
          cdi(i) = o.cdi;
        });
  }

  const Real rd = w3ucur_rd(dt0t, dt0n);  // :344, host scalar

  Kokkos::parallel_for(  // :383-458
      "ww::updt::w3ucur", Policy(exec, 0, nsea), KOKKOS_LAMBDA(const int i) {
        Real x = 0;
        Real y = 0;
        w3ucur_point(ca0(i), cai(i), cd0(i), cdi(i), rd, &x, &y);
        cx(i) = x;
        cy(i) = y;
      });
}

}  // namespace ww::updt
